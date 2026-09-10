#include "LeyboldProtocol.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace leyrga {

static std::string trim(const std::string& in)
{
    std::size_t first = 0;
    while (first < in.size() && std::isspace(static_cast<unsigned char>(in[first]))) ++first;
    std::size_t last = in.size();
    while (last > first && std::isspace(static_cast<unsigned char>(in[last - 1]))) --last;
    return in.substr(first, last - first);
}

std::string Protocol::makeCommand(int address, const std::string& command,
                                  const std::string& parameter)
{
    std::ostringstream os;
    os << '#' << std::setw(2) << std::setfill('0') << address
       << command << parameter << "\r\n";
    return os.str();
}

unsigned int Protocol::checksum(const std::string& payload)
{
    unsigned int sum = 0;
    for (unsigned char c : payload) sum = (sum + c) & 0xFFFFu;
    return sum;
}

std::string Protocol::checksumString(const std::string& payload)
{
    std::ostringstream os;
    os << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
       << checksum(payload);
    return os.str();
}

bool Protocol::parseResponse(const std::string& frame, Response& response)
{
    response = Response();
    response.raw = frame;

    std::string data = frame;
    while (!data.empty() && (data.back() == '\r' || data.back() == '\n')) data.pop_back();
    if (data.size() < 8 || data[0] != '#') {
        response.error = "Invalid response frame";
        return false;
    }

    response.address = std::atoi(data.substr(1, 2).c_str());
    std::string body = data.substr(0, data.size() - 4);
    std::string suppliedChecksum = data.substr(data.size() - 4);
    bool checksumHex = suppliedChecksum.size() == 4 &&
        std::all_of(suppliedChecksum.begin(), suppliedChecksum.end(), [](unsigned char c) {
            return std::isxdigit(c) != 0;
        });
    if (!checksumHex) {
        response.error = "Missing or invalid checksum";
        return false;
    }

    unsigned int expected = checksum(body);
    unsigned int supplied = static_cast<unsigned int>(std::strtoul(suppliedChecksum.c_str(), nullptr, 16));
    if (expected != supplied) {
        response.error = "Checksum mismatch";
        return false;
    }

    response.valid = true;

    const unsigned char control = static_cast<unsigned char>(data[3]);
    if (control == 0x06 || control == 0x15) {
        response.ack = (control == 0x06);
        response.command.assign(1, static_cast<char>(control));
        if (data.size() >= 6) {
            response.parameter = data.substr(4, 2);
            response.errorCode = std::atoi(response.parameter.c_str());
        }
        return true;
    }

    if (data.size() < 9) {
        response.error = "Response too short";
        return false;
    }

    response.command = data.substr(3, 2);
    if (body.size() > 5) response.parameter = body.substr(5);
    return true;
}

bool Protocol::parseScientific(const std::string& value, double& result)
{
    std::string v = trim(value);
    if (v.empty()) return false;
    char* end = nullptr;
    result = std::strtod(v.c_str(), &end);
    return end != v.c_str() && *end == '\0' && std::isfinite(result);
}

bool Protocol::parseDd(const Response& response, std::vector<double>& spectrum,
                       double& totalPressure, int& errorFlag, int& tpSet)
{
    spectrum.clear();
    totalPressure = 0.0;
    errorFlag = 0;
    tpSet = 0;
    if (!response.valid || response.command != "DD") return false;

    std::vector<std::string> fields;
    std::string token;
    std::stringstream ss(response.parameter);
    while (std::getline(ss, token, ',')) fields.push_back(trim(token));

    // Scan/trend response: Ch1..ChN, TP, AN1, AN2, ERR, TP_SET.
    if (fields.size() < 6) return false;
    const std::size_t tpIndex = fields.size() - 5;
    if (!parseScientific(fields[tpIndex], totalPressure)) return false;
    tpSet = std::atoi(fields[fields.size() - 1].c_str());
    errorFlag = std::atoi(fields[fields.size() - 2].c_str());

    for (std::size_t i = 0; i < tpIndex; ++i) {
        double x = 0.0;
        if (!parseScientific(fields[i], x)) return false;
        spectrum.push_back(x);
    }
    return !spectrum.empty();
}

} // namespace leyrga
