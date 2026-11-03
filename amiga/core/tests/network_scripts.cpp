/* SPDX-License-Identifier: GPL-3.0-only */
#undef NDEBUG
#include <openrct2/network/ScriptTransfer.h>
#include <cassert>
#include <cstdio>
using namespace OpenRCT2::Network;

int main()
{
    // Actual server wire message from a build with ENABLE_SCRIPTING disabled.
    Packet empty(Command::scriptsData);
    empty << uint32_t{0};
    empty.header.size = empty.data.size();
    uint32_t count = 99;
    assert(ReadScriptsHeader(empty, count) && count == 0 && empty.bytesRead == 4);
    // A script-bearing host is distinguishable and must still be refused by
    // a no-scripting client, without destroying the active packet/connection.
    Packet scripts(Command::scriptsData);
    scripts << uint32_t{1} << uint32_t{3};
    scripts.write("abc", 3);
    scripts.header.size = scripts.data.size();
    assert(ReadScriptsHeader(scripts, count) && count == 1 && scripts.bytesRead == 4);
    for (size_t n = 0; n < 4; ++n)
    {
        Packet shortPacket(Command::scriptsData);
        shortPacket.data.resize(n);
        shortPacket.header.size = n;
        assert(!ReadScriptsHeader(shortPacket, count));
    }
    Packet trailing(Command::scriptsData);
    trailing << uint32_t{0} << uint8_t{42};
    trailing.header.size = trailing.data.size();
    assert(!ReadScriptsHeader(trailing, count));
    empty.bytesRead = 0; empty.header.size = 3;
    assert(!ReadScriptsHeader(empty, count));
    empty.header.size = 4; empty.bytesRead = 5;
    assert(!ReadScriptsHeader(empty, count));
    puts("Scripts handshake: zero-script host accepted, real scripts distinguished, short/trailing/inconsistent packets rejected PASS");
}
