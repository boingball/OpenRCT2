/* SPDX-License-Identifier: GPL-3.0-only
 * Bounds-checked C adaptation of OpenRCT2's G1Element.h and
 * drawing/Drawing.Sprite.RLE.cpp (OpenRCT2 developers, 2014-2026).
 * Byte reads are deliberate: 68k is big endian and sprites can be unaligned.
 * Supports the original RCT2 G1 file, bitmap/RLE and one primary remap.
 */
#include "runner.h"
#include "guest_palette.h"

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)le16(p) | (uint32_t)le16(p + 2) << 16;
}

int g1_open(G1 *g, const void *bytes, uint32_t size) {
    const uint8_t *p = bytes;
    uint32_t n, data_size, start;
    memset(g, 0, sizeof(*g));
    if (!p || size < 8 || size > MAX_G1_BYTES) return 0;
    n = le32(p); data_size = le32(p + 4);
    if (!n || n > MAX_G1_ENTRIES) return 0;
    start = 8u + n * 16u;
    if (start > size || data_size > size - start) return 0;
    g->file = p; g->table = p + 8; g->data = p + start;
    g->size = size; g->count = n; g->data_size = data_size;
    return 1;
}

int g1_sprite(const G1 *g, uint32_t id, Sprite *s) {
    const uint8_t *p;
    if (!g->table || id >= g->count) return 0;
    p = g->table + id * 16u;
    s->offset = le32(p); s->width = (int16_t)le16(p + 4);
    s->height = (int16_t)le16(p + 6); s->x = (int16_t)le16(p + 8);
    s->y = (int16_t)le16(p + 10); s->flags = le16(p + 12);
    return s->offset < g->data_size;
}

/* Validate every row before writing any pixels. Malformed rows cannot leave
 * a half-drawn sprite, loop forever or read beyond the data allocation. */
static int valid_pixels(const G1 *g, const Sprite *s) {
    uint32_t left = g->data_size - s->offset;
    const uint8_t *p = g->data + s->offset;
    int y;
    if (s->width <= 0 || s->height <= 0 || s->width > 2048 || s->height > 2048 || (s->flags & 8)) return 0;
    if (!(s->flags & 4)) return (uint32_t)s->width * (uint32_t)s->height <= left;
    if ((uint32_t)s->height * 2u > left) return 0;
    for (y = 0; y < s->height; y++) {
        uint32_t run = le16(p + y * 2);
        int end = 0, runs = 0;
        if (run < (uint32_t)s->height * 2u) return 0;
        while (!end) {
            unsigned n, x;
            if (run > left || left - run < 2 || ++runs > 256) return 0;
            n = p[run++]; x = p[run++]; end = n & 128; n &= 127;
            if (n > left - run || x + n > (unsigned)s->width) return 0;
            run += n;
        }
    }
    return 1;
}

static uint8_t remap(uint8_t pixel, uint8_t colour) {
    /* Primary clothing ramp: 243..254. A zero colour preserves source. */
    return colour && pixel >= 243 && pixel <= 254 ? (uint8_t)(colour + pixel - 243) : pixel;
}

int g1_draw(const G1 *g, uint32_t id, Canvas *c, int x, int y, uint8_t colour) {
    Sprite s;
    const uint8_t *base;
    int row;
    if (!g1_sprite(g, id, &s)) return 0;
    x += s.x; y += s.y;
    if (x >= c->width || y >= c->bottom || x + s.width <= 0 || y + s.height <= c->top) return 1;
    if (!valid_pixels(g, &s)) return 0;
    base = g->data + s.offset;
    for (row = 0; row < s.height; row++) {
        int dy = y + row;
        if (dy < c->top || dy >= c->bottom) continue;
        if (s.flags & 4) {
            uint32_t run = le16(base + row * 2);
            int end = 0;
            while (!end) {
                unsigned n = base[run++], first = base[run++];
                int start, stop, k;
                end = n & 128; n &= 127;
                start = x + (int)first; stop = start + (int)n;
                if (start < 0) start = 0;
                if (stop > c->width) stop = c->width;
                if (!colour && stop > start)
                    memcpy(c->pixels + dy * c->width + start, base + run + start - x - first, (size_t)(stop - start));
                else for (k = start; k < stop; k++)
                    c->pixels[dy * c->width + k] = remap(base[run + k - x - first], colour);
                run += n;
            }
        } else {
            int start = x < 0 ? -x : 0;
            int end = s.width;
            if (x + end > c->width) end = c->width - x;
            for (; start < end; start++) {
                uint8_t p = base[row * s.width + start];
                if (!(s.flags & 1) || p)
                    c->pixels[dy * c->width + x + start] = remap(p, colour);
            }
        }
    }
    return 1;
}

void default_palette(uint32_t rgb[256]) {
    unsigned i;
    for (i = 0; i < 256; i++) {
        unsigned r = ((i >> 5) & 7) * 255 / 7, g = ((i >> 2) & 7) * 255 / 7, b = (i & 3) * 85;
        rgb[i] = (r << 16) | (g << 8) | b;
    }
    rgb[0] = 0x152322; rgb[1] = 0x202d34; rgb[2] = 0x38505b;
    rgb[3] = 0xf2eee0; rgb[4] = 0xf5bd55; rgb[5] = 0x8ba8a2;
    rgb[6] = 0x4d8039; rgb[7] = 0x638c43; rgb[8] = 0x9c9d7f;
    rgb[9] = 0x484c42;
}

void g1_palette(const G1 *g, uint32_t rgb[256]) {
    Sprite s;
    int i;
    default_palette(rgb);
    /* SPR_G1_PALETTE_DEFAULT, SpriteIds.h. Palette data is BGR. */
    if (!g1_sprite(g, 1532, &s) || !(s.flags & 8) || s.width < 1 || s.width > 256 ||
        s.x < 0 || s.x + s.width > 256 || (uint32_t)s.width * 3u > g->data_size - s.offset) return;
    for (i = 0; i < s.width; i++) {
        const uint8_t *p = g->data + s.offset + i * 3;
        rgb[s.x + i] = (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
    }
    /* Keep diagnostic text pens legible; the original dynamic palette starts at 10. */
}

/* Static snapshot sprites use nearest-neighbour sampling on the anchor's
 * world-pixel grid. Both clothing ramps follow Drawing.Sprite.cpp. All source
 * rows are validated before writing, including rows skipped by zoom. */
int g1_draw_guest(const G1 *g,uint32_t id,Canvas *c,int x,int y,unsigned zoom,unsigned shirt,unsigned trousers) {
    Sprite s;const uint8_t *base;int row;unsigned scale;
    if(zoom>3 || shirt>=32 || trousers>=32 || !g1_sprite(g,id,&s))return 0;
    if(s.width>64 || s.height>128 || !valid_pixels(g,&s))return 0;
    scale=1u<<zoom;base=g->data+s.offset;
    for(row=0;row<s.height;row++) {
        int dy,offset=s.y+row;
        unsigned run=0;int end=0;
        if(offset%(int)scale)continue;
        dy=y+offset/(int)scale;if(dy<c->top || dy>=c->bottom)continue;
        if(s.flags&4)run=le16(base+row*2);
        while(!end) {
            unsigned first,n,k;
            if(s.flags&4){n=base[run++];first=base[run++];end=n&128;n&=127;}
            else {first=0;n=(unsigned)s.width;run=(unsigned)row*(unsigned)s.width;end=1;}
            for(k=0;k<n;k++) {
                int dx,sx=s.x+(int)(first+k);uint8_t pixel=base[run+k];
                if(sx%(int)scale || (!(s.flags&4) && (s.flags&1) && !pixel))continue;
                dx=x+sx/(int)scale;if(dx<0 || dx>=c->width)continue;
                if(pixel>=243 && pixel<=254)pixel=guest_colour_ramps[shirt][pixel-243];
                else if(pixel>=202 && pixel<=213)pixel=guest_colour_ramps[trousers][pixel-202];
                c->pixels[dy*c->width+dx]=pixel;
            }
            run+=n;
        }
    }
    return 1;
}
