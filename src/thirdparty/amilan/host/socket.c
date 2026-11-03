/* POSIX adapter for the network tests/tools, not linked into the Amiga. */
#include "amilan/socket.h"
#include "amilan/protocol.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
int amilan_socket_open(struct amilan_socket_api *api) { api->base = api; return 1; }
void amilan_socket_shutdown(struct amilan_socket_api *api) { api->base = 0; }
void amilan_socket_close(struct amilan_socket_api *api, int fd) { if (api->base && fd >= 0) close(fd); }
static int setup(int fd,int tcp)
{
    int n = AMILAN_SOCKET_BUFFER,one=1;
    if (fd < 0) return -1;
    if (fd >= FD_SETSIZE || fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n)) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) < 0) { close(fd); return -1; }
    /* Small control frames should not wait for a delayed TCP acknowledgement. */
    if(tcp)setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof(one));
    return fd;
}
static struct sockaddr_in address(const uint8_t ip[4], uint16_t port)
{
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a)); a.sin_family = AF_INET; a.sin_port = htons(port);
    if (ip) memcpy(&a.sin_addr, ip, 4);
    return a;
}
int amilan_socket_listen(struct amilan_socket_api *api, uint16_t port)
{ return amilan_socket_listen_at(api,0,port); }
int amilan_socket_listen_at(struct amilan_socket_api *api,const uint8_t ip[4],uint16_t port)
{
    int fd, one = 1;
    struct sockaddr_in a = address(ip, port);
    if (!api->base) return -1;
    fd = setup(socket(AF_INET, SOCK_STREAM, 0),1);
    if (fd < 0) return -1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0 ||
        bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(fd, AMILAN_CONNECTIONS) < 0) { close(fd); return -1; }
    return fd;
}
int amilan_socket_accept(struct amilan_socket_api *api, int fd)
{
    int n;
    if (!api->base) return -1;
    n = accept(fd, 0, 0);
    if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? AMILAN_AGAIN : -1;
    return setup(n,1);
}
int amilan_socket_connect(struct amilan_socket_api *api, const uint8_t ip[4], uint16_t port)
{
    int fd;
    struct sockaddr_in a = address(ip, port);
    if (!api->base) return -1;
    fd = setup(socket(AF_INET, SOCK_STREAM, 0),1);
    if (fd < 0) return -1;
    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) { close(fd); return -1; }
    return fd;
}
int amilan_socket_connect_ready(struct amilan_socket_api *api, int fd)
{
    fd_set w, e;
    struct timeval tv = { 0, 0 };
    int n, error = 0;
    socklen_t size = sizeof(error);
    if (!api->base || fd < 0 || fd >= FD_SETSIZE) return -1;
    FD_ZERO(&w); FD_ZERO(&e); FD_SET(fd, &w); FD_SET(fd, &e);
    n = select(fd + 1, 0, &w, &e, &tv);
    if (n < 0) return errno == EINTR ? 0 : -1;
    if (!n) return 0;
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error) return -1;
    return 1;
}
int amilan_socket_recv(struct amilan_socket_api *api, int fd, uint8_t *buf, unsigned size)
{
    int n;
    if (!api->base) return -1;
    n = (int)recv(fd, buf, size, 0);
    return n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ? AMILAN_AGAIN : n;
}
int amilan_socket_send(struct amilan_socket_api *api, int fd, const uint8_t *buf, unsigned size)
{
    int n;
    if (!api->base) return -1;
    n = (int)send(fd, buf, size, MSG_NOSIGNAL);
    return n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) ? AMILAN_AGAIN : n;
}
int amilan_socket_udp(struct amilan_socket_api *api,uint16_t port)
{
    int fd,one=1;struct sockaddr_in a=address(0,port);
    if(!api->base)return -1;
    fd=setup(socket(AF_INET,SOCK_DGRAM,0),0);if(fd<0)return -1;
    if(setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&one,sizeof(one))<0||bind(fd,(struct sockaddr *)&a,sizeof(a))<0){close(fd);return -1;}
    return fd;
}
int amilan_socket_sendto(struct amilan_socket_api *api,int fd,const uint8_t ip[4],uint16_t port,const uint8_t *buf,unsigned size)
{
    struct sockaddr_in a=address(ip,port);int n;
    if(!api->base)return -1;
    n=(int)sendto(fd,buf,size,MSG_NOSIGNAL,(struct sockaddr *)&a,sizeof(a));
    return n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR)?AMILAN_AGAIN:n;
}
int amilan_socket_recvfrom(struct amilan_socket_api *api,int fd,uint8_t ip[4],uint16_t *port,uint8_t *buf,unsigned size)
{
    struct sockaddr_in a; socklen_t len=sizeof(a);int n;
    if(!api->base)return -1;
    n=(int)recvfrom(fd,buf,size,MSG_TRUNC,(struct sockaddr *)&a,&len);
    if(n<0)return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?AMILAN_AGAIN:-1;
    if(len!=sizeof(a)||a.sin_family!=AF_INET)return -1;
    memcpy(ip,&a.sin_addr,4);*port=ntohs(a.sin_port);return n;
}
int amilan_socket_interfaces(struct amilan_socket_api *api,int fd,struct amilan_interface out[AMILAN_INTERFACES])
{
    struct ifreq entries[32],req;struct ifconf c;int i,n=0,j;
    if(!api->base)return 0;
    c.ifc_len=sizeof(entries);c.ifc_req=entries;if(ioctl(fd,SIOCGIFCONF,&c)<0)return 0;
    for(i=0;i<c.ifc_len/(int)sizeof(entries[0])&&i<32&&n<AMILAN_INTERFACES;i++){
        const struct sockaddr_in *a=(const struct sockaddr_in *)&entries[i].ifr_addr;
        const uint8_t *ip=(const uint8_t *)&a->sin_addr;
        if(a->sin_family!=AF_INET||!ip[0]||ip[0]==127||ip[0]>=224)continue;
        for(j=0;j<n;j++)if(!memcmp(out[j].ip,ip,4))break;
        if(j<n)continue;
        req=entries[i];if(ioctl(fd,SIOCGIFFLAGS,&req)<0||!(req.ifr_flags&IFF_UP))continue;
        memcpy(out[n].ip,ip,4);memset(out[n].broadcast,0,4);
        if(req.ifr_flags&IFF_BROADCAST){req=entries[i];if(ioctl(fd,SIOCGIFBRDADDR,&req)==0)memcpy(out[n].broadcast,&((struct sockaddr_in *)&req.ifr_broadaddr)->sin_addr,4);}
        n++;
    }
    return n;
}


int amilan_socket_peer(struct amilan_socket_api *api,int fd,uint8_t ip[4])
{ struct sockaddr_in a; socklen_t n=sizeof(a); if(!api->base||getpeername(fd,(struct sockaddr*)&a,&n)<0)return 0; const uint8_t* p=(const uint8_t*)&a.sin_addr;for(int i=0;i<4;i++)ip[i]=p[i];return 1; }
void amilan_socket_nodelay(struct amilan_socket_api *api,int fd,int enabled)
{ if(api->base)setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&enabled,sizeof enabled); }
void amilan_socket_finish(struct amilan_socket_api *api,int fd)
{ if(api->base)shutdown(fd,SHUT_WR); }
