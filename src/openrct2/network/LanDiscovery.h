#pragma once

#include "Network.h"
#include "ServerList.h"
#include "Socket.h"
#include "../core/Json.hpp"

namespace OpenRCT2::Network
{
    // Poll on the UI task: Amiga socket library bases belong to their opening
    // task. No nested std::async calls, blocking waits or master-server request.
    class LanDiscovery
    {
        std::unique_ptr<IUdpSocket> _socket;
        uint32_t _started = 0;

    public:
        void Start(uint32_t now)
        {
            _socket.reset();
            auto socket = CreateUdpSocket();
            const std::string_view query = kLanBroadcastMsg;
            bool sent = false;
            for (const auto& address : GetBroadcastAddresses())
                sent |= socket->SendData(address->GetHostname(), kLanBroadcastPort, query.data(), query.size()) == query.size();
            if (!sent)
                throw std::runtime_error("Unable to send LAN discovery query");
            _started = now;
            _socket = std::move(socket);
        }

        bool Active() const { return _socket != nullptr; }

        std::vector<ServerListEntry> Poll(uint32_t now)
        {
            std::vector<ServerListEntry> entries;
            if (!_socket)
                return entries;
            // Bound work even if a peer floods the discovery port.
            for (unsigned i = 0; i < 16; ++i)
            {
                char buffer[1024];
                size_t count = 0;
                std::unique_ptr<INetworkEndpoint> sender;
                auto status = _socket->ReceiveData(buffer, sizeof(buffer), &count, &sender);
                if (status == ReadPacket::disconnected)
                {
                    _socket.reset();
                    throw std::runtime_error("LAN discovery socket disconnected");
                }
                if (status != ReadPacket::success)
                    break;
                if (!sender || count == 0 || count >= sizeof(buffer))
                    continue;
                // Ignore malformed/truncated datagrams without aborting discovery.
                try
                {
                    auto json = json_t::parse(buffer, buffer + count);
                    if (!json.is_object())
                        continue;
                    json["ip"] = { { "v4", { sender->GetHostname() } } };
                    auto entry = ServerListEntry::FromJson(json);
                    if (entry)
                    {
                        entry->Local = true;
                        entries.push_back(std::move(*entry));
                    }
                }
                catch (const json_t::exception&) {}
            }
            if (static_cast<uint32_t>(now - _started) >= 2000)
                _socket.reset();
            return entries;
        }
    };
}
