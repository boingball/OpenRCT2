/* SPDX-License-Identifier: GPL-3.0-only
 * Actual OpenRCT2 Amiga socket adapter over AmiLAN's POSIX test backend. */
#include <openrct2/network/Socket.h>
#include <cassert>
#include <chrono>
#include <thread>
#include <cstdio>
#include <vector>
#include <cstring>
namespace OpenRCT2::Platform {
uint32_t GetTicks() { return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
void Sleep(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
}
using namespace OpenRCT2::Network;
int main()
{
    auto listener=CreateTcpSocket();unsigned port;
    for(port=42170;port<42270;++port) { try { listener->Listen("127.0.0.1",static_cast<uint16_t>(port));break; } catch (...) {} }
    assert(port<42270);
    auto client=CreateTcpSocket();client->ConnectAsync("127.0.0.1",static_cast<uint16_t>(port));
    std::unique_ptr<ITcpSocket> accepted;
    for(unsigned i=0;i<5000 && (!accepted || client->GetStatus()!=SocketStatus::connected);++i)
    { if(!accepted)accepted=listener->Accept();OpenRCT2::Platform::Sleep(1); }
    assert(accepted && client->GetStatus()==SocketStatus::connected && accepted->GetIpAddress()=="127.0.0.1");
    listener->Close(); /* accepted sockets must retain the owning backend */
    std::vector<unsigned char> tx(1024*1024),rx(tx.size());
    for(size_t i=0;i<tx.size();++i)tx[i]=static_cast<unsigned char>(i*37+11);
    size_t sent=0,got=0;unsigned loops=0;
    while(got<rx.size() && ++loops<200000)
    {
        if(sent<tx.size())sent+=client->SendData(tx.data()+sent,tx.size()-sent);
        size_t n=0;auto status=accepted->ReceiveData(rx.data()+got,std::min<size_t>(rx.size()-got,137),&n);
        assert(status!=ReadPacket::disconnected);got+=n;
    }
    assert(got==tx.size() && sent==tx.size() && tx==rx);
    client->Finish();size_t n=0;ReadPacket status=ReadPacket::noData;
    for(unsigned i=0;i<1000 && status!=ReadPacket::disconnected;++i)
    { unsigned char b;status=accepted->ReceiveData(&b,1,&n);OpenRCT2::Platform::Sleep(1); }
    assert(status==ReadPacket::disconnected);
    client->Close();accepted->Close();
    auto invalid=CreateTcpSocket();invalid->ConnectAsync("example.invalid",11753);assert(invalid->GetStatus()==SocketStatus::closed && invalid->GetError());
    auto udp=CreateUdpSocket();udp->Listen(static_cast<uint16_t>(port));auto sender=CreateUdpSocket();
    const unsigned char payload[]={0x32,0x54,0x52,0x4f,0,2,0,255};
    assert(sender->SendData("127.0.0.1",static_cast<uint16_t>(port),payload,sizeof payload)==sizeof payload);
    unsigned char out[64];std::unique_ptr<INetworkEndpoint> from;
    for(unsigned i=0;i<1000;++i) { if(udp->ReceiveData(out,sizeof out,&n,&from)==ReadPacket::success)break;OpenRCT2::Platform::Sleep(1); }
    assert(n==sizeof payload && !memcmp(out,payload,n) && from && from->GetHostname()=="127.0.0.1");
    puts("AmiLAN/OpenRCT2 sockets: real TCP 1 MiB partial I/O, accept lifetime, EOF, invalid address and raw UDP bytes PASS");
}
