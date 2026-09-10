#include "../leyrgaApp/src/LeyboldProtocol.h"
#include <cassert>
#include <iostream>

int main()
{
    std::string ack = "#01\x06" "00" "00EA\r\n";
    leyrga::Response r;
    assert(leyrga::Protocol::parseResponse(ack, r));
    assert(r.ack && r.errorCode == 0);

    const std::string body = "#01CS02111";
    const std::string frame = body + leyrga::Protocol::checksumString(body) + "\r\n";
    assert(leyrga::Protocol::parseResponse(frame, r));
    assert(r.command == "CS" && r.parameter == "02111");

    std::cout << "Leybold protocol smoke tests passed.\n";
    return 0;
}
