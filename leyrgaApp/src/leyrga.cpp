#include "LeyboldProtocol.h"

#include <asynPortDriver.h>
#include <asynOctetSyncIO.h>
#include <drvAsynIPPort.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <iocsh.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

namespace leyrga {

class Driver : public asynPortDriver {
public:
    Driver(const char* portName, const char* ioPort, int address);
    virtual ~Driver();
    asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value) override;
    asynStatus readFloat64Array(asynUser* pasynUser, epicsFloat64* value,
                                size_t nElements, size_t* nIn) override;

private:
    static void pollTaskC(void* arg);
    void pollTask();
    bool transact(const std::string& command, const std::string& parameter,
                  Response& response, double timeout = 1.0);
    bool commandAck(const std::string& command, const std::string& parameter = "");
    bool initialise();
    void updateStatus();
    void updateData();
    void setConnected(bool connected);
    void setCommError(const std::string& message);
    void publishSpectrum(const std::vector<double>& spectrum);

    int address_;
    asynUser* ioUser_;
    std::atomic<bool> stopThread_;
    bool initialised_;
    int pollCounter_;
    int massFirst_;
    int massLast_;
    int mode_;
    int speed_;
    std::vector<double> trendMasses_;
    std::vector<double> spectrum_;
    std::vector<double> massAxis_;
    epicsUInt32 commCount_;
    epicsUInt32 commErrors_;

    int P_Connected, P_Error, P_Status, P_Version;
    int P_Filament, P_FilamentCmd, P_Detector, P_DetectorCmd;
    int P_Mode, P_ModeCmd, P_MassFirst, P_MassFirstSet, P_MassLast, P_MassLastSet;
    int P_SpeedSet, P_MeasureStart, P_MeasureStop, P_Measuring;
    int P_TotalPressure, P_Spectrum, P_MassAxis, P_CommCount, P_CommError;
};

Driver::Driver(const char* portName, const char* ioPort, int address)
    : asynPortDriver(portName, 1, 23,
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynFloat64ArrayMask,
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynFloat64ArrayMask,
                     0, 1, 0, 0),
      address_(address), ioUser_(nullptr), stopThread_(false), initialised_(false),
      pollCounter_(0), massFirst_(1), massLast_(200), mode_(0), speed_(3),
      commCount_(0), commErrors_(0)
{
    createParam("CONNECTED", asynParamInt32, &P_Connected);
    createParam("ERROR", asynParamInt32, &P_Error);
    createParam("STATUS", asynParamOctet, &P_Status);
    createParam("VERSION", asynParamOctet, &P_Version);
    createParam("FILAMENT", asynParamInt32, &P_Filament);
    createParam("FILAMENT:CMD", asynParamInt32, &P_FilamentCmd);
    createParam("DETECTOR", asynParamInt32, &P_Detector);
    createParam("DETECTOR:CMD", asynParamInt32, &P_DetectorCmd);
    createParam("MODE", asynParamInt32, &P_Mode);
    createParam("MODE:CMD", asynParamInt32, &P_ModeCmd);
    createParam("MASS:FIRST", asynParamInt32, &P_MassFirst);
    createParam("MASS:FIRST:SET", asynParamInt32, &P_MassFirstSet);
    createParam("MASS:LAST", asynParamInt32, &P_MassLast);
    createParam("MASS:LAST:SET", asynParamInt32, &P_MassLastSet);
    createParam("SPEED:SET", asynParamInt32, &P_SpeedSet);
    createParam("MEASUREMENT:START", asynParamInt32, &P_MeasureStart);
    createParam("MEASUREMENT:STOP", asynParamInt32, &P_MeasureStop);
    createParam("MEASURING", asynParamInt32, &P_Measuring);
    createParam("PRESSURE:TOTAL", asynParamFloat64, &P_TotalPressure);
    createParam("SPECTRUM", asynParamFloat64Array, &P_Spectrum);
    createParam("MASS:AXIS", asynParamFloat64Array, &P_MassAxis);
    createParam("COMM:COUNT", asynParamInt32, &P_CommCount);
    createParam("COMM:ERROR", asynParamInt32, &P_CommError);

    trendMasses_.assign(20, 0.0);
    trendMasses_[0] = 2; trendMasses_[1] = 18; trendMasses_[2] = 28;
    trendMasses_[3] = 32; trendMasses_[4] = 40; trendMasses_[5] = 44;

    if (pasynOctetSyncIO->connect(ioPort, 0, &ioUser_, nullptr) != asynSuccess)
        setCommError("Unable to connect to asyn TCP port");

    setIntegerParam(P_Connected, 0);
    setIntegerParam(P_Error, 0);
    setIntegerParam(P_Filament, 0);
    setIntegerParam(P_Detector, 0);
    setIntegerParam(P_Mode, 0);
    setIntegerParam(P_MassFirst, massFirst_);
    setIntegerParam(P_MassLast, massLast_);
    setIntegerParam(P_MassFirstSet, massFirst_);
    setIntegerParam(P_MassLastSet, massLast_);
    setIntegerParam(P_SpeedSet, speed_);
    setIntegerParam(P_Measuring, 0);
    setIntegerParam(P_CommCount, 0);
    setIntegerParam(P_CommError, 0);
    callParamCallbacks();

    epicsThreadCreate("leyrgaPoll", epicsThreadPriorityMedium,
                      epicsThreadGetStackSize(epicsThreadStackMedium), pollTaskC, this);
}

Driver::~Driver()
{
    stopThread_ = true;
    if (ioUser_) pasynOctetSyncIO->disconnect(ioUser_);
}

void Driver::pollTaskC(void* arg) { static_cast<Driver*>(arg)->pollTask(); }

void Driver::setConnected(bool connected)
{
    setIntegerParam(P_Connected, connected ? 1 : 0);
    if (!connected) setIntegerParam(P_Measuring, 0);
}

void Driver::setCommError(const std::string& message)
{
    ++commErrors_;
    setIntegerParam(P_Error, 1);
    setIntegerParam(P_CommError, static_cast<int>(commErrors_));
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "leyrga: %s\n", message.c_str());
}

bool Driver::transact(const std::string& command, const std::string& parameter,
                      Response& response, double timeout)
{
    if (!ioUser_) return false;
    std::string tx = Protocol::makeCommand(address_, command, parameter);
    char rx[8192]; size_t nwrite = 0, nread = 0; int eom = 0;
    ++commCount_;
    asynStatus status = pasynOctetSyncIO->writeRead(ioUser_, tx.c_str(), tx.size(),
                                                     rx, sizeof(rx)-1, timeout,
                                                     &nwrite, &nread, &eom);
    if (status != asynSuccess) {
        setCommError("TCP write/read failed");
        setIntegerParam(P_CommCount, static_cast<int>(commCount_));
        return false;
    }
    rx[nread] = '\0';
    if (!Protocol::parseResponse(std::string(rx, nread), response)) {
        setCommError("Invalid Leybold response: " + response.error);
        setIntegerParam(P_CommCount, static_cast<int>(commCount_));
        return false;
    }
    setIntegerParam(P_CommCount, static_cast<int>(commCount_));
    if (!response.ack && response.command.size() == 1 &&
        static_cast<unsigned char>(response.command[0]) == 0x15) {
        setIntegerParam(P_Error, response.errorCode);
        return false;
    }
    return true;
}

bool Driver::commandAck(const std::string& command, const std::string& parameter)
{
    Response response;
    return transact(command, parameter, response) && response.ack;
}

bool Driver::initialise()
{
    if (!commandAck("CN")) return false;
    if (!commandAck("CM", "2")) return false;
    initialised_ = true;
    setConnected(true);
    return true;
}

void Driver::updateStatus()
{
    Response r;
    if (transact("DS", "", r) && r.command == "DS" && r.parameter.size() >= 12) {
        int filament = (r.parameter[3] == '1') ? 1 : (r.parameter[3] == '2' ? 2 : 0);
        int detector = (r.parameter[9] == '0') ? 0 : 1;
        int measuring = (r.parameter[11] == '0') ? 1 : 0;
        setIntegerParam(P_Filament, filament);
        setIntegerParam(P_Detector, detector);
        setIntegerParam(P_Measuring, measuring);
        setStringParam(P_Status, r.parameter.c_str());
    }
    if (transact("ES", "", r) && r.command == "ES") {
        int error = 0; for (char c : r.parameter) if (c == '1') error = 1;
        setIntegerParam(P_Error, error);
    }
    if (transact("VR", "", r) && r.command == "VR")
        setStringParam(P_Version, r.parameter.c_str());
}

void Driver::publishSpectrum(const std::vector<double>& spectrum)
{
    spectrum_ = spectrum;
    massAxis_.clear();
    if (mode_ == 0) {
        for (int m = massFirst_; m <= massLast_ && massAxis_.size() < spectrum.size(); ++m)
            massAxis_.push_back(static_cast<double>(m));
    } else if (mode_ == 1) {
        for (size_t i = 0; i < spectrum.size() && i < trendMasses_.size(); ++i)
            massAxis_.push_back(trendMasses_[i]);
    }
    doCallbacksFloat64Array(spectrum_.data(), spectrum_.size(), P_Spectrum, 0);
    doCallbacksFloat64Array(massAxis_.data(), massAxis_.size(), P_MassAxis, 0);
}

void Driver::updateData()
{
    Response r;
    if (!transact("DD", "", r)) return;
    std::vector<double> spectrum; double totalPressure = 0.0; int errorFlag = 0, tpSet = 0;
    if (!Protocol::parseDd(r, spectrum, totalPressure, errorFlag, tpSet)) {
        setCommError("Unable to parse DD measurement data"); return;
    }
    setDoubleParam(P_TotalPressure, totalPressure);
    if (errorFlag) setIntegerParam(P_Error, 1);
    publishSpectrum(spectrum);
}

void Driver::pollTask()
{
    while (!stopThread_) {
        if (!ioUser_) { epicsThreadSleep(1.0); continue; }
        if (!initialised_) {
            if (!initialise()) { setConnected(false); epicsThreadSleep(2.0); continue; }
        }
        ++pollCounter_;
        if ((pollCounter_ % 10) == 0) updateStatus();
        updateData();
        callParamCallbacks();
        epicsThreadSleep(0.5);
    }
}

asynStatus Driver::writeInt32(asynUser* pasynUser, epicsInt32 value)
{
    const int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    if (function == P_FilamentCmd) {
        status = commandAck(value ? "F1" : "F0") ? asynSuccess : asynError;
    } else if (function == P_DetectorCmd) {
        status = commandAck(value ? "SF" : "SS") ? asynSuccess : asynError;
    } else if (function == P_ModeCmd) {
        const char* commands[] = {"G0", "G1", "G2"};
        if (value < 0 || value > 2) status = asynError;
        else { status = commandAck(commands[value]) ? asynSuccess : asynError; if (status == asynSuccess) mode_ = value; }
    } else if (function == P_MassFirstSet) {
        if (value < 1 || value >= massLast_) status = asynError;
        else { char p[4]; std::snprintf(p, sizeof(p), "%03d", value); status = commandAck("FM", p) ? asynSuccess : asynError; if (status == asynSuccess) massFirst_ = value; }
    } else if (function == P_MassLastSet) {
        if (value <= massFirst_ || value > 300) status = asynError;
        else { char p[4]; std::snprintf(p, sizeof(p), "%03d", value); status = commandAck("LM", p) ? asynSuccess : asynError; if (status == asynSuccess) massLast_ = value; }
    } else if (function == P_SpeedSet) {
        if (value < 1 || value > 6) status = asynError;
        else { status = commandAck("SD", std::to_string(value)) ? asynSuccess : asynError; if (status == asynSuccess) speed_ = value; }
    } else if (function == P_MeasureStart) {
        if (value) status = commandAck("ST") ? asynSuccess : asynError;
    } else if (function == P_MeasureStop) {
        if (value) status = commandAck("SP") ? asynSuccess : asynError;
    } else {
        status = asynPortDriver::writeInt32(pasynUser, value);
    }
    if (status == asynSuccess) { setIntegerParam(function, value); callParamCallbacks(); }
    return status;
}

asynStatus Driver::readFloat64Array(asynUser* pasynUser, epicsFloat64* value,
                                    size_t nElements, size_t* nIn)
{
    const int function = pasynUser->reason;
    const std::vector<double>* source = nullptr;
    if (function == P_Spectrum) source = &spectrum_;
    else if (function == P_MassAxis) source = &massAxis_;
    if (!source) return asynPortDriver::readFloat64Array(pasynUser, value, nElements, nIn);
    const size_t n = std::min(nElements, source->size());
    std::copy(source->begin(), source->begin() + n, value);
    if (nIn) *nIn = n;
    return asynSuccess;
}

} // namespace leyrga

static void leyrgaConfigureCall(const iocshArgBuf* args)
{
    new leyrga::Driver(args[0].sval, args[1].sval, args[2].ival);
}

static const iocshArg arg0 = {"Port name", iocshArgString};
static const iocshArg arg1 = {"asyn TCP port", iocshArgString};
static const iocshArg arg2 = {"RGA address", iocshArgInt};
static const iocshArg* const args[] = {&arg0, &arg1, &arg2};
static const iocshFuncDef funcDef = {"leyrgaConfigure", 3, args};

static void leyrgaRegister(void) { iocshRegister(&funcDef, leyrgaConfigureCall); }
extern "C" { epicsExportRegistrar(leyrgaRegister); }
