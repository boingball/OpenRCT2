/* Platform socket adapter. All calls nonblocking; no DNS, threads or heap.
 * Open lazily, only with OS scheduling enabled. Close peers before backend. */
#ifndef AMILAN_SOCKET_H
#define AMILAN_SOCKET_H
#include <stdint.h>
#define AMILAN_AGAIN (-2)
#define AMILAN_SOCKET_BUFFER 4096
#define AMILAN_INTERFACES 4
/* Remote connection slots a host takes (a game for eight is the host and
 * seven more); also the listen backlog. A game may build with its own. */
#ifndef AMILAN_CONNECTIONS
#define AMILAN_CONNECTIONS 7
#endif
struct amilan_interface { uint8_t ip[4], broadcast[4]; };
struct amilan_socket_api { void *base; };
int amilan_socket_open(struct amilan_socket_api *api);
void amilan_socket_shutdown(struct amilan_socket_api *api);
int amilan_socket_listen(struct amilan_socket_api *api, uint16_t port);
int amilan_socket_listen_at(struct amilan_socket_api*,const uint8_t ip[4],uint16_t port);
int amilan_socket_peer(struct amilan_socket_api*,int fd,uint8_t ip[4]);
void amilan_socket_nodelay(struct amilan_socket_api*,int fd,int enabled);
void amilan_socket_finish(struct amilan_socket_api*,int fd);
int amilan_socket_accept(struct amilan_socket_api *api, int listener);
/* returns fd, then poll connect_ready (even if immediately connected). */
int amilan_socket_connect(struct amilan_socket_api *api, const uint8_t ip[4], uint16_t port);
int amilan_socket_connect_ready(struct amilan_socket_api *api, int fd);
int amilan_socket_recv(struct amilan_socket_api *api, int fd, uint8_t *buf, unsigned size);
int amilan_socket_send(struct amilan_socket_api *api, int fd, const uint8_t *buf, unsigned size);
void amilan_socket_close(struct amilan_socket_api *api, int fd);
/* Optional LAN discovery adapter; never DNS or blocking waits. Callers reserve
 * one byte beyond their maximum valid datagram to detect truncation on stacks
 * that return the copied length rather than the original datagram length. */
int amilan_socket_udp(struct amilan_socket_api *api, uint16_t port);
int amilan_socket_sendto(struct amilan_socket_api *api,int fd,const uint8_t ip[4],uint16_t port,const uint8_t *buf,unsigned size);
int amilan_socket_recvfrom(struct amilan_socket_api *api,int fd,uint8_t ip[4],uint16_t *port,uint8_t *buf,unsigned size);
int amilan_socket_interfaces(struct amilan_socket_api *api,int fd,struct amilan_interface out[AMILAN_INTERFACES]);
#endif

