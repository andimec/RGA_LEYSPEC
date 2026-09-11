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

static bool parseInt(const std::string& text, int& value)
{
    const std::string v = trim(text);
    if (v.empty()) return false;
    char* end = nullptr;
    long n = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || *end != '\0') return false;
    value = static_cast<int>(n);
    return true;
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

    int address = 0;
    if (!parseInt(data.substr(1, 2), address) || address < 1 || address > 16) {
        response.error = "Invalid RGA address";
        return false;
    }
    response.address = address;

    const std::size_t checksumPos = data.size() - 4;
    const std::string body = data.substr(0, checksumPos);
    const std::string suppliedChecksum = data.substr(checksumPos);
    const bool checksumHex = suppliedChecksum.size() == 4 &&
        std::all_of(suppliedChecksum.begin(), suppliedChecksum.end(), [](unsigned char c) {
            return std::isxdigit(c) != 0;
        });
    if (!checksumHex) {
        response.error = "Missing or invalid checksum";
        return false;
    }

    const unsigned int expected = checksum(body);
    const unsigned int supplied = static_cast<unsigned int>(
        std::strtoul(suppliedChecksum.c_str(), nullptr, 16));
    if (expected != supplied) {
        response.error = "Checksum mismatch";
        return false;
    }

    response.valid = true;

    const unsigned char control = static_cast<unsigned char>(data[3]);
    if (control == 0x06 || control == 0x15) {
        response.ack = (control == 0x06);
        response.command.assign(1, static_cast<char>(control));
        if (body.size() >= 6) {
            response.parameter = body.substr(4, 2);
            parseInt(response.parameter, response.errorCode);
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
    const std::string v = trim(value);
    if (v.empty()) return false;
    char* end = nullptr;
    result = std::strtod(v.c_str(), &end);
    return end != v.c_str() && *end == '\0' && std::isfinite(result);
}

bool Protocol::parseDd(const Response& response, int mode, int firstMass, int lastMass,
                       const std::vector<double>& trendMasses, DdData& data)
{
    data = DdData();
    if (!response.valid || response.command != "DD") return false;

    std::vector<std::string> fields;
    std::string token;
    std::stringstream ss(response.parameter);
    while (std::getline(ss, token, ',')) fields.push_back(trim(token));

    if (mode == 2) {
        // Analog mode: 20 pairs of DAC value and measurement, followed by ERR and TP_SET.
        if (fields.size() != 42) return false;
        const std::size_t statusIndex = fields.size() - 2;
        if (!parseInt(fields[statusIndex], data.errorFlag)) return false;
        if (!parseInt(fields[statusIndex + 1], data.tpSet)) return false;
        for (std::size_t i = 0; i < statusIndex; i += 2) {
            int dac = 0;
            double value = 0.0;
            if (!parseInt(fields[i], dac) || !parseScientific(fields[i + 1], value)) return false;
            data.axis.push_back(static_cast<double>(dac) / 20.0);
            data.values.push_back(value);
        }
        data.valid = data.values.size() == 20;
        return data.valid;
    }

    // Scan and Trend mode: Ch1..ChN, TP, AN1, AN2, ERR, TP_SET.
    if (fields.size() < 6) return false;
    const std::size_t tpIndex = fields.size() - 5;
    if (!parseScientific(fields[tpIndex], data.totalPressure)) return false;
    if (!parseScientific(fields[tpIndex + 1], data.analog1)) return false;
    if (!parseScientific(fields[tpIndex + 2], data.analog2)) return false;
    if (!parseInt(fields[tpIndex + 3], data.errorFlag)) return false;
    if (!parseInt(fields[tpIndex + 4], data.tpSet)) return false;

    for (std::size_t i = 0; i < tpIndex; ++i) {
        double value = 0.0;
        if (!parseScientific(fields[i], value)) return false;
        data.values.push_back(value);
    }

    if (mode == 0) {
        if (firstMass < 1 || lastMass < firstMass) return false;
        for (int mass = firstMass; mass <= lastMass && data.axis.size() < data.values.size(); ++mass)
            data.axis.push_back(static_cast<double>(mass));
    } else if (mode == 1) {
        for (std::size_t i = 0; i < data.values.size(); ++i)
            data.axis.push_back(i < trendMasses.size() ? trendMasses[i] : 0.0);
    }

    data.valid = !data.values.empty() && data.axis.size() == data.values.size();
    return data.valid;
}

} // namespace leyrga
