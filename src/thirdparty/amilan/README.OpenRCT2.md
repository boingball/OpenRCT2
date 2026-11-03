# AmiLAN transport subset for OpenRCT2

Vendored from boingball/AmiLAN main at tree e2c6dc56d36b30ae8cb6b27f5bc83ad927e9bd3a.
Files: include/amilan/{socket,amiga,protocol}.h, amiga/socket.c,
host/socket.c, src/{protocol,ipv4_text}.c and tests/test_amiga.c.
Origin: https://github.com/boingball/AmiLAN
The repository owner explicitly authorised reuse for this port.

OpenRCT2 uses raw TCP/UDP only. AmiLAN packet framing, world-transfer,
compression and session protocols are not substituted for OpenRCT2's.
OpenRCT2 keeps its own packet format, authentication, map/object transfer,
game commands, tick synchronisation and checksums.

Local socket additions: bind a requested IPv4 listener address, peer IPv4
lookup, TCP_NODELAY and shutdown-write. Both native and POSIX test backends
implement these. Production library instances stay on their owning OS task;
accepted sockets share their listener's backend lifetime.

The Amiga bridge is implemented separately in platform/amiga_net_call.S
and amiga_net_bridge.c. DNS and IPv6 are not supplied in this prototype.
Direct numeric IPv4 is the initial test path. OpenRCT2's existing UDP LAN
query/reply format is retained, rather than AmiLAN discovery framing.
