/* SPDX-License-Identifier: GPL-3.0-only
 * Actual OpenRCT2 packet/connection code with a fragmented byte stream. */
#include <openrct2/network/NetworkConnection.h>
#include <openrct2/core/Crypt.h>
#include <openrct2/Diagnostic.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <deque>
namespace OpenRCT2::Network { Key::Key()=default; Key::~Key()=default; }
namespace OpenRCT2::Platform { uint32_t GetTicks() { return 1; } }
namespace OpenRCT2::Guard { void Assert(bool b,const char*,...) { assert(b); } }
void DiagnosticLog(DiagnosticLevel,const char*,...) {}
void DiagnosticLogWithLocation(DiagnosticLevel,const char*,const char*,int32_t,const char*,...) {}
using namespace OpenRCT2::Network;
class Fragments final : public ITcpSocket
{
public:
    std::deque<uint8_t> stream;
    SocketStatus GetStatus() const override {return SocketStatus::connected;}
    const char* GetError() const override {return nullptr;}
    const char* GetHostName() const override {return "test";}
    std::string GetIpAddress() const override {return "127.0.0.1";}
    void Listen(uint16_t) override {}
    void Listen(const std::string&,uint16_t) override {}
    std::unique_ptr<ITcpSocket> Accept() override {return nullptr;}
    void Connect(const std::string&,uint16_t) override {}
    void ConnectAsync(const std::string&,uint16_t) override {}
    size_t SendData(const void* p,size_t n) override
    { n=std::min<size_t>(n,2);const auto* b=static_cast<const uint8_t*>(p);stream.insert(stream.end(),b,b+n);return n; }
    ReadPacket ReceiveData(void* p,size_t,size_t* n) override
    { *n=0;if(stream.empty())return ReadPacket::noData;*static_cast<uint8_t*>(p)=stream.front();stream.pop_front();*n=1;return ReadPacket::success; }
    void SetNoDelay(bool) override {}
    void Finish() override {}
    void Disconnect() override {}
    void Close() override {}
};
int main()
{
    Connection c;c.socket=std::make_unique<Fragments>();c.authStatus=Auth::ok;
    for(unsigned round=0;round<16;++round)
    {
        Packet packet(Command::ping);packet<<uint32_t{0x12345678}<<uint16_t{0xabcd};
        c.queuePacket(packet);ReadPacket status=ReadPacket::moreData;
        unsigned i=0;for(;i<200 && status!=ReadPacket::success;++i){c.update();status=c.readPacket();}
        assert(status==ReadPacket::success && i>=sizeof(PacketHeader));
        assert(c.inboundPacket.data==packet.data && c.inboundPacket.header.id==Command::ping);
        c.inboundPacket.clear();
    }
    puts("OpenRCT2 framing: actual Connection serialize/parse, two-byte writes, one-byte reads and repeated packets PASS");
}
