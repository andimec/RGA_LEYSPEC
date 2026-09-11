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
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace leyrga {

class Driver : public asynPortDriver {
public:
    Driver(const char* portName, const char* ioPort, int address,
           const char* trendMassesCsv);
    ~Driver() override;

    asynStatus writeInt32(asynUser* pasynUser, epicsInt32 value) override;
    asynStatus readFloat64Array(asynUser* pasynUser, epicsFloat64* value,
                                size_t nElements, size_t* nIn) override;

private:
    static void pollTaskC(void* arg);
    void pollTask();

    bool transact(const std::string& command, const std::string& parameter,
                  Response& response, double timeout = 1.0);
    bool commandAck(const std::string& command, const std::string& parameter = "");
    bool reconnect();
    bool initialise();
    bool configureMode(int mode);
    bool configureTrendMasses();
    bool configureMassRange();
    void updateStatus();
    void updateData();
    void setConnected(bool connected);
    void setCommError(const std::string& message);
    void publishData(const DdData& data);

    int address_;
    std::string ioPortName_;
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
    std::mutex transactionMutex_;

    int P_Connected;
    int P_Error;
    int P_ErrorCode;
    int P_ErrorRaw;
    int P_Status;
    int P_Version;
    int P_Model;
    int P_MassRange;
    int P_Filament;
    int P_FilamentCmd;
    int P_FilamentSelect;
    int P_Detector;
    int P_DetectorCmd;
    int P_Mode;
    int P_ModeCmd;
    int P_MassFirst;
    int P_MassFirstSet;
    int P_MassLast;
    int P_MassLastSet;
    int P_SpeedSet;
    int P_MeasureStart;
    int P_MeasureStop;
    int P_Measuring;
    int P_TotalPressure;
    int P_TpSetStatus;
    int P_Analog1;
    int P_Analog2;
    int P_Spectrum;
    int P_MassAxis;
    int P_CommCount;
    int P_CommError;
};

static std::vector<double> parseCsv(const char* csv)
{
    std::vector<double> values;
    if (!csv) return values;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        char* end = nullptr;
        const double value = std::strtod(token.c_str(), &end);
        if (end != token.c_str() && *end == '\0' && value >= 0.0 && value <= 300.0)
            values.push_back(value);
    }
    return values;
}

Driver::Driver(const char* portName, const char* ioPort, int address,
               const char* trendMassesCsv)
    : asynPortDriver(portName, 1, 31,
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynFloat64ArrayMask,
                     asynInt32Mask | asynFloat64Mask | asynOctetMask | asynFloat64ArrayMask,
                     0, 1, 0, 0),
      address_(address), ioPortName_(ioPort ? ioPort : ""), ioUser_(nullptr),
      stopThread_(false), initialised_(false), pollCounter_(0),
      massFirst_(1), massLast_(50), mode_(0), speed_(3),
      commCount_(0), commErrors_(0)
{
    createParam("CONNECTED", asynParamInt32, &P_Connected);
    createParam("ERROR", asynParamInt32, &P_Error);
    createParam("ERROR:CODE", asynParamInt32, &P_ErrorCode);
    createParam("ERROR:RAW", asynParamOctet, &P_ErrorRaw);
    createParam("STATUS", asynParamOctet, &P_Status);
    createParam("VERSION", asynParamOctet, &P_Version);
    createParam("MODEL", asynParamOctet, &P_Model);
    createParam("MASS:RANGE", asynParamInt32, &P_MassRange);
    createParam("FILAMENT", asynParamInt32, &P_Filament);
    createParam("FILAMENT:CMD", asynParamInt32, &P_FilamentCmd);
    createParam("FILAMENT:SELECT", asynParamInt32, &P_FilamentSelect);
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
    createParam("PRESSURE:SETPOINT:STATUS", asynParamInt32, &P_TpSetStatus);
    createParam("ANALOG:INPUT1", asynParamFloat64, &P_Analog1);
    createParam("ANALOG:INPUT2", asynParamFloat64, &P_Analog2);
    createParam("SPECTRUM", asynParamFloat64Array, &P_Spectrum);
    createParam("MASS:AXIS", asynParamFloat64Array, &P_MassAxis);
    createParam("COMM:COUNT", asynParamInt32, &P_CommCount);
    createParam("COMM:ERROR", asynParamInt32, &P_CommError);

    trendMasses_ = parseCsv(trendMassesCsv);
    if (trendMasses_.empty()) {
        const double defaults[] = {2, 18, 28, 32, 40, 44};
        trendMasses_.assign(defaults, defaults + 6);
    }
    while (trendMasses_.size() < 20) trendMasses_.push_back(0.0);
    if (trendMasses_.size() > 20) trendMasses_.resize(20);

    spectrum_.reserve(300);
    massAxis_.reserve(300);

    setIntegerParam(P_Connected, 0);
    setIntegerParam(P_Error, 0);
    setIntegerParam(P_ErrorCode, 0);
    setIntegerParam(P_Filament, 0);
    setIntegerParam(P_FilamentCmd, 0);
    setIntegerParam(P_FilamentSelect, 1);
    setIntegerParam(P_Detector, 0);
    setIntegerParam(P_DetectorCmd, 0);
    setIntegerParam(P_Mode, mode_);
    setIntegerParam(P_ModeCmd, mode_);
    setIntegerParam(P_MassFirst, massFirst_);
    setIntegerParam(P_MassFirstSet, massFirst_);
    setIntegerParam(P_MassLast, massLast_);
    setIntegerParam(P_MassLastSet, massLast_);
    setIntegerParam(P_SpeedSet, speed_);
    setIntegerParam(P_Measuring, 0);
    setIntegerParam(P_MassRange, 200);
    setIntegerParam(P_CommCount, 0);
    setIntegerParam(P_CommError, 0);
    callParamCallbacks();

    if (pasynOctetSyncIO->connect(ioPortName_.c_str(), 0, &ioUser_, nullptr) != asynSuccess)
        setCommError("Unable to connect to asyn TCP port");

    epicsThreadCreate("leyrgaPoll", epicsThreadPriorityMedium,
                      epicsThreadGetStackSize(epicsThreadStackMedium), pollTaskC, this);
}

Driver::~Driver()
{
    stopThread_ = true;
    if (ioUser_) {
        pasynOctetSyncIO->disconnect(ioUser_);
        ioUser_ = nullptr;
    }
}

void Driver::pollTaskC(void* arg)
{
    static_cast<Driver*>(arg)->pollTask();
}

void Driver::setConnected(bool connected)
{
    setIntegerParam(P_Connected, connected ? 1 : 0);
    if (!connected) {
        initialised_ = false;
        setIntegerParam(P_Measuring, 0);
    }
}

void Driver::setCommError(const std::string& message)
{
    ++commErrors_;
    setIntegerParam(P_CommError, static_cast<int>(commErrors_));
    setIntegerParam(P_Error, 1);
    setStringParam(P_ErrorRaw, message.c_str());
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "leyrga: %s\n", message.c_str());
}

bool Driver::reconnect()
{
    std::lock_guard<std::mutex> lock(transactionMutex_);
    if (ioUser_) {
        pasynOctetSyncIO->disconnect(ioUser_);
        ioUser_ = nullptr;
    }
    if (pasynOctetSyncIO->connect(ioPortName_.c_str(), 0, &ioUser_, nullptr) != asynSuccess) {
        setCommError("TCP reconnect failed");
        return false;
    }
    initialised_ = false;
    return true;
}

bool Driver::transact(const std::string& command, const std::string& parameter,
                      Response& response, double timeout)
{
    std::lock_guard<std::mutex> lock(transactionMutex_);
    if (!ioUser_) return false;

    const std::string tx = Protocol::makeCommand(address_, command, parameter);
    char rx[16384];
    size_t nwrite = 0;
    size_t nread = 0;
    int eom = 0;
    ++commCount_;

    const asynStatus status = pasynOctetSyncIO->writeRead(
        ioUser_, tx.c_str(), tx.size(), rx, sizeof(rx) - 1, timeout,
        &nwrite, &nread, &eom);

    setIntegerParam(P_CommCount, static_cast<int>(commCount_));
    if (status != asynSuccess) {
        setCommError("TCP write/read failed");
        return false;
    }

    rx[nread] = '\0';
    if (!Protocol::parseResponse(std::string(rx, nread), response)) {
        setCommError("Invalid Leybold response: " + response.error);
        return false;
    }

    if (response.address != address_) {
        setCommError("Response address does not match configured RGA address");
        return false;
    }

    if (!response.ack && response.command.size() == 1 &&
        static_cast<unsigned char>(response.command[0]) == 0x15) {
        setIntegerParam(P_ErrorCode, response.errorCode);
        setIntegerParam(P_Error, 1);
        return false;
    }
    return true;
}

bool Driver::commandAck(const std::string& command, const std::string& parameter)
{
    Response response;
    return transact(command, parameter, response) && response.ack;
}

bool Driver::configureTrendMasses()
{
    for (std::size_t i = 0; i < trendMasses_.size(); ++i) {
        char channel[4];
        char mass[4];
        std::snprintf(channel, sizeof(channel), "%02d", static_cast<int>(i));
        std::snprintf(mass, sizeof(mass), "%03d", static_cast<int>(trendMasses_[i]));
        if (!commandAck("MC", channel) || !commandAck("MM", mass)) return false;
    }
    return true;
}

bool Driver::configureMode(int mode)
{
    if (mode < 0 || mode > 2) return false;
    const char* commands[] = {"G0", "G1", "G2"};
    if (!commandAck(commands[mode])) return false;
    if (mode == 1 && !configureTrendMasses()) return false;
    if (mode != 1 && !configureMassRange()) return false;
    return true;
}

bool Driver::configureMassRange()
{
    char first[4];
    char last[4];
    std::snprintf(first, sizeof(first), "%03d", massFirst_);
    std::snprintf(last, sizeof(last), "%03d", massLast_);
    return commandAck("FM", first) && commandAck("LM", last) &&
           commandAck("SD", std::to_string(speed_));
}

bool Driver::initialise()
{
    if (!commandAck("CN")) return false;
    if (!commandAck("CM", "2")) return false;
    if (!commandAck("UC", "2")) return false;
    if (!configureMode(mode_)) return false;
    if (!configureMassRange()) return false;

    Response r;
    if (transact("CS", "", r) && r.command == "CS" && r.parameter.size() >= 5) {
        const int massRange = (r.parameter[1] - '0') * 100;
        setIntegerParam(P_MassRange, massRange);
        setStringParam(P_Model, "LEYSPEC view series");
    }

    initialised_ = true;
    setConnected(true);
    setIntegerParam(P_Error, 0);
    setIntegerParam(P_ErrorCode, 0);
    return true;
}

void Driver::updateStatus()
{
    Response r;
    if (transact("DS", "", r) && r.command == "DS" && r.parameter.size() >= 12) {
        const int error = r.parameter[1] == '1' ? 1 : 0;
        const int filament = r.parameter[3] == '1' ? 1 : (r.parameter[3] == '2' ? 2 : 0);
        const int detector = r.parameter[9] == '1' ? 1 : 0;
        const int measuring = r.parameter[11] == '0' ? 1 : 0;
        setIntegerParam(P_Filament, filament);
        setIntegerParam(P_Detector, detector);
        setIntegerParam(P_Measuring, measuring);
        setIntegerParam(P_Error, error);
        setStringParam(P_Status, r.parameter.c_str());
    }

    if (transact("ES", "", r) && r.command == "ES") {
        int error = 0;
        if (r.parameter.size() >= 2) {
            for (std::size_t i = 2; i < r.parameter.size(); ++i)
                if (r.parameter[i] == '1') error = 1;
        }
        setIntegerParam(P_Error, error);
        setStringParam(P_ErrorRaw, r.parameter.c_str());
    }

    if (transact("VR", "", r) && r.command == "VR")
        setStringParam(P_Version, r.parameter.c_str());
}

void Driver::publishData(const DdData& data)
{
    spectrum_ = data.values;
    massAxis_ = data.axis;
    setDoubleParam(P_TotalPressure, data.totalPressure);
    setDoubleParam(P_Analog1, data.analog1);
    setDoubleParam(P_Analog2, data.analog2);
    setIntegerParam(P_TpSetStatus, data.tpSet);
    if (data.errorFlag) setIntegerParam(P_Error, 1);

    if (!spectrum_.empty())
        doCallbacksFloat64Array(spectrum_.data(), spectrum_.size(), P_Spectrum, 0);
    if (!massAxis_.empty())
        doCallbacksFloat64Array(massAxis_.data(), massAxis_.size(), P_MassAxis, 0);
}

void Driver::updateData()
{
    Response r;
    if (!transact("DD", "", r)) return;

    DdData data;
    if (!Protocol::parseDd(r, mode_, massFirst_, massLast_, trendMasses_, data)) {
        setCommError("Unable to parse DD measurement data");
        return;
    }
    publishData(data);
}

void Driver::pollTask()
{
    while (!stopThread_) {
        if (!ioUser_) {
            epicsThreadSleep(1.0);
            continue;
        }

        if (!initialised_) {
            if (!initialise()) {
                setConnected(false);
                epicsThreadSleep(2.0);
                reconnect();
                continue;
            }
        }

        ++pollCounter_;
        if ((pollCounter_ % 10) == 0) updateStatus();

        int measuring = 0;
        getIntegerParam(P_Measuring, &measuring);
        if (measuring) updateData();

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
    } else if (function == P_FilamentSelect) {
        if (value != 1 && value != 2) status = asynError;
        else status = commandAck(value == 1 ? "FA" : "FB") ? asynSuccess : asynError;
    } else if (function == P_DetectorCmd) {
        status = commandAck(value ? "SF" : "SS") ? asynSuccess : asynError;
    } else if (function == P_ModeCmd) {
        status = configureMode(value) ? asynSuccess : asynError;
        if (status == asynSuccess) mode_ = value;
    } else if (function == P_MassFirstSet) {
        if (value < 1 || value >= massLast_ || value > 300) status = asynError;
        else {
            char p[4]; std::snprintf(p, sizeof(p), "%03d", value);
            status = commandAck("FM", p) ? asynSuccess : asynError;
            if (status == asynSuccess) massFirst_ = value;
        }
    } else if (function == P_MassLastSet) {
        if (value <= massFirst_ || value > 300) status = asynError;
        else {
            char p[4]; std::snprintf(p, sizeof(p), "%03d", value);
            status = commandAck("LM", p) ? asynSuccess : asynError;
            if (status == asynSuccess) massLast_ = value;
        }
    } else if (function == P_SpeedSet) {
        if (value < 1 || value > 6) status = asynError;
        else {
            status = commandAck("SD", std::to_string(value)) ? asynSuccess : asynError;
            if (status == asynSuccess) speed_ = value;
        }
    } else if (function == P_MeasureStart) {
        if (value) status = commandAck("ST") ? asynSuccess : asynError;
    } else if (function == P_MeasureStop) {
        if (value) status = commandAck("SP") ? asynSuccess : asynError;
    } else {
        status = asynPortDriver::writeInt32(pasynUser, value);
    }

    if (status == asynSuccess) {
        setIntegerParam(function, value);
        if (function == P_MassFirstSet) setIntegerParam(P_MassFirst, massFirst_);
        if (function == P_MassLastSet) setIntegerParam(P_MassLast, massLast_);
        if (function == P_ModeCmd) setIntegerParam(P_Mode, mode_);
        callParamCallbacks();
    }
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
    new leyrga::Driver(args[0].sval, args[1].sval, args[2].ival, args[3].sval);
}

static const iocshArg arg0 = {"Port name", iocshArgString};
static const iocshArg arg1 = {"asyn TCP port", iocshArgString};
static const iocshArg arg2 = {"RGA address", iocshArgInt};
static const iocshArg arg3 = {"Trend masses CSV", iocshArgString};
static const iocshArg* const args[] = {&arg0, &arg1, &arg2, &arg3};
static const iocshFuncDef funcDef = {"leyrgaConfigure", 4, args};

static void leyrgaRegister(void)
{
    iocshRegister(&funcDef, leyrgaConfigureCall);
}

extern "C" { epicsExportRegistrar(leyrgaRegister); }
