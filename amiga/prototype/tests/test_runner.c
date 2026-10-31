/* SPDX-License-Identifier: GPL-3.0-only */
#include "runner.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __m68k__
/* The cross libc's calloc uses a 68020 64-bit multiply which a bare 68060
 * traps on. Keep test allocations out of that library routine; the Amiga
 * executable uses AllocMem, not libc. */
static uint8_t allocation_arena[2 * 1024 * 1024];
static size_t allocation_used;
static void *test_calloc(size_t n, size_t size) {
    size_t bytes = n * size;
    void *p;
    assert(allocation_used + bytes <= sizeof(allocation_arena));
    p = allocation_arena + allocation_used;
    allocation_used += (bytes + 3) & ~(size_t)3;
    memset(p, 0, bytes);
    return p;
}
#define calloc test_calloc
#define free(p) ((void)(p))
#endif

void *platform_alloc(uint32_t n) { return calloc(1,n); }
void platform_free(void *p,uint32_t n) { (void)n; free(p); }
void platform_log(const char *s) { (void)s; }
int platform_load(const char *p,uint8_t **o,uint32_t *s) { (void)p; *o=NULL; *s=0; return 0; }
int platform_open(const Options *o) { (void)o; return 0; }
void platform_close(void) {}
void platform_palette(const uint32_t p[256]) { (void)p; }
int platform_input(void) { return KEY_QUIT; }
void platform_wait(void) {}
uint32_t platform_ticks(void) { return 0; }
uint32_t platform_frequency(void) { return 1000000; }
void platform_present(const Canvas *c,uint8_t *d) { (void)c; (void)d; }
int platform_write_log(const char *s) { (void)s; return 0; }

static void put16(uint8_t *p,unsigned n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); }
static void put32(uint8_t *p,uint32_t n) { put16(p,n); put16(p+2,n>>16); }
static uint32_t checksum(const void *p,size_t n) {
    const uint8_t *b=p; uint32_t h=2166136261u;
    while (n--) h=(h^*b++)*16777619u;
    return h;
}
static void test_g1(void) {
    uint8_t file[128]={0}, guarded[8*8+32], before[sizeof(guarded)];
    G1 g; Sprite s; Canvas c={guarded+16,8,8,1,7};
    int x,y,i;
    put32(file,1); put32(file+4,24);
    put16(file+12,4); put16(file+14,3); put16(file+16,(uint16_t)-1); put16(file+18,(uint16_t)-2); put16(file+20,4);
    put16(file+24,6); put16(file+26,12); put16(file+28,17);
    /* 3 rows: full run, middle run, two chunks. */
    file[30]=0x84; file[31]=0; file[32]=10; file[33]=11; file[34]=12; file[35]=13;
    file[36]=0x82; file[37]=1; file[38]=21; file[39]=22;
    /* Offset deliberately allows unused padding. */
    file[41]=1; file[42]=0; file[43]=30; file[44]=0x81; file[45]=3; file[46]=33;
    assert(g1_open(&g,file,48)); assert(g1_sprite(&g,0,&s)); assert(s.x==-1 && s.y==-2 && s.width==4);
    for (y=-4;y<12;y++) for (x=-4;x<12;x++) {
        uint8_t expected[64]; memset(guarded,0xa5,sizeof(guarded)); memset(c.pixels,0,64); memset(expected,0,64);
        for (i=0;i<12;i++) {
            int px=i%4, py=i/4, dx=x-1+px, dy=y-2+py; uint8_t colour=0;
            if (py==0) colour=(uint8_t)(10+px);
            if (py==1 && (px==1 || px==2)) colour=(uint8_t)(20+px);
            if (py==2 && (px==0 || px==3)) colour=(uint8_t)(30+px);
            if (colour && dx>=0 && dx<8 && dy>=1 && dy<7) expected[dy*8+dx]=colour;
        }
        assert(g1_draw(&g,0,&c,x,y,0)); assert(!memcmp(c.pixels,expected,64));
        for (i=0;i<16;i++) assert(guarded[i]==0xa5 && guarded[80+i]==0xa5);
    }
    memset(guarded,0x55,sizeof(guarded)); memcpy(before,guarded,sizeof(before));
    file[44]=0x7f; /* invalid: run overflows image/data, no end-of-line */
    assert(!g1_draw(&g,0,&c,2,3,0)); assert(!memcmp(before,guarded,sizeof(before)));
    file[44]=0x81;
    assert(!g1_open(&g,file,7)); assert(!g1_open(&g,file,47));
    put32(file,0xffffffffu); assert(!g1_open(&g,file,48));
    put32(file,1); put32(file+4,0xffffffffu); assert(!g1_open(&g,file,48));
    /* Bitmap with transparent zero and primary remap. */
    put32(file+4,12); put16(file+20,1); put16(file+16,0); put16(file+18,0);
    memset(file+24,0,12); file[24]=243; file[25]=254; file[27]=4;
    assert(g1_open(&g,file,36)); memset(c.pixels,19,64);
    assert(g1_draw(&g,0,&c,1,1,22)); assert(c.pixels[9]==22 && c.pixels[10]==33 && c.pixels[11]==19 && c.pixels[12]==4);
    put32(file+8,0xffffffffu); assert(!g1_sprite(&g,0,&s));
}

static void test_palette(void) {
    uint8_t *file=calloc(1,8+1533*16+6); G1 g; uint32_t pal[256]; uint8_t *e=file+8+1532*16;
    assert(file); put32(file,1533); put32(file+4,6);
    put16(e+4,2); put16(e+8,10); put16(e+12,8);
    file[8+1533*16]=1; file[8+1533*16+1]=2; file[8+1533*16+2]=3;
    assert(g1_open(&g,file,8+1533*16+6)); g1_palette(&g,pal); assert(pal[10]==0x030201 && pal[11]==0);
    put16(e+4,257); g1_palette(&g,pal); assert(pal[10]!=0x030201);
    free(file);
}
static void test_options(void) {
    Options o; const char *args="LOW G1=\"Work:Games/RCT 2/Data/g1.dat\" MODEID=0x50011000 BENCH";
    options_init(&o); assert(options_parse(&o,args,(int)strlen(args)));
    assert(o.width==320 && o.height==240 && o.bench && o.mode==0x50011000u && !strcmp(o.g1_path,"Work:Games/RCT 2/Data/g1.dat"));
    assert(!options_parse(&o,"MODEID=0xGG",11)); assert(!options_parse(&o,"G1=\"broken",10));
    assert(!options_parse(&o,"UNKNOWN",7)); assert(!options_parse(&o,"MODEID=123456789",16));
    assert(!options_parse(&o,"G1=",3));
}
static void test_upstream_fixture(void) {
    static uint8_t file[9000], pixels[128*128];
    G1 g;
    Canvas canvas={pixels,128,128,0,128};
    FILE *f=fopen("../../test/tests/testdata/sprites/example.dat","rb");
    size_t n;
    assert(f); n=fread(file,1,sizeof(file),f); assert(!ferror(f)); assert(!fclose(f));
    assert(g1_open(&g,file,(uint32_t)n));
    assert(g1_draw(&g,0,&canvas,0,0,0));
    /* Independent byte-by-byte decode of upstream's real 128x128 RLE fixture. */
    assert(checksum(pixels,sizeof(pixels))==0x4e310b7au);
}
static uint32_t test_scene(int w,int h) {
    static Runner incremental,reference;
    size_t bytes=(size_t)w*h;
    uint8_t *a=calloc(1,bytes), *b=calloc(1,bytes), *backa=calloc(1,bytes), *backb=calloc(1,bytes), *old=calloc(1,bytes);
    unsigned frame; uint32_t hash;
    assert(a&&b&&backa&&backb&&old);
    runner_init(&incremental,a,backa,w,h); runner_init(&reference,b,backb,w,h);
    for (frame=0;frame<140;frame++) {
        int key=KEY_NONE; size_t i;
        if (frame==10) key=KEY_MORE;
        if (frame==20) key=KEY_RIGHT;
        if (frame==30) key=KEY_DOWN;
        if (frame==40) key=KEY_PAUSE;
        if (frame==50) key=KEY_PAUSE;
        if (frame==60) key=KEY_HOME;
        if (frame==70) key=KEY_MORE;
        if (frame==80) key=KEY_MORE;
        if (frame==90) key=KEY_LESS;
        if (frame==100) key=KEY_GRID;
        if (frame==110 || frame==120) key=KEY_VIEW;
        runner_input(&incremental,key); runner_input(&reference,key);
        memcpy(old,a,bytes);
        runner_step(&incremental,40000,1000000); runner_step(&reference,40000,1000000);
        runner_render(&incremental); reference.full_redraw=1; runner_render(&reference);
        assert(!memcmp(a,b,bytes));
        /* Every changed pixel must be included in the regions copied to RTG. */
        for (i=0;i<bytes;i++) if (a[i]!=old[i]) {
            unsigned x=(unsigned)(i%(size_t)w), y=(unsigned)(i/(size_t)w);
            assert(incremental.dirty[(y/TILE_H)*(unsigned)(w/TILE_W)+x/TILE_W]);
        }
        memset(incremental.dirty,0,sizeof(incremental.dirty));
    }
    hash=checksum(a,bytes);
    free(a);free(b);free(backa);free(backb);free(old);return hash;
}
static void putle(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static void map_hash(uint8_t *p,unsigned size){unsigned i;uint32_t h=2166136261u;for(i=32;i<size;i++)h=(h^p[i])*16777619u;putle(p+20,h);}
static void test_park_map(void) {
    uint8_t blob[32+26*4+26*8]={0},bad[sizeof(blob)];
    static uint8_t a[640*480],b[640*480],ba[640*480],bb[640*480],old[640*480];
    static Runner r,reference;
    ParkMap parsed;
    unsigned cell,n=0,k,res;
    memcpy(blob,"AMAP",4);blob[4]=1;blob[6]=5;putle(blob+8,25);putle(blob+12,26);putle(blob+16,0x12345678);
    for(cell=0;cell<25;cell++) {
        uint8_t *record;
        putle(blob+32+cell*4,n);
        record=blob+32+26*4+n++*8;record[1]=2;
        if(cell==1)record[1]=4;
        if(cell==2)record[3]=0x10;
        if(cell==3)record[5]=8;
        if(cell==12){record=blob+32+26*4+n++*8;record[0]=1;record[1]=2;record[3]=0xff;record[4]=4;}
    }
    putle(blob+32+25*4,n);map_hash(blob,sizeof(blob));
    assert(park_map_open(&parsed,blob,sizeof(blob))&&parsed.side==5&&parsed.elements==26);
    assert(!park_map_open(&parsed,NULL,sizeof(blob))&&!park_map_open(&parsed,blob,sizeof(blob)-1));
    for(k=0;k<7;k++) {
        memcpy(bad,blob,sizeof(blob));
        switch(k){case 0:bad[0]='X';break;case 1:bad[4]=4;break;case 2:bad[6]=0;break;
        case 3:putle(bad+32+4,999);break;case 4:bad[32+26*4]=9;break;
        case 5:bad[32+26*4+3]=0xff;break;default:bad[32+26*4+2]=0x80;break;}
        map_hash(bad,sizeof(bad));assert(!park_map_open(&parsed,bad,sizeof(bad)));
    }
    memcpy(bad,blob,sizeof(blob));bad[sizeof(bad)-1]^=1;assert(!park_map_open(&parsed,bad,sizeof(bad)));
    for(res=0;res<2;res++) {
        int w=res?320:640,h=res?240:480;unsigned frame;size_t bytes=(size_t)w*h;
        runner_init(&r,a,ba,w,h);runner_init(&reference,b,bb,w,h);
        assert(runner_set_map(&r,blob,sizeof(blob))&&runner_set_map(&reference,blob,sizeof(blob)));
        for(frame=0;frame<50;frame++) {
            int key=KEY_NONE;
            if(frame%5==0)key=KEY_PREV;if(frame%7==0)key=KEY_NEXT;
            if(frame%11==0)key=KEY_HOME;if(frame%3==0)key=KEY_GRID;
            if(frame==10){r.pan_x=reference.pan_x=-9000;r.pan_y=reference.pan_y=6000;key=KEY_LEFT;}
            if(frame==20){r.pan_x=reference.pan_x=9000;r.pan_y=reference.pan_y=-6000;key=KEY_RIGHT;}
            if(frame==30)key=KEY_HOME;
            runner_input(&r,key);runner_input(&reference,key);runner_input(&r,KEY_MORE);
            memcpy(old,a,bytes);runner_render(&r);reference.full_redraw=1;runner_render(&reference);
            assert(!memcmp(a,b,bytes)&&r.guests==0);
            for(k=0;k<bytes;k++)if(old[k]!=a[k])assert(r.dirty[((k/w)/TILE_H)*(w/TILE_W)+(k%w)/TILE_W]);
            memset(r.dirty,0,sizeof(r.dirty));
        }
        /* Avoid the cross libc's decimal formatter: its _itoa_word uses a
         * 68020 64-bit multiply, unavailable on a bare 68060. */
        printf("Imported map parsing/zoom/pan/clipping/dirty rectangles: PASS %s %08lx\n",res?"320x240":"640x480",(unsigned long)checksum(a,bytes));
    }
}
static void test_saved_guests(void) {
    enum { BASE=32+26*4+25*8, SIZE=BASE+2*PARK_MAP_GUEST_RECORD };
    uint8_t map[SIZE]={0},bad[SIZE];
    static uint8_t pixels[640*480],back[640*480],without[640*480],terrain_back[640*480];
    static Runner r,terrain;
    ParkMap parsed;
    unsigned i;
    memcpy(map,"AMAP",4);map[4]=2;map[6]=5;putle(map+8,25);putle(map+12,25);putle(map+24,2);
    for(i=0;i<=25;i++)putle(map+32+i*4,i);
    for(i=0;i<25;i++)map[32+26*4+i*8+1]=2;
    // Two depth-ordered saved positions; the second is on a ride and hidden.
    for(i=0;i<2;i++) {
        uint8_t *g=map+BASE+i*PARK_MAP_GUEST_RECORD;
        put16(g,i+1);put16(g+2,80+i*32);put16(g+4,80+i*32);put16(g+6,16);
        g[8]=8;g[9]=i?3:5;g[11]=21;g[12]=7;g[13]=15;
    }
    map_hash(map,SIZE);assert(park_map_open(&parsed,map,SIZE)&&parsed.guest_count==2);
    for(i=0;i<9;i++) {
        memcpy(bad,map,SIZE);
        switch(i) {
            case 0:putle(bad+24,10001);break;
            case 1:put16(bad+BASE+16,1);break;
            case 2:put16(bad+BASE+6,65535);break;
            case 3:put16(bad+BASE+2,160);break;
            case 4:put16(bad+BASE+16+2,0);put16(bad+BASE+16+4,0);break;
            case 5:bad[BASE+9]=24;break;
            case 6:bad[BASE+10]=2;break;
            case 7:bad[BASE+15]=1;break;
            default:bad[BASE+11]=32;break;
        }
        map_hash(bad,SIZE);assert(!park_map_open(&parsed,bad,SIZE));
    }
    runner_init(&r,pixels,back,640,480);assert(runner_set_map(&r,map,SIZE));runner_render(&r);
    assert(pixels[234*640+320]==3); // exact saved-position marker head
    assert(pixels[250*640+320]!=3); // on-ride position is not drawn as a walker
    memcpy(without,pixels,sizeof(pixels));r.phase+=137;r.full_redraw=1;runner_render(&r);
    assert(!memcmp(pixels,without,sizeof(pixels))); // no invented movement/animation
    runner_input(&r,KEY_PAUSE);runner_render(&r);assert(!r.map_guests_visible&&pixels[234*640+320]!=3);
    // Hiding snapshots exactly restores the v1 terrain viewport.
    map[4]=1;putle(map+24,0);map_hash(map,BASE);
    runner_init(&terrain,without,terrain_back,640,480);assert(runner_set_map(&terrain,map,BASE));runner_render(&terrain);
    assert(!memcmp(pixels+48*640,without+48*640,(480-96)*640));
    runner_input(&r,KEY_PAUSE);runner_render(&r);assert(pixels[234*640+320]==3);
    puts("Saved guest bounds/order/identity/state/toggle/static-position checks: PASS");
}
static void test_saved_rides(void) {
    enum { BASE=32+26*4, SIZE=BASE+28*8+2*PARK_MAP_RIDE_RECORD };
    uint8_t map[SIZE]={0},bad[SIZE];
    static uint8_t pixels[640*480],back[640*480],snapshot[640*480];static Runner r;
    ParkMap parsed;unsigned cell,n=0,i;
    memcpy(map,"AMAP",4);map[4]=3;map[6]=5;putle(map+8,25);putle(map+12,28);putle(map+28,2);
    for(cell=0;cell<25;cell++) {
        uint8_t *p;putle(map+32+cell*4,n);p=map+BASE+n++*8;p[1]=2;
        if(cell==12||cell==18||cell==24){p=map+BASE+n++*8;p[0]=2;p[1]=2;p[3]=2;p[4]=cell==12?1:0;p[7]=cell==12?2:cell==18?7:255;}
    }
    putle(map+32+25*4,n);
    memcpy(map+BASE+28*8,(uint8_t[]){2,4,9,1,2,2,2,1,7,43,9,2,4,3,3,0},16);
    map_hash(map,SIZE);assert(park_map_open(&parsed,map,SIZE)&&parsed.ride_count==2&&parsed.track_count==3);
    for(i=0;i<8;i++) {
        memcpy(bad,map,SIZE);
        switch(i) {
            case 0:putle(bad+28,256);break;
            case 1:bad[BASE+28*8+8]=2;break;
            case 2:bad[BASE+28*8+1]=255;break;
            case 3:bad[BASE+28*8+3]=4;break;
            case 4:bad[BASE+28*8+7]=5;break;
            case 5:bad[BASE+13*8+7]=6;break; // unresolved track ride
            case 6:bad[BASE+13*8+3]=4;break; // bad direction
            default:bad[4]=2;putle(bad+28,0);break;
        }
        map_hash(bad,SIZE);assert(!park_map_open(&parsed,bad,SIZE));
    }
    runner_init(&r,pixels,back,640,480);assert(runner_set_map(&r,map,SIZE));runner_render(&r);
    assert(pixels[232*640+320]==5); // exact saved track tile outline, all rides
    memcpy(snapshot,pixels,sizeof(pixels));
    runner_input(&r,KEY_MORE);runner_render(&r);
    assert(r.map_ride_selected==0&&pixels[232*640+320]==4); // selected ride 2, gold
    runner_input(&r,KEY_MORE);runner_render(&r);assert(r.map_ride_selected==1); // sparse ride ID 7
    runner_input(&r,KEY_MORE);runner_input(&r,KEY_HOME);runner_render(&r);
    assert(r.map_ride_selected==-1&&!memcmp(snapshot,pixels,sizeof(pixels)));
    runner_input(&r,KEY_LESS);assert(r.map_ride_selected==1);runner_input(&r,KEY_MORE);runner_input(&r,KEY_HOME);
    runner_input(&r,KEY_VIEW);runner_render(&r);assert(!r.map_tracks_visible&&pixels[232*640+320]!=5);
    runner_input(&r,KEY_VIEW);runner_render(&r);assert(!memcmp(snapshot,pixels,sizeof(pixels)));
    puts("Ride catalogue/reference/direction/selection/track-toggle checks: PASS");
}
int main(void) {
    uint32_t a,b;
    test_g1(); test_palette(); test_options(); test_upstream_fixture();
    a=test_scene(640,480); b=test_scene(320,240);
    test_park_map(); test_saved_guests(); test_saved_rides();
    printf("G1 clipping/malformed files/palettes/options/dirty redraw: PASS\n640x480 %08lx 320x240 %08lx\n",(unsigned long)a,(unsigned long)b);
    return 0;
}
