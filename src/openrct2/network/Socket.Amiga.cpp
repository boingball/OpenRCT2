/* SPDX-License-Identifier: GPL-3.0-only
 * OpenRCT2 socket interface over AmiLAN. Only raw TCP/UDP transport is used:
 * OpenRCT2 retains framing, authentication, park transfer and simulation. */
#if (defined(__amigaos__) || defined(OPENRCT2_AMIGA_SOCKET_TEST)) && !defined(DISABLE_NETWORK)
#include "Socket.h"
#include "../platform/Platform.h"
#include <algorithm>
#include <array>
#include <stdexcept>
extern "C" {
#include <amilan/socket.h>
#include <amilan/protocol.h>
}
namespace OpenRCT2::Network
{
namespace
{
struct Session
{
    amilan_socket_api api{};
    Session()
    {
        if (!amilan_socket_open(&api)) throw std::runtime_error("AmiLAN: bsdsocket.library v4 unavailable; start your TCP/IP stack");
    }
    ~Session() { amilan_socket_shutdown(&api); }
};
using Ip = std::array<uint8_t,4>;
Ip parse(const std::string& address)
{
    Ip ip{};
    if (!amilan_ipv4(address.c_str(),ip.data())) throw std::runtime_error("AmiLAN prototype requires a numeric IPv4 address");
    return ip;
}
std::string text(const Ip& ip)
{
    char out[16]; amilan_ipv4_text(out,ip.data()); return out;
}
struct Endpoint final : INetworkEndpoint
{
    Ip ip{}; uint16_t port{};
    Endpoint(Ip a,uint16_t p) : ip(a),port(p) {}
    std::string GetHostname() const override { return text(ip); }
};
class Tcp final : public ITcpSocket
{
    std::shared_ptr<Session> _session;
    int _fd=-1;
    mutable SocketStatus _status=SocketStatus::closed;
    mutable std::string _error;
    std::string _address;
    uint32_t _connectStart=0;
    void startSession() { if (!_session) _session=std::make_shared<Session>(); }
    void requireClosed() { if (_fd>=0) throw std::runtime_error("Socket not closed"); }
public:
    Tcp()=default;
    Tcp(std::shared_ptr<Session> session,int fd) : _session(std::move(session)),_fd(fd),_status(SocketStatus::connected)
    {
        Ip ip{};
        if (amilan_socket_peer(&_session->api,_fd,ip.data())) _address=text(ip);
        else _address="unknown";
    }
    ~Tcp() override { Close(); }
    SocketStatus GetStatus() const override
    {
        if (_status==SocketStatus::connecting)
        {
            int n=amilan_socket_connect_ready(&_session->api,_fd);
            if (n>0) _status=SocketStatus::connected;
            else if (n<0 || Platform::GetTicks()-_connectStart>=10000)
            { _error="AmiLAN: connection failed or timed out"; _status=SocketStatus::closed; }
        }
        return _status;
    }
    const char* GetError() const override { return _error.empty()?nullptr:_error.c_str(); }
    const char* GetHostName() const override { return _address.c_str(); }
    std::string GetIpAddress() const override { return _address; }
    void Listen(uint16_t port) override { Listen("",port); }
    void Listen(const std::string& address,uint16_t port) override
    {
        requireClosed(); startSession(); Ip ip{};
        if (!address.empty()) ip=parse(address);
        _fd=amilan_socket_listen_at(&_session->api,ip.data(),port);
        if (_fd<0) throw std::runtime_error("AmiLAN: cannot listen on requested address/port");
        _status=SocketStatus::listening; _address=address;
    }
    std::unique_ptr<ITcpSocket> Accept() override
    {
        if (_status!=SocketStatus::listening) throw std::runtime_error("Socket not listening");
        int fd=amilan_socket_accept(&_session->api,_fd);
        if (fd==AMILAN_AGAIN) return nullptr;
        if (fd<0) throw std::runtime_error("AmiLAN: accept failed");
        try { return std::make_unique<Tcp>(_session,fd); }
        catch (...) { amilan_socket_close(&_session->api,fd); throw; }
    }
    void ConnectAsync(const std::string& address,uint16_t port) override
    {
        requireClosed(); _error.clear(); _address=address;
        try
        {
            Ip ip=parse(address); startSession();
            _fd=amilan_socket_connect(&_session->api,ip.data(),port);
            if (_fd<0) throw std::runtime_error("AmiLAN: connect failed");
            _connectStart=Platform::GetTicks(); _status=SocketStatus::connecting;
        }
        catch (const std::exception& e) { _error=e.what(); _status=SocketStatus::closed; }
    }
    void Connect(const std::string& address,uint16_t port) override
    {
        ConnectAsync(address,port);
        while (GetStatus()==SocketStatus::connecting) Platform::Sleep(1);
        if (_status!=SocketStatus::connected) throw std::runtime_error(_error);
    }
    size_t SendData(const void* buffer,size_t size) override
    {
        if (GetStatus()!=SocketStatus::connected || !size) return 0;
        int n=amilan_socket_send(&_session->api,_fd,static_cast<const uint8_t*>(buffer),static_cast<unsigned>(std::min<size_t>(size,65536)));
        if (n==AMILAN_AGAIN) return 0;
        if (n<=0) { _error="AmiLAN: send disconnected"; _status=SocketStatus::closed; return 0; }
        return static_cast<size_t>(n);
    }
    ReadPacket ReceiveData(void* buffer,size_t size,size_t* received) override
    {
        *received=0;
        if (GetStatus()!=SocketStatus::connected) return ReadPacket::disconnected;
        if (!size) return ReadPacket::noData;
        int n=amilan_socket_recv(&_session->api,_fd,static_cast<uint8_t*>(buffer),static_cast<unsigned>(std::min<size_t>(size,65536)));
        if (n==AMILAN_AGAIN) return ReadPacket::noData;
        if (n<=0) { _status=SocketStatus::closed; return ReadPacket::disconnected; }
        *received=static_cast<size_t>(n); return ReadPacket::success;
    }
    void SetNoDelay(bool enabled) override { if (_fd>=0) amilan_socket_nodelay(&_session->api,_fd,enabled); }
    void Finish() override { if (_fd>=0) amilan_socket_finish(&_session->api,_fd); }
    void Disconnect() override { Close(); }
    void Close() override
    {
        if (_fd>=0) amilan_socket_close(&_session->api,_fd);
        _fd=-1; _status=SocketStatus::closed; _session.reset();
    }
};
class Udp final : public IUdpSocket
{
    std::shared_ptr<Session> _session;
    int _fd=-1;
    std::string _error;
    void ensure(uint16_t port=0)
    {
        if (_fd>=0) return;
        _session=std::make_shared<Session>(); _fd=amilan_socket_udp(&_session->api,port);
        if (_fd<0) throw std::runtime_error("AmiLAN: UDP open failed");
    }
public:
    ~Udp() override { Close(); }
    SocketStatus GetStatus() const override { return _fd>=0?SocketStatus::listening:SocketStatus::closed; }
    const char* GetError() const override { return _error.empty()?nullptr:_error.c_str(); }
    const char* GetHostName() const override { return ""; }
    void Listen(uint16_t port) override { Listen("",port); }
    void Listen(const std::string& address,uint16_t port) override
    {
        if (!address.empty() && address!="0.0.0.0") throw std::runtime_error("AmiLAN: UDP prototype binds all IPv4 interfaces");
        if (_fd>=0) throw std::runtime_error("Socket not closed");
        ensure(port);
    }
    size_t SendData(const std::string& address,uint16_t port,const void* buffer,size_t size) override
    { return SendData(Endpoint(parse(address),port),buffer,size); }
    size_t SendData(const INetworkEndpoint& destination,const void* buffer,size_t size) override
    {
        const auto* endpoint=dynamic_cast<const Endpoint*>(&destination);
        if (!endpoint || size>65507) throw std::runtime_error("AmiLAN: invalid IPv4 datagram");
        ensure();
        int n=amilan_socket_sendto(&_session->api,_fd,endpoint->ip.data(),endpoint->port,static_cast<const uint8_t*>(buffer),static_cast<unsigned>(size));
        return n<0?0:static_cast<size_t>(n);
    }
    ReadPacket ReceiveData(void* buffer,size_t size,size_t* received,std::unique_ptr<INetworkEndpoint>* sender) override
    {
        *received=0; ensure(); Ip ip{}; uint16_t port=0;
        int n=amilan_socket_recvfrom(&_session->api,_fd,ip.data(),&port,static_cast<uint8_t*>(buffer),static_cast<unsigned>(std::min<size_t>(size,65536)));
        if (n==AMILAN_AGAIN) return ReadPacket::noData;
        if (n<0) return ReadPacket::disconnected;
        *received=static_cast<size_t>(n);
        if (sender) *sender=std::make_unique<Endpoint>(ip,port);
        return ReadPacket::success;
    }
    void Close() override
    { if (_fd>=0) amilan_socket_close(&_session->api,_fd); _fd=-1; _session.reset(); }
};
}
std::unique_ptr<ITcpSocket> CreateTcpSocket() { return std::make_unique<Tcp>(); }
std::unique_ptr<IUdpSocket> CreateUdpSocket() { return std::make_unique<Udp>(); }
std::vector<std::unique_ptr<INetworkEndpoint>> GetBroadcastAddresses()
{
    std::vector<std::unique_ptr<INetworkEndpoint>> result;
    Session session; int fd=amilan_socket_udp(&session.api,0);
    if (fd<0) return result;
    amilan_interface interfaces[AMILAN_INTERFACES];
    int n=amilan_socket_interfaces(&session.api,fd,interfaces);
    amilan_socket_close(&session.api,fd);
    for (int i=0;i<n;++i)
    {
        Ip ip{}; std::copy_n(interfaces[i].broadcast,4,ip.begin());
        if (ip!=Ip{}) result.push_back(std::make_unique<Endpoint>(ip,0));
    }
    if (result.empty()) result.push_back(std::make_unique<Endpoint>(Ip{255,255,255,255},0));
    return result;
}
}
#endif
