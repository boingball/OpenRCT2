/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "NetworkPacket.h"

#ifndef DISABLE_NETWORK
namespace OpenRCT2::Network
{
    // A scriptsData packet is sent even by hosts built without scripting.
    // Its zero count is a valid handshake message, not a plugin requirement.
    inline bool ReadScriptsHeader(Packet& packet, uint32_t& count)
    {
        if (packet.header.size != packet.data.size() || packet.bytesRead > packet.data.size()
            || packet.data.size() - packet.bytesRead < sizeof(count))
            return false;
        packet >> count;
        return count != 0 || packet.bytesRead == packet.data.size();
    }
}
#endif
