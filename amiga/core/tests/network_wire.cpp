/* SPDX-License-Identifier: GPL-3.0-only */
#include <openrct2/network/NetworkPacket.h>
#include <openrct2/network/Socket.h>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace OpenRCT2::Network;
int main()
{
    Packet p(Command::ping); p << uint16_t{0x1234} << uint32_t{0x89abcdef};
    const unsigned char expected[]={0x12,0x34,0x89,0xab,0xcd,0xef};
    assert(p.data.size()==sizeof expected && !std::memcmp(p.getData(),expected,sizeof expected));
    p.header.size=sizeof expected;
    uint16_t a=0;uint32_t b=0;p>>a>>b;assert(a==0x1234 && b==0x89abcdef);
    auto magic=OpenRCT2::Convert::HostToNetwork(PacketHeader::kMagic);
    const unsigned char wireMagic[]={0x32,0x54,0x52,0x4f};assert(!std::memcmp(&magic,wireMagic,4));
    puts("OpenRCT2 protocol: actual Packet scalar write/read and network header magic fixed bytes PASS");
}
