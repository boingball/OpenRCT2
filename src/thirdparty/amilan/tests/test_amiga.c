/* Mock verified AmiTCP v4 register ABI; not a real stack/hardware test. */
#include "amilan/amiga.h"
#include "amilan/socket.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int library,closes,fd=3,err,connect_pending,ready,fail_option,fail_ioctl;
static int fail_nodelay;
static int udp,iface_fail,iface_amiberry,brd_fail;
void *os_openlib(const char *name,long version)
{ assert(!strcmp(name,"bsdsocket.library")&&version==4);return library?(void *)1:0; }
void os_closelib(void *base) { assert(base==(void *)1);library=0; }
long os_call(void *base,long lvo,const long *r)
{
    assert(base==(void *)1);
    switch(lvo) {
    case -132: assert(!r[0]&&!r[1]&&!r[2]);return 0;
    case -30: assert(r[0]==2&&(r[1]==1||r[1]==2)&&!r[2]);udp=r[1]==2;return fd;
    case -114: {
        unsigned long request=(unsigned long)r[1];
        if(request==0x8004667eUL){assert(*(long *)r[8]==1);return fail_ioctl?-1:0;}
        if(iface_fail)return -1;
        if(request==0xc0086924UL){
            struct {int32_t len;uint8_t *buf;} *c=(void *)r[8];int stride=iface_amiberry?40:32;
            assert(c->len==1024);memset(c->buf,0,120);
            for(int i=0;i<3;i++){uint8_t *e=c->buf+stride*i;e[0]='0'+i;e[16]=iface_amiberry?0:16;e[17]=2;e[20]=i?192:127;e[21]=i?168:0;e[22]=i?1:0;e[23]=i?2:1;}
            c->len=stride*3;return 0;
        }
        uint8_t *b=(void *)r[8];assert(b[0]=='1');
        if(request==0xc0206911UL){b[16]=0;b[17]=3;return 0;}
        if(request==0xc0206923UL){
            if(iface_amiberry||brd_fail)return -1;
            b[16]=16;b[17]=2;b[20]=192;b[21]=168;b[22]=1;b[23]=255;return 0;
        }
        assert(request==0x80106925UL);
        if(brd_fail)return -1;
        assert(iface_amiberry);b[16]=0;b[17]=2;b[20]=192;b[21]=168;b[22]=1;b[23]=255;return 0;
    }
    case -90: assert((r[1]==0xffff||(r[1]==6&&r[2]==1))&&r[3]==4&&r[8]);return fail_option||(fail_nodelay&&r[1]==6)?-1:0;
    case -36: case -54: {
        const uint8_t *a=(const uint8_t *)r[8];
        assert(r[1]==16&&a[0]==16&&a[1]==2&&a[2]==0x63&&a[3]==(udp?0xe0:0xdf));
        if(lvo==-54) { assert(a[4]==192&&a[5]==168&&a[6]==1&&a[7]==2);return connect_pending?-1:0; }
        assert(!a[4]&&!a[5]&&!a[6]&&!a[7]);return 0;
    }
    case -42: assert(r[1]==AMILAN_CONNECTIONS);return 0;
    case -48: assert(!r[8]&&!r[9]);return fd;
    case -162: return err;
    case -120: closes++;return 0;
    case -126:
        assert(r[0]==4&&!r[8]&&*(uint32_t *)r[9]==8&&*(uint32_t *)r[10]==8);
        assert(!((long *)r[11])[0]&&!((long *)r[11])[1]&&!r[1]);return ready;
    case -96: assert(r[0]==3&&r[1]==0xffff&&r[2]==0x1007&&*(long *)r[9]==4);*(long *)r[8]=err;return 0;
    case -78: case -66: assert(r[0]==3&&r[1]==4&&!r[2]&&r[8]);return err?-1:4;
    case -60: {
        const uint8_t *a=(void *)r[9];assert(r[0]==3&&r[1]==4&&!r[2]&&r[3]==16&&r[8]);
        assert(a[0]==16&&a[1]==2&&a[2]==0x63&&a[3]==0xe0&&a[4]==192&&a[7]==2);return err?-1:4;
    }
    case -72: {
        uint8_t *a=(void *)r[9];assert(r[0]==3&&r[1]==4&&!r[2]&&r[8]&&*(long *)r[10]==16);
        a[0]=16;a[1]=2;a[2]=0x63;a[3]=0xe0;a[4]=192;a[5]=168;a[6]=1;a[7]=2;return err?-1:4;
    }
    default: assert(0);return -1;
    }
}
int main(void)
{
    struct amilan_socket_api api={0};uint8_t ip[4]={192,168,1,2},buf[4]={0};
    assert(!amilan_socket_open(&api));library=1;assert(amilan_socket_open(&api));assert(amilan_socket_open(&api));
    fail_nodelay=1;assert(amilan_socket_listen(&api,25567)==3);fail_nodelay=0;assert(amilan_socket_accept(&api,3)==3);
    connect_pending=1;err=36;assert(amilan_socket_connect(&api,ip,25567)==3);err=0;
    assert(!amilan_socket_connect_ready(&api,3));ready=1;assert(amilan_socket_connect_ready(&api,3)==1);
    err=61;assert(amilan_socket_connect_ready(&api,3)==-1);err=0;
    assert(amilan_socket_send(&api,3,buf,4)==4&&amilan_socket_recv(&api,3,buf,4)==4);
    err=35;assert(amilan_socket_send(&api,3,buf,4)==AMILAN_AGAIN&&amilan_socket_recv(&api,3,buf,4)==AMILAN_AGAIN);
    err=4;assert(amilan_socket_send(&api,3,buf,4)==AMILAN_AGAIN);err=61;assert(amilan_socket_recv(&api,3,buf,4)==-1);err=0;
    fail_option=1;assert(amilan_socket_listen(&api,25567)==-1&&closes==1);fail_option=0;
    fail_ioctl=1;assert(amilan_socket_listen(&api,25567)==-1&&closes==2);fail_ioctl=0;
    fd=32;assert(amilan_socket_listen(&api,25567)==-1&&closes==3);
    assert(amilan_socket_connect_ready(&api,32)==-1);
    fd=3;assert(amilan_socket_udp(&api,25568)==3);uint16_t port;uint8_t from[4];struct amilan_interface interfaces[AMILAN_INTERFACES];
    assert(amilan_socket_sendto(&api,3,ip,25568,buf,4)==4&&amilan_socket_recvfrom(&api,3,from,&port,buf,4)==4&&port==25568&&!memcmp(from,ip,4));
    assert(amilan_socket_interfaces(&api,3,interfaces)==1&&!memcmp(interfaces[0].ip,ip,4)&&interfaces[0].broadcast[3]==255);
    iface_amiberry=1;memset(interfaces,0,sizeof(interfaces));
    assert(amilan_socket_interfaces(&api,3,interfaces)==1&&!memcmp(interfaces[0].ip,ip,4)&&interfaces[0].broadcast[0]==192&&interfaces[0].broadcast[3]==255);
    iface_amiberry=0;brd_fail=1;memset(interfaces,0,sizeof(interfaces));
    assert(amilan_socket_interfaces(&api,3,interfaces)==1&&!memcmp(interfaces[0].ip,ip,4)&&interfaces[0].broadcast[0]==255&&interfaces[0].broadcast[3]==255);
    brd_fail=0;iface_fail=1;assert(!amilan_socket_interfaces(&api,3,interfaces));iface_fail=0;
    err=35;assert(amilan_socket_recvfrom(&api,3,from,&port,buf,4)==AMILAN_AGAIN&&amilan_socket_sendto(&api,3,ip,25568,buf,4)==AMILAN_AGAIN);err=0;
    fail_option=1;assert(amilan_socket_udp(&api,25568)==-1);fail_option=0;
    amilan_socket_shutdown(&api);assert(!api.base);amilan_socket_shutdown(&api);
    assert(amilan_socket_listen(&api,25567)==-1);
    puts("Amiga TCP/UDP socket ABI, bounded interface enumeration, errors and lazy-open tests passed");return 0;
}

