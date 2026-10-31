/* SPDX-License-Identifier: GPL-3.0-only
 * AmigaOS 3.1 RTG backend. The OS and its mouse pointer remain running.
 * Intuition/graphics.library calling conventions follow MinTee Golf's
 * existing Amiga RTG backend. No SDL, FPU, takeover or native bitplanes.
 */
#include "runner.h"

extern void *SysBase, *DOSBase;
extern const char *cli_args;
extern long cli_arglen;
extern Options runner_options;
void *os_alloc(unsigned long size, unsigned long flags);
void os_free(void *p, unsigned long size);
void *os_openlib(const char *name, long version);
void os_closelib(void *base);
void os_print(const char *s, long length);
long os_call(void *base, long lvo, const long *registers);
long dos_open(const char *name, long mode);
void dos_close(long handle);
long dos_read(long handle, void *buf, long length);
long dos_write(long handle, const void *buf, long length);

enum { D0, D1, D2, D3, D4, D5, D6, D7, A0, A1, A2, A3, A4, A5 };
#define MEMF_FAST 4u
#define MEMF_CLEAR 65536u
#define INVALID_ID 0xffffffffu
static void *gfx, *intuition, *screen, *window, *timer;
static uint32_t rastport[25], timer_request[12], frequency;
static uint32_t rgb32[770];
static uint8_t held[128];
static int timer_opened, width, height, offset_x, offset_y, screen_w, screen_h;
static uint32_t repeat_at;

void *memset(void *dst, int value, size_t n) {
    uint8_t *p = dst;
    uint32_t v = (uint8_t)value; v |= v << 8; v |= v << 16;
    while (n && ((uintptr_t)p & 3)) { *p++ = (uint8_t)value; n--; }
    while (n >= 16) { uint32_t *q = (uint32_t *)p; q[0]=q[1]=q[2]=q[3]=v; p+=16; n-=16; }
    while (n--) *p++ = (uint8_t)value;
    return dst;
}
void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *p = dst; const uint8_t *q = src;
    if (!(((uintptr_t)p | (uintptr_t)q) & 3)) while (n >= 16) {
        uint32_t *a = (uint32_t *)p; const uint32_t *b = (const uint32_t *)q;
        a[0]=b[0]; a[1]=b[1]; a[2]=b[2]; a[3]=b[3]; p+=16; q+=16; n-=16;
    }
    while (n--) *p++ = *q++;
    return dst;
}
static uint16_t get16(const void *base, int off) { uint16_t n; memcpy(&n, (const uint8_t *)base + off, 2); return n; }
static uint32_t get32(const void *base, int off) { uint32_t n; memcpy(&n, (const uint8_t *)base + off, 4); return n; }
static long call(void *base, long lvo, long r[14]) { return os_call(base, lvo, r); }
static void clear(long r[14]) { memset(r, 0, 14 * sizeof(long)); }
void *platform_alloc(uint32_t size) { return os_alloc(size, MEMF_FAST | MEMF_CLEAR); }
void platform_free(void *p, uint32_t size) { if (p) os_free(p, size); }
void platform_log(const char *text) { long n = 0; while (text[n]) n++; os_print(text, n); }

int platform_load(const char *path, uint8_t **out, uint32_t *size) {
    long r[14], fh = dos_open(path, 1005), end, done = 0;
    uint8_t *p;
    *out = 0; *size = 0;
    if (!fh) return 0;
    clear(r); r[D1] = fh; r[D2] = 0; r[D3] = 1;
    if (call(DOSBase, -66, r) == -1) { dos_close(fh); return 0; } /* Seek END */
    r[D3] = 0; end = call(DOSBase, -66, r); /* current position, zero offset */
    if (end < 8 || (uint32_t)end > MAX_G1_BYTES) { dos_close(fh); return 0; }
    r[D3] = -1;
    if (call(DOSBase, -66, r) == -1) { dos_close(fh); return 0; }
    p = platform_alloc((uint32_t)end);
    if (!p) { dos_close(fh); return 0; }
    while (done < end) {
        long n = dos_read(fh, p + done, end - done);
        if (n <= 0) { platform_free(p, (uint32_t)end); dos_close(fh); return 0; }
        done += n;
    }
    dos_close(fh); *out = p; *size = (uint32_t)end; return 1;
}
int platform_write_log(const char *text) {
    long fh = dos_open("PROGDIR:OpenRCT2Runner.bench", 1006), n = 0, done = 0;
    if (!fh) return 0;
    while (text[n]) n++;
    while (done < n) {
        long result = dos_write(fh, text + done, n - done);
        if (result <= 0) { dos_close(fh); return 0; }
        done += result;
    }
    dos_close(fh); return 1;
}

static int display_data(uint32_t id, uint32_t tag, void *data, int bytes) {
    long r[14]; clear(r);
    r[A1] = (long)data; r[D0] = bytes; r[D1] = (long)tag; r[D2] = (long)id;
    return call(gfx, -756, r) != 0;
}
static int suitable(uint32_t id, int *w, int *h) {
    uint32_t info[14], dims[22];
    uint32_t monitor = id & 0xffff1000u;
    if (!display_data(id, 0x80000000u, info, sizeof(info)) || get16(info, 16)) return 0;
    if (!(get32(info, 18) & 0x80000000u) && monitor <= 0x000a9000u) return 0;
    if (!display_data(id, 0x80001000u, dims, sizeof(dims)) || get16(dims, 16) != 8) return 0;
    *w = (int16_t)get16(dims, 30) - (int16_t)get16(dims, 26) + 1;
    *h = (int16_t)get16(dims, 32) - (int16_t)get16(dims, 28) + 1;
    return *w >= width && *h >= height;
}
static uint32_t find_mode(uint32_t wanted) {
    uint32_t id = INVALID_ID, best = INVALID_ID, area = 0xffffffffu;
    long r[14]; int w, h;
    if (wanted != INVALID_ID) {
        if (!suitable(wanted, &screen_w, &screen_h)) return INVALID_ID;
        return wanted;
    }
    for (;;) {
        clear(r); r[D0] = (long)id; id = (uint32_t)call(gfx, -732, r);
        if (id == INVALID_ID) break;
        if (suitable(id, &w, &h) && (uint32_t)w * (uint32_t)h < area) {
            best = id; area = (uint32_t)w * (uint32_t)h; screen_w = w; screen_h = h;
        }
    }
    return best;
}

int platform_open(const Options *o) {
    uint32_t tags[24], id; long r[14];
    width = o->width; height = o->height;
    gfx = os_openlib("graphics.library", 40);
    intuition = os_openlib("intuition.library", 39);
    if (!gfx || !intuition) { platform_log("Needs AmigaOS 3.1 or newer.\n"); return 0; }
    /* Only ReadEClock is used: no asynchronous IO, hence no reply port. */
    ((uint16_t *)timer_request)[9] = sizeof(timer_request);
    clear(r); r[A0] = (long)"timer.device"; r[D0] = 1; r[A1] = (long)timer_request;
    if (call(SysBase, -444, r)) { platform_log("Cannot open timer.device.\n"); return 0; }
    timer_opened = 1; timer = (void *)(uintptr_t)timer_request[5];
    { uint32_t stamp[2]; clear(r); r[A0] = (long)stamp; frequency = (uint32_t)call(timer, -60, r); }
    id = find_mode(o->mode);
    if (id == INVALID_ID) { platform_log("No suitable 8-bit RTG mode. Add 640x480 256-colour mode in P96/CGX, or use LOW for 320x240.\n"); return 0; }
    tags[0]=0x80000023u; tags[1]=(uint32_t)screen_w; /* SA_Width */
    tags[2]=0x80000024u; tags[3]=(uint32_t)screen_h;
    tags[4]=0x80000025u; tags[5]=8;
    tags[6]=0x80000032u; tags[7]=id;
    tags[8]=0x8000002du; tags[9]=15;
    tags[10]=0x80000038u; tags[11]=1;
    tags[12]=0x80000036u; tags[13]=0;
    tags[14]=0x8000003eu; tags[15]=0;
    tags[16]=0x80000028u; tags[17]=(uint32_t)"OpenRCT2 graphics runner";
    tags[18]=0;
    clear(r); r[A1]=(long)tags; screen=(void *)call(intuition, -612, r);
    if (!screen) { platform_log("Could not open RTG screen.\n"); return 0; }
    tags[0]=0x80000070u; tags[1]=(uint32_t)screen;
    tags[2]=0x80000064u; tags[3]=0;
    tags[4]=0x80000065u; tags[5]=0;
    tags[6]=0x80000066u; tags[7]=(uint32_t)screen_w;
    tags[8]=0x80000067u; tags[9]=(uint32_t)screen_h;
    tags[10]=0x8000006au; tags[11]=0x00080400u; /* RAWKEY | INACTIVEWINDOW */
    tags[12]=0x8000006bu; tags[13]=0x00031940u; /* backdrop, borderless, active, RMB trap, simple refresh */
    tags[14]=0;
    clear(r); r[A1]=(long)tags; window=(void *)call(intuition, -606, r);
    if (!window) { platform_log("Could not open input window.\n"); return 0; }
    clear(r); r[A1]=(long)rastport; call(gfx, -198, r);
    rastport[1] = get32(screen, 88); /* Screen.RastPort.BitMap */
    offset_x=(screen_w-width)/2; offset_y=(screen_h-height)/2;
    return 1;
}
void platform_palette(const uint32_t rgb[256]) {
    int i; uint32_t *p=rgb32; long r[14];
    *p++=256u<<16;
    for (i=0;i<256;i++) {
        *p++=((rgb[i]>>16)&255u)*0x01010101u;
        *p++=((rgb[i]>>8)&255u)*0x01010101u;
        *p++=(rgb[i]&255u)*0x01010101u;
    }
    *p=0; clear(r); r[A0]=(long)((uint8_t *)screen+44); r[A1]=(long)rgb32; call(gfx,-882,r);
}
static int key_code(unsigned code) {
    switch (code) {
        case 0x45: return KEY_QUIT;
        case 0x4f: return KEY_LEFT; case 0x4e: return KEY_RIGHT;
        case 0x4c: return KEY_UP; case 0x4d: return KEY_DOWN;
        case 0x42: return KEY_VIEW; case 0x40: return KEY_PAUSE;
        case 0x1a: return KEY_PREV; case 0x1b: return KEY_NEXT;
        case 0x25: case 0x70: return KEY_HOME;
        case 0x0c: case 0x5e: return KEY_MORE;
        case 0x0b: case 0x4a: return KEY_LESS;
        case 0x24: return KEY_GRID; case 0x35: return KEY_BENCH;
        default: return KEY_NONE;
    }
}
int platform_input(void) {
    long r[14]; void *port;
    if (!window) return KEY_NONE;
    port=(void *)(uintptr_t)get32(window,86);
    for (;;) {
        uint8_t *msg; uint32_t klass; unsigned code; int key=KEY_NONE;
        clear(r); r[A0]=(long)port; msg=(uint8_t *)call(SysBase,-372,r);
        if (!msg) break;
        klass=get32(msg,20); code=get16(msg,24);
        if (klass==0x400u && code<256u) {
            unsigned raw=code&127u;
            if (!(code&128u) && !held[raw]) key=key_code(raw);
            held[raw]=(uint8_t)!(code&128u);
            if (key) repeat_at=platform_ticks();
        } else if (klass==0x80000u) memset(held,0,sizeof(held));
        clear(r); r[A1]=(long)msg; call(SysBase,-378,r);
        if (key) return key;
    }
    { /* Repeat navigation only; toggles never repeat. */
        static const uint8_t repeat_keys[]={0x4f,0x4e,0x4c,0x4d,0x1a,0x1b};
        uint32_t now=platform_ticks(); unsigned i;
        if (now-repeat_at >= frequency/8u) for (i=0;i<sizeof(repeat_keys);i++)
            if (held[repeat_keys[i]]) { repeat_at=now; return key_code(repeat_keys[i]); }
    }
    return KEY_NONE;
}
uint32_t platform_ticks(void) {
    uint32_t stamp[2]; long r[14];
    if (!timer) return 0;
    clear(r); r[A0]=(long)stamp; call(timer,-60,r); return stamp[1];
}
uint32_t platform_frequency(void) { return frequency; }
void platform_wait(void) { long r[14]; clear(r); call(gfx,-270,r); }
void platform_present(const Canvas *c, uint8_t *dirty) {
    int y, columns=c->width/TILE_W, rows=c->height/TILE_H;
    for (y=0;y<rows;y++) {
        int x=0;
        while (x<columns) {
            int begin; long r[14];
            while (x<columns && !dirty[y*columns+x]) x++;
            if (x==columns) break;
            begin=x;
            while (x<columns && dirty[y*columns+x]) dirty[y*columns+x++]=0;
            clear(r); r[A0]=(long)rastport;
            r[D0]=offset_x+begin*TILE_W; r[D1]=offset_y+y*TILE_H;
            r[D2]=offset_x+x*TILE_W-1; r[D3]=offset_y+(y+1)*TILE_H-1;
            r[A2]=(long)(c->pixels+y*TILE_H*c->width+begin*TILE_W);
            r[D4]=c->width; call(gfx,-1056,r);
        }
    }
}
void platform_close(void) {
    long r[14];
    if (window) {
        void *port=(void *)(uintptr_t)get32(window,86), *msg;
        do { clear(r); r[A0]=(long)port; msg=(void *)call(SysBase,-372,r);
             if (msg) { clear(r); r[A1]=(long)msg; call(SysBase,-378,r); } } while (msg);
        clear(r); r[A0]=(long)window; call(intuition,-72,r); window=0;
    }
    if (screen) { clear(r); r[A0]=(long)screen; call(intuition,-66,r); screen=0; }
    if (timer_opened) { clear(r); r[A1]=(long)timer_request; call(SysBase,-450,r); timer_opened=0; timer=0; }
    if (intuition) os_closelib(intuition); intuition=0;
    if (gfx) os_closelib(gfx); gfx=0;
}
__attribute__((used)) static const char version[]="$VER: OpenRCT2ParkView 0.9 (31.10.2026) 68060 RTG graphics prototype";
int main(void) {
    options_init(&runner_options);
    if (!options_parse(&runner_options,cli_args,(int)cli_arglen)) {
        platform_log("Usage: OpenRCT2Runner [MAP=\"ImportedMap.amap\"] [G1=\"path/Data/g1.dat\"] [LOW] [MODEID=0x...] [BENCH] [DIAGNOSTIC]\n"); return 20;
    }
    return runner_main();
}
