#ifndef LEYBOLD_PROTOCOL_H
#define LEYBOLD_PROTOCOL_H

#include <string>
#include <vector>

namespace leyrga {

struct Response {
    bool valid = false;
    bool ack = false;
    int errorCode = 0;
    int address = 0;
    std::string command;
    std::string parameter;
    std::string raw;
    std::string error;
};

struct DdData {
    bool valid = false;
    std::vector<double> values;
    std::vector<double> axis;
    double totalPressure = 0.0;
    double analog1 = 0.0;
    double analog2 = 0.0;
    int errorFlag = 0;
    int tpSet = 0;
};

class Protocol {
public:
    static std::string makeCommand(int address, const std::string& command,
                                   const std::string& parameter = "");
    static unsigned int checksum(const std::string& payload);
    static std::string checksumString(const std::string& payload);
    static bool parseResponse(const std::string& frame, Response& response);
    static bool parseScientific(const std::string& value, double& result);
    static bool parseDd(const Response& response, int mode, int firstMass, int lastMass,
                        const std::vector<double>& trendMasses, DdData& data);
};

} // namespace leyrga

#endif
