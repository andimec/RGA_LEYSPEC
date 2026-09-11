#include "../leyrgaApp/src/LeyboldProtocol.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

static std::string frame(const std::string& body)
{
    return body + leyrga::Protocol::checksumString(body) + "\r\n";
}

int main()
{
    leyrga::Response r;

    const std::string ack = "#01\x06" "00" "00EA\r\n";
    assert(leyrga::Protocol::parseResponse(ack, r));
    assert(r.ack && r.errorCode == 0);

    const std::string body = "#01CS02111";
    assert(leyrga::Protocol::parseResponse(frame(body), r));
    assert(r.command == "CS" && r.parameter == "02111");

    const std::string bad = "#01CS021110000\r\n";
    assert(!leyrga::Protocol::parseResponse(bad, r));

    leyrga::DdData data;
    std::vector<double> trend = {2, 18, 28, 32, 40, 44};

    const std::string scanBody = "#01DD1.0e-8,2.0e-8,3.0e-6,0.1,0.2,0,0";
    assert(leyrga::Protocol::parseResponse(frame(scanBody), r));
    assert(leyrga::Protocol::parseDd(r, 0, 1, 2, trend, data));
    assert(data.values.size() == 2);
    assert(data.axis[0] == 1.0 && data.axis[1] == 2.0);
    assert(std::fabs(data.totalPressure - 3.0e-6) < 1e-12);

    const std::string trendBody = "#01DD1.0e-8,2.0e-8,3.0e-8,4.0e-8,5.0e-8,6.0e-8,7.0e-8,8.0e-8,9.0e-8,1.0e-7,1.1e-7,1.2e-7,1.3e-7,1.4e-7,1.5e-7,1.6e-7,1.7e-7,1.8e-7,1.9e-7,2.0e-7,3.0e-6,0.1,0.2,0,0";
    assert(leyrga::Protocol::parseResponse(frame(trendBody), r));
    assert(leyrga::Protocol::parseDd(r, 1, 1, 50, trend, data));
    assert(data.values.size() == 20);
    assert(data.axis[0] == 2.0 && data.axis[5] == 44.0);

    std::string analogParams;
    for (int i = 0; i < 20; ++i) {
        if (i) analogParams += ",";
        analogParams += std::to_string(i * 20);
        analogParams += ",1.0e-8";
    }
    analogParams += ",0,0";
    const std::string analogBody = "#01DD" + analogParams;
    assert(leyrga::Protocol::parseResponse(frame(analogBody), r));
    assert(leyrga::Protocol::parseDd(r, 2, 1, 50, trend, data));
    assert(data.values.size() == 20);
    assert(data.axis[0] == 0.0 && data.axis[1] == 1.0);

    std::cout << "Leybold protocol V1 tests passed.\n";
    return 0;
}
