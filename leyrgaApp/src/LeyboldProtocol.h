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

class Protocol {
public:
    static std::string makeCommand(int address, const std::string& command,
                                   const std::string& parameter = "");
    static unsigned int checksum(const std::string& payload);
    static std::string checksumString(const std::string& payload);
    static bool parseResponse(const std::string& frame, Response& response);
    static bool parseScientific(const std::string& value, double& result);
    static bool parseDd(const Response& response, std::vector<double>& spectrum,
                        double& totalPressure, int& errorFlag, int& tpSet);
};

} // namespace leyrga

#endif
