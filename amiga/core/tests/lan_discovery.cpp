/* SPDX-License-Identifier: GPL-3.0-only */
#undef NDEBUG
#include <openrct2/network/LanDiscovery.h>
#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>
#include <thread>

using namespace OpenRCT2::Network;
static std::deque<std::string> incoming;
static auto owner = std::this_thread::get_id();
static int live = 0, queries = 0, reads = 0;
static bool disconnected = false, sendFails = false;
struct Endpoint final : INetworkEndpoint
{
    std::string GetHostname() const override { return "192.168.1.10"; }
};
struct Socket final : IUdpSocket
{
    Socket() { ++live; }
    ~Socket() override { assert(owner == std::this_thread::get_id()); --live; }
    SocketStatus GetStatus() const override { return SocketStatus::listening; }
    const char* GetError() const override { return ""; }
    const char* GetHostName() const override { return ""; }
    void Listen(uint16_t) override {}
    void Listen(const std::string&, uint16_t) override {}
    size_t SendData(const INetworkEndpoint&, const void*, size_t) override { assert(false); return 0; }
    size_t SendData(const std::string& address, uint16_t port, const void* data, size_t size) override
    {
        assert(owner == std::this_thread::get_id());
        assert(address == "192.168.1.10" && port == 11754);
        assert(std::string_view(static_cast<const char*>(data), size) == "openrct2.server.query");
        ++queries;
        return sendFails ? 0 : size;
    }
    ReadPacket ReceiveData(void* out, size_t capacity, size_t* count, std::unique_ptr<INetworkEndpoint>* sender) override
    {
        assert(owner == std::this_thread::get_id());
        ++reads;
        if (disconnected) return ReadPacket::disconnected;
        if (incoming.empty()) return ReadPacket::noData;
        auto data = incoming.front(); incoming.pop_front();
        *count = std::min(capacity, data.size());
        memcpy(out, data.data(), *count);
        *sender = std::make_unique<Endpoint>();
        return ReadPacket::success;
    }
    void Close() override {}
};
namespace OpenRCT2::Network
{
    std::unique_ptr<IUdpSocket> CreateUdpSocket() { return std::make_unique<Socket>(); }
    std::vector<std::unique_ptr<INetworkEndpoint>> GetBroadcastAddresses()
    {
        std::vector<std::unique_ptr<INetworkEndpoint>> out;
        out.push_back(std::make_unique<Endpoint>()); return out;
    }
    // Only endpoint conversion is mocked; discovery and JSON parsing are production code.
    std::optional<ServerListEntry> ServerListEntry::FromJson(json_t& json)
    {
        ServerListEntry entry;
        entry.Address = json["ip"]["v4"][0].get<std::string>();
        return entry;
    }
}
int main()
{
    LanDiscovery discovery;
    discovery.Start(100);
    assert(live == 1 && queries == 1 && discovery.Active());
    incoming = { "broken json", "[]", std::string(1024, 'x'), "{}" };
    auto entries = discovery.Poll(101);
    assert(entries.size() == 1 && entries[0].Local && entries[0].Address == "192.168.1.10");
    assert(discovery.Active());
    discovery.Poll(2099); assert(discovery.Active());
    discovery.Poll(2100); assert(!discovery.Active() && live == 0);
    discovery.Start(UINT32_MAX - 1000);
    discovery.Poll(1000); assert(!discovery.Active() && live == 0);
    discovery.Start(0);
    for (int i = 0; i < 40; ++i) incoming.push_back("{}");
    const int before = reads;
    assert(discovery.Poll(1).size() == 16 && reads - before == 16);
    discovery.Start(2); assert(live == 1); // restarting closes the old socket
    disconnected = true;
    try { discovery.Poll(3); assert(false); } catch (const std::runtime_error&) {}
    assert(!discovery.Active() && live == 0);
    sendFails = true;
    try { discovery.Start(4); assert(false); } catch (const std::runtime_error&) {}
    assert(!discovery.Active() && live == 0);
    std::cout << "LAN discovery: owning task, bounded polling, malformed input, timeout/wrap, restart and failure cleanup PASS\n";
}
