/* Optional AmiTCP v4-compatible bsdsocket.library backend. Independently
 * written from the public SDK's ABI facts; no SDK headers or code included.
 * IMPORTANT: hardware takeover/Forbid must end before opening this backend. */
#include "amilan/amiga.h"
#include "amilan/socket.h"
#include "amilan/protocol.h"

#define L_SOCKET (-30)
#define L_BIND (-36)
#define L_LISTEN (-42)
#define L_ACCEPT (-48)
#define L_CONNECT (-54)
#define L_SENDTO (-60)
#define L_RECVFROM (-72)
#define L_SEND (-66)
#define L_RECV (-78)
#define L_SETOPT (-90)
#define L_GETOPT (-96)
#define L_IOCTL (-114)
#define L_CLOSE (-120)
#define L_SELECT (-126)
#define L_SIGNALS (-132)
#define L_ERRNO (-162)
#define AGAIN 35
#define INTERRUPTED 4
#define IN_PROGRESS 36
#define ALREADY 37
#define IS_CONNECTED 56
#define SOCKET_LEVEL 0xffff
#define OPT_ERROR 0x1007
#define OPT_SNDBUF 0x1001
#define OPT_RCVBUF 0x1002
#define OPT_REUSE 4
#define NONBLOCK 0x8004667eUL
/* Deliberately cap descriptors to one native 32-bit fd mask. */
#define FD_LIMIT 32

static long call(struct amilan_socket_api *api, long lvo, long *regs)
{
    return os_call(api->base, lvo, regs);
}
static int error(struct amilan_socket_api *api)
{
    long r[14] = { 0 };
    return (int)call(api, L_ERRNO, r);
}
static int would_block(struct amilan_socket_api *api)
{
    int n = error(api);
    return n == AGAIN || n == INTERRUPTED;
}
int amilan_socket_open(struct amilan_socket_api *api)
{
    long r[14] = { 0 };
    if (api->base) return 1;
    api->base = os_openlib("bsdsocket.library", 4);
    if (!api->base) return 0;
    /* No asynchronous socket signals or Ctrl-C interrupts in the game. */
    call(api, L_SIGNALS, r);
    return 1;
}
void amilan_socket_shutdown(struct amilan_socket_api *api)
{
    if (api->base) os_closelib(api->base);
    api->base = 0;
}
void amilan_socket_close(struct amilan_socket_api *api, int fd)
{
    long r[14] = { 0 };
    if (api->base && fd >= 0) { r[0] = fd; call(api, L_CLOSE, r); }
}
static int option(struct amilan_socket_api *api, int fd, int name, long value)
{
    long r[14] = { 0 };
    r[0] = fd; r[1] = SOCKET_LEVEL; r[2] = name;
    r[8] = (long)&value; r[3] = 4;
    return call(api, L_SETOPT, r) == 0;
}
static int setup(struct amilan_socket_api *api, int fd,int tcp)
{
    long r[14] = { 0 }, nonblock = 1;
    if (fd < 0) return -1;
    r[0] = fd; r[1] = NONBLOCK; r[8] = (long)&nonblock;
    if (fd >= FD_LIMIT || call(api, L_IOCTL, r) < 0 ||
        !option(api, fd, OPT_SNDBUF, AMILAN_SOCKET_BUFFER) ||
        !option(api, fd, OPT_RCVBUF, AMILAN_SOCKET_BUFFER)) {
        amilan_socket_close(api, fd); return -1;
    }
    /* IPPROTO_TCP=6, TCP_NODELAY=1 in the BSD API. Optional on older stacks. */
    for(int i=0;i<14;i++)r[i]=0;
    r[0]=fd;r[1]=6;r[2]=1;r[8]=(long)&nonblock;r[3]=4;
    if(tcp)call(api,L_SETOPT,r);
    return fd;
}
static int new_socket(struct amilan_socket_api *api)
{
    long r[14] = { 0 };
    if (!api->base) return -1;
    r[0] = 2; r[1] = 1; /* AF_INET, SOCK_STREAM, protocol 0 */
    return setup(api, (int)call(api, L_SOCKET, r),1);
}
static void address(uint8_t a[16], const uint8_t ip[4], uint16_t port)
{
    int i;
    for (i = 0; i < 16; i++) a[i] = 0;
    a[0] = 16; a[1] = 2; /* sockaddr length, AF_INET */
    amilan_put16(a + 2, port);
    if (ip) for (i = 0; i < 4; i++) a[4 + i] = ip[i];
}
int amilan_socket_listen(struct amilan_socket_api *api, uint16_t port)
{ return amilan_socket_listen_at(api,0,port); }
int amilan_socket_listen_at(struct amilan_socket_api *api,const uint8_t ip[4],uint16_t port)
{
    uint8_t a[16];
    long r[14] = { 0 };
    int fd = new_socket(api);
    if (fd < 0) return -1;
    address(a, ip, port);
    r[0] = fd; r[8] = (long)a; r[1] = 16;
    if (!option(api, fd, OPT_REUSE, 1) || call(api, L_BIND, r) < 0) goto fail;
    r[8] = 0; r[1] = AMILAN_CONNECTIONS;
    if (call(api, L_LISTEN, r) < 0) goto fail;
    return fd;
fail:
    amilan_socket_close(api, fd); return -1;
}
int amilan_socket_accept(struct amilan_socket_api *api, int fd)
{
    long r[14] = { 0 };
    int n;
    if (!api->base) return -1;
    r[0] = fd; n = (int)call(api, L_ACCEPT, r);
    if (n < 0) return would_block(api) ? AMILAN_AGAIN : -1;
    return setup(api, n,1);
}
int amilan_socket_connect(struct amilan_socket_api *api, const uint8_t ip[4], uint16_t port)
{
    uint8_t a[16];
    long r[14] = { 0 };
    int n, fd = new_socket(api);
    if (fd < 0) return -1;
    address(a, ip, port);
    r[0] = fd; r[8] = (long)a; r[1] = 16;
    if (call(api, L_CONNECT, r) < 0) {
        n = error(api);
        if (n != IN_PROGRESS && n != ALREADY && n != AGAIN && n != IS_CONNECTED) {
            amilan_socket_close(api, fd); return -1;
        }
    }
    return fd;
}
int amilan_socket_connect_ready(struct amilan_socket_api *api, int fd)
{
    long r[14] = { 0 }, tv[2] = { 0, 0 }, size = 4, result = 0;
    uint32_t w, e;
    int n;
    if (!api->base || fd < 0 || fd >= FD_LIMIT) return -1;
    w = e = 1UL << fd;
    r[0] = fd + 1; r[9] = (long)&w; r[10] = (long)&e; r[11] = (long)tv;
    n = (int)call(api, L_SELECT, r);
    if (n < 0) return error(api) == INTERRUPTED ? 0 : -1;
    if (!n) return 0;
    r[0] = fd; r[1] = SOCKET_LEVEL; r[2] = OPT_ERROR;
    r[8] = (long)&result; r[9] = (long)&size;
    if (call(api, L_GETOPT, r) < 0 || size != 4 || result) return -1;
    return 1;
}
int amilan_socket_recv(struct amilan_socket_api *api, int fd, uint8_t *buf, unsigned size)
{
    long r[14] = { 0 };
    int n;
    if (!api->base) return -1;
    r[0] = fd; r[8] = (long)buf; r[1] = size;
    n = (int)call(api, L_RECV, r);
    return n < 0 && would_block(api) ? AMILAN_AGAIN : n;
}
int amilan_socket_send(struct amilan_socket_api *api, int fd, const uint8_t *buf, unsigned size)
{
    long r[14] = { 0 };
    int n;
    if (!api->base) return -1;
    r[0] = fd; r[8] = (long)buf; r[1] = size;
    n = (int)call(api, L_SEND, r);
    return n < 0 && would_block(api) ? AMILAN_AGAIN : n;
}
int amilan_socket_udp(struct amilan_socket_api *api,uint16_t port)
{
    long r[14]={0};uint8_t a[16];int fd;
    if(!api->base)return -1;
    r[0]=2;r[1]=2;fd=setup(api,(int)call(api,L_SOCKET,r),0);if(fd<0)return -1;
    address(a,0,port);r[0]=fd;r[8]=(long)a;r[1]=16;
    if(!option(api,fd,0x20,1)||call(api,L_BIND,r)<0){amilan_socket_close(api,fd);return -1;}
    return fd;
}
int amilan_socket_sendto(struct amilan_socket_api *api,int fd,const uint8_t ip[4],uint16_t port,const uint8_t *buf,unsigned size)
{
    long r[14]={0};uint8_t a[16];int n;
    if(!api->base)return -1;
    address(a,ip,port);r[0]=fd;r[8]=(long)buf;r[1]=size;r[9]=(long)a;r[3]=16;
    n=(int)call(api,L_SENDTO,r);return n<0&&would_block(api)?AMILAN_AGAIN:n;
}
int amilan_socket_recvfrom(struct amilan_socket_api *api,int fd,uint8_t ip[4],uint16_t *port,uint8_t *buf,unsigned size)
{
    long r[14]={0},len=16;uint8_t a[16]={0};int n;
    if(!api->base)return -1;
    r[0]=fd;r[8]=(long)buf;r[1]=size;r[9]=(long)a;r[10]=(long)&len;
    n=(int)call(api,L_RECVFROM,r);if(n<0)return would_block(api)?AMILAN_AGAIN:-1;
    if(len!=16||a[1]!=2)return -1;
    for(int i=0;i<4;i++)ip[i]=a[4+i];
    *port=amilan_get16(a+2);return n;
}
static int amilan_if_ipv4_record(const uint8_t *entries,int at,int len)
{
    /* An ifreq record is useful to us when its sockaddr is AF_INET.  The
     * interface name check also keeps padding from looking like a record. */
    return at>=0&&at+24<=len&&entries[at]&&entries[at+17]==2;
}
static int amilan_if_stride(const uint8_t *entries,int at,int len)
{
    const uint8_t *e=entries+at;
    int stride=16+(e[16]>16?e[16]:16);
    if(stride<32)stride=32;
    /* Native AmiTCP-style records are normally 32 bytes.  Amiberry's POSIX
     * bsdsocket bridge copies Linux struct ifreq records into the guest
     * buffer; on 64-bit Linux those records are 40 bytes and its translated
     * sockaddr has sa_len=0.  Detect the next AF_INET record rather than
     * hard-coding 40 bytes for real Amiga stacks. */
    if(e[16]==0&&stride==32&&amilan_if_ipv4_record(entries,at+40,len))return 40;
    return stride;
}
int amilan_socket_interfaces(struct amilan_socket_api *api,int fd,struct amilan_interface out[AMILAN_INTERFACES])
{
    /* AmiTCP v4 ifconf: 32-bit size + pointer; ifreq: name[16] + sockaddr.
     * Amiberry/Linux may return 40-byte native Linux ifreq records instead.
     * Fixed scratch space, no gethostbyname/DNS or unbounded allocation. */
    struct {int32_t len;uint8_t *buf;} c;
    uint8_t entries[1024],req[32];long r[14]={0};int at=0,n=0,i,j,stride,flags,brd_ok;
    if(!api->base)return 0;
    c.len=sizeof(entries);c.buf=entries;r[0]=fd;r[1]=0xc0086924UL;r[8]=(long)&c;
    if(call(api,L_IOCTL,r)<0||c.len<0||c.len>(int)sizeof(entries))return 0;
    while(at+32<=c.len&&n<AMILAN_INTERFACES){
        const uint8_t *e=entries+at;stride=amilan_if_stride(entries,at,c.len);if(at+32>c.len)break;at+=stride;
        if(e[17]!=2||!e[20]||e[20]==127||e[20]>=224)continue;
        for(j=0;j<n;j++){for(i=0;i<4&&out[j].ip[i]==e[20+i];i++);if(i==4)break;}if(j<n)continue;
        for(i=0;i<32;i++)req[i]=e[i];
        r[1]=0xc0206911UL;r[8]=(long)req;
        if(call(api,L_IOCTL,r)<0)continue;
        flags=amilan_get16(req+16);if(!(flags&1))continue;
        for(i=0;i<4;i++){out[n].ip[i]=e[20+i];out[n].broadcast[i]=0;}
        if(flags&2){
            brd_ok=0;
            for(i=0;i<32;i++)req[i]=e[i];
            r[1]=0xc0206923UL;
            if(call(api,L_IOCTL,r)==0&&req[17]==2)brd_ok=1;
            /* Amiberry's POSIX bsdsocket layer exposes its translated
             * SIOCGIFBRDADDR using this 16-byte sockaddr request value. */
            if(!brd_ok){
                for(i=0;i<32;i++)req[i]=e[i];
                r[1]=0x80106925UL;
                if(call(api,L_IOCTL,r)==0&&req[17]==2)brd_ok=1;
            }
            if(brd_ok)for(i=0;i<4;i++)out[n].broadcast[i]=req[20+i];
            else for(i=0;i<4;i++)out[n].broadcast[i]=255;
        }
        n++;
    }
    return n;
}


int amilan_socket_peer(struct amilan_socket_api *api,int fd,uint8_t ip[4])
{
    uint8_t a[16]={0}; long len=16,r[14]={0};
    r[0]=fd;r[8]=(long)a;r[9]=(long)&len;
    if(call(api,-108,r)<0||len!=16||a[1]!=2)return 0;
    for(int i=0;i<4;i++)ip[i]=a[4+i];return 1;
}
void amilan_socket_nodelay(struct amilan_socket_api *api,int fd,int enabled)
{ long value=enabled?1:0,r[14]={0};r[0]=fd;r[1]=6;r[2]=1;r[8]=(long)&value;r[3]=4;call(api,L_SETOPT,r); }
void amilan_socket_finish(struct amilan_socket_api *api,int fd)
{ long r[14]={0};r[0]=fd;r[1]=1;call(api,-84,r); }
