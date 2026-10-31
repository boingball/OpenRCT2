/* SPDX-License-Identifier: GPL-3.0-only */
#include "runner.h"

static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-./+[]=?";
static const uint8_t font[][5] = {
    {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},{7,4,6,4,7},{7,4,6,4,4},
    {3,4,5,5,3},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,2},{5,5,6,5,5},{4,4,4,4,7},
    {5,7,7,5,5},{5,7,7,7,5},{2,5,5,5,2},{6,5,6,4,4},{2,5,5,3,1},{6,5,6,5,5},
    {3,4,2,1,6},{7,2,2,2,2},{5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},
    {5,5,2,2,2},{7,1,2,4,7},
    {7,5,5,5,7},{2,6,2,2,7},{6,1,2,4,7},{6,1,2,1,6},{5,5,7,1,1},
    {7,4,6,1,6},{3,4,6,5,2},{7,1,2,2,2},{2,5,2,5,2},{2,5,3,1,6},
    {0,2,0,2,0},{0,0,7,0,0},{0,0,0,0,2},{1,1,2,4,4},{0,2,7,2,0},
    {3,2,2,2,3},{6,2,2,2,6},{0,7,0,7,0},{6,1,2,0,2}
};

void runner_fill(Canvas *c, int x, int y, int w, int h, uint8_t colour) {
    int yy;
    if (x < 0) { w += x; x = 0; }
    if (y < c->top) { h -= c->top - y; y = c->top; }
    if (x + w > c->width) w = c->width - x;
    if (y + h > c->bottom) h = c->bottom - y;
    if (w <= 0 || h <= 0) return;
    for (yy = y; yy < y + h; yy++) memset(c->pixels + yy * c->width + x, colour, (size_t)w);
}

void runner_text(Canvas *c, int x, int y, const char *text, uint8_t colour) {
    int start = x;
    while (*text) {
        int k, yy, xx;
        char ch = *text++;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch == '\n') { y += 14; x = start; continue; }
        for (k = 0; alphabet[k] && alphabet[k] != ch; k++);
        if (alphabet[k]) for (yy = 0; yy < 5; yy++) for (xx = 0; xx < 3; xx++)
            if (font[k][yy] & (4 >> xx)) runner_fill(c, x + xx * 2, y + yy * 2, 2, 2, colour);
        x += 8;
    }
}

static char *number(char *p, uint32_t n) {
    char tmp[11]; int k = 0;
    do { tmp[k++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (k) *p++ = tmp[--k];
    *p = 0;
    return p;
}
static char *word(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
static int equal(const char *a, const char *b) {
    while (*a && *b) {
        char c = *a++;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c != *b++) return 0;
    }
    return !*a && !*b;
}
static int hex(const char *p, uint32_t *n) {
    unsigned count = 0; *n = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    else if (*p == '$') p++;
    while (*p) {
        unsigned v; char c = *p++;
        if (c >= '0' && c <= '9') v = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v = (unsigned)(c - 'A' + 10);
        else return 0;
        if (++count > 8) return 0;
        *n = (*n << 4) | v;
    }
    return count > 0;
}

void options_init(Options *o) {
    memset(o, 0, sizeof(*o)); o->width = 640; o->height = 480; o->mode = 0xffffffffu;
}
int options_parse(Options *o, const char *args, int length) {
    int i = 0;
    while (i < length) {
        char token[544], key[32]; int n = 0, quote = 0, k;
        while (i < length && (unsigned char)args[i] <= ' ') i++;
        if (i == length) break;
        while (i < length) {
            char c = args[i++];
            if (c == '"') { quote = !quote; continue; }
            if (!quote && (unsigned char)c <= ' ') break;
            if (n == (int)sizeof(token) - 1) return 0;
            token[n++] = c;
        }
        if (quote) return 0;
        token[n] = 0;
        if (equal(token, "LOW")) { o->width = 320; o->height = 240; }
        else if (equal(token, "BENCH")) o->bench = 1;
        else if (equal(token, "DIAGNOSTIC")) o->g1_path[0] = '-';
        else {
            for (k = 0; token[k] && token[k] != '=' && k < 31; k++) key[k] = token[k];
            key[k] = 0;
            if (token[k] != '=') return 0;
            if (equal(key, "MODEID")) { if (!hex(token + k + 1, &o->mode)) return 0; }
            else if (equal(key, "MAP")) {
                if (n-k-1 >= (int)sizeof(o->map_path) || n==k+1) return 0;
                memcpy(o->map_path,token+k+1,(size_t)(n-k));
            }
            else if (equal(key, "G1")) {
                if (n - k - 1 >= (int)sizeof(o->g1_path) || n == k + 1) return 0;
                memcpy(o->g1_path, token + k + 1, (size_t)(n - k));
            } else return 0;
        }
    }
    return 1;
}

void runner_init(Runner *r, uint8_t *frame, uint8_t *background, int w, int h) {
    memset(r, 0, sizeof(*r));
    r->canvas.pixels = frame; r->canvas.width = w; r->canvas.height = h;
    r->canvas.top = 0; r->canvas.bottom = h; r->background = background;
    r->guests = 128; r->full_redraw = 1; r->diagnostic = 1;
}
void runner_input(Runner *r, int key) {
    switch (key) {
        case KEY_QUIT: r->quit = 1; return;
        case KEY_LEFT: r->pan_x += 24; break;
        case KEY_RIGHT: r->pan_x -= 24; break;
        case KEY_UP: r->pan_y += 16; break;
        case KEY_DOWN: r->pan_y -= 16; break;
        case KEY_VIEW: if(r->map.side)r->map_tracks_visible=!r->map_tracks_visible;else r->atlas = !r->atlas; break;
        case KEY_PREV: if (r->map.side&&!r->atlas) {if(r->map_zoom<3){r->map_zoom++;r->pan_x/=2;r->pan_y/=2;}break;} if (r->page > 0) r->page--; break;
        case KEY_NEXT: if (r->map.side&&!r->atlas) {if(r->map_zoom>0){r->map_zoom--;r->pan_x*=2;r->pan_y*=2;}break;} if ((uint32_t)(r->page + 1) * 12u < r->g1.count) r->page++; break;
        case KEY_PAUSE: if(r->map.side)r->map_guests_visible=!r->map_guests_visible;else r->paused = !r->paused; break;
        case KEY_HOME: r->pan_x = r->pan_y = r->page = 0; break;
        case KEY_MORE:
            if(r->map.side){r->map_ride_selected++;if(r->map_ride_selected>=(int)r->map.ride_count)r->map_ride_selected=-1;}
            else if (r->guests < 1024) r->guests *= 2; break;
        case KEY_LESS:
            if(r->map.side){r->map_ride_selected--;if(r->map_ride_selected < -1)r->map_ride_selected=(int)r->map.ride_count-1;}
            else if (r->guests > 16) r->guests /= 2; break;
        case KEY_GRID: r->grid = !r->grid; break;
        default: return;
    }
    if(r->map.side && (key==KEY_MORE || key==KEY_LESS) && r->map_ride_selected>=0) {
        const uint8_t *ride=r->map.ride_records+r->map_ride_selected*PARK_MAP_RIDE_RECORD;
        if(ride[5]<r->map.side && ride[6]<r->map.side) {
            int z=1<<r->map_zoom;
            r->pan_x=-((int)ride[5]-(int)ride[6])*32/z;
            r->pan_y=((int)r->map.side-1-(int)ride[5]-(int)ride[6])*16/z;
        }
    }
    /* Keep projections in a small signed range, even after long key repeats. */
    if (!r->map.side) {
    if (r->pan_x < -2200) r->pan_x = -2200;
    if (r->pan_x > 2200) r->pan_x = 2200;
    if (r->pan_y < -1400) r->pan_y = -1400;
    if (r->pan_y > 1400) r->pan_y = 1400;
    } else {
        int bx=(int)r->map.side*32+640,by=(int)r->map.side*16+2048;
        if(r->pan_x < -bx)r->pan_x=-bx;if(r->pan_x>bx)r->pan_x=bx;
        if(r->pan_y < -by)r->pan_y=-by;if(r->pan_y>by)r->pan_y=by;
    }
    r->full_redraw = 1;
}
void runner_step(Runner *r, uint32_t elapsed_ticks, uint32_t frequency) {
    /* 25 animation steps/second; no dependence on the rendered frame rate.
     * Clamp long pauses and bound arithmetic to avoid 32-bit multiplication overflow. */
    uint32_t quantum = frequency / 25u;
    if (!quantum || r->paused) return;
    if (elapsed_ticks > frequency) elapsed_ticks = frequency;
    r->elapsed_ticks += elapsed_ticks;
    r->phase += r->elapsed_ticks / quantum;
    r->elapsed_ticks %= quantum;
}

void runner_mark(Runner *r, Rect q) {
    int x, y, endx, endy, columns = r->canvas.width / TILE_W;
    if (q.w <= 0 || q.h <= 0 || q.x >= r->canvas.width || q.y >= r->canvas.height || q.x + q.w <= 0 || q.y + q.h <= 0) return;
    endx = (q.x + q.w - 1) / TILE_W;
    endy = (q.y + q.h - 1) / TILE_H;
    if (q.x < 0) q.x = 0;
    if (q.y < 0) q.y = 0;
    if (endx >= columns) endx = columns - 1;
    if (endy >= r->canvas.height / TILE_H) endy = r->canvas.height / TILE_H - 1;
    for (y = q.y / TILE_H; y <= endy; y++) for (x = q.x / TILE_W; x <= endx; x++) r->dirty[y * columns + x] = 1;
}

static void diamond(Canvas *c, int x, int y, uint8_t colour, int grid) {
    int row;
    for (row = 0; row < 32; row++) {
        int half = row < 16 ? row * 2 : (31 - row) * 2;
        runner_fill(c, x - half, y + row - 16, half * 2 + 1, 1, colour);
        if (grid && half) {
            runner_fill(c, x - half, y + row - 16, 1, 1, 9);
            runner_fill(c, x + half, y + row - 16, 1, 1, 9);
        }
    }
}
static void make_background(Runner *r) {
    Canvas c = r->canvas;
    int sum, x, y, origin_x = c.width / 2 + r->pan_x, origin_y = c.height / 2 - 1024 + r->pan_y;
    c.pixels = r->background; memset(c.pixels, 0, (size_t)c.width * c.height);
    c.top = 48; c.bottom = c.height - 48;
    if (r->map.side) {runner_map_background(r,&c);return;}
    for (sum = 0; sum <= 126; sum++) for (x = 0; x < 64; x++) {
        int sx, sy, path;
        y = sum - x; if (y < 0 || y >= 64) continue;
        sx = origin_x + (x - y) * 32; sy = origin_y + sum * 16;
        if (sx < -64 || sx >= c.width + 64 || sy < c.top - 48 || sy > c.bottom + 48) continue;
        path = x == 28 || x == 36 || y == 28 || y == 36;
        if (r->g1.count && !path) {
            /* First flat grass sprite from rct2.terrain_surface.grass. */
            if (!g1_draw(&r->g1, 1915, &c, sx, sy, 0)) diamond(&c, sx, sy, 6, r->grid);
        } else diamond(&c, sx, sy, path ? 8 : ((x ^ y) & 1 ? 6 : 7), r->grid);
    }
}
static void atlas(Runner *r) {
    static uint8_t cell[(MAX_WIDTH / 4) * ((MAX_HEIGHT - 96) / 3)];
    Canvas *c = &r->canvas;
    int cellw = c->width / 4, cellh = (c->height - 96) / 3, i;
    for (i = 0; i < 12; i++) {
        uint32_t id = (uint32_t)r->page * 12u + (unsigned)i;
        int x = (i % 4) * cellw, y = 48 + (i / 4) * cellh;
        char line[32], *p = line;
        Sprite s; Canvas clip = {cell, cellw, cellh, 0, cellh};
        int row;
        memset(cell, 1, (size_t)cellw * cellh);
        p = word(p, "ID "); number(p, id);
        runner_text(&clip, 8, 6, line, 4);
        if (g1_sprite(&r->g1, id, &s)) {
            if (s.flags & 8) runner_text(&clip, 8, 24, "PALETTE", 5);
            else {
                clip.top = 20; clip.bottom = cellh - 2;
                if (!g1_draw(&r->g1, id, &clip, cellw / 2 - s.width / 2 - s.x,
                             24 + (cellh - 24 - s.height) / 2 - s.y, 0)) r->rejected++;
            }
        }
        for (row = 1; row < cellh - 1; row++)
            memcpy(c->pixels + (y + row) * c->width + x + 1, cell + row * cellw + 1, (size_t)cellw - 2);
    }
}

static Rect guest(Runner *r, int n, int draw) {
    /* Scripted visitors orbit the square path. No guest AI or park simulation. */
    uint32_t t = (r->phase + (uint32_t)n * 31u) % 1024u;
    int side = (int)(t >> 8), pos = (int)(t & 255);
    int wx, wy, sx, sy, dir;
    Rect rect;
    Sprite s;
    uint32_t id;
    switch (side) {
        default: wx = 28 * 32 + pos; wy = 28 * 32; dir = 2; break;
        case 1: wx = 36 * 32; wy = 28 * 32 + pos; dir = 1; break;
        case 2: wx = 36 * 32 - pos; wy = 36 * 32; dir = 0; break;
        case 3: wx = 28 * 32; wy = 36 * 32 - pos; dir = 3; break;
    }
    /* Separate lanes without changing the path geometry. */
    wx += (n & 3) * 2 - 3; wy += ((n >> 2) & 3) * 2 - 3;
    sx = r->canvas.width / 2 + r->pan_x + wx - wy;
    sy = r->canvas.height / 2 - 1024 + r->pan_y + (wx + wy) / 2;
    id = 6409u + (unsigned)dir + ((r->phase / 2u + (unsigned)n) % 6u) * 4u;
    rect = (Rect){sx - 4, sy - 18, 9, 20};
    if (g1_sprite(&r->g1, id, &s)) rect = (Rect){sx + s.x, sy + s.y, s.width, s.height};
    if (draw) {
        Canvas clip = r->canvas; clip.top = 48; clip.bottom -= 48;
        if (r->g1.count) {
            if (!g1_draw(&r->g1, id, &clip, sx, sy, (uint8_t)(22 + (n % 12) * 12))) r->rejected++;
            else r->blits++;
        } else {
            runner_fill(&clip, sx - 2, sy - 16, 5, 5, 4);
            runner_fill(&clip, sx - 3, sy - 11, 7, 8, (uint8_t)(64 + (n * 17) % 160));
            runner_fill(&clip, sx - 2, sy - 3, 2, 4, 9);
            runner_fill(&clip, sx + 1, sy - 3, 2, 4, 9);
        }
    }
    return rect;
}

static void hud(Runner *r) {
    Canvas *c = &r->canvas;
    char line[96], *p = line;
    runner_fill(c, 0, 0, c->width, 48, 1);
    runner_fill(c, 0, c->height - 48, c->width, 48, 1);
    runner_fill(c, 0, 46, c->width, 2, 4);
    runner_text(c, 8, 6, r->map.side ? "OPENRCT2 / IMPORTED PARK VIEW 0.10" : c->width > 320 ? "OPENRCT2 / AMIGA GRAPHICS RUNNER 0.1" : "OPENRCT2 / AMIGA RUNNER 0.1", 3);
    if(r->map.side&&!r->atlas){p=word(p,"MAP ");p=number(p,r->map.side);p=word(p," GUESTS ");p=number(p,r->map_guests_visible?r->map.guest_count:0);}
    else {p = word(p, r->atlas ? "SPRITES " : "CROWD "); p = number(p, r->atlas ? (uint32_t)r->page : (uint32_t)r->guests);}
    p = word(p, "  FPS "); p = number(p, (unsigned)r->fps10 / 10u); *p++ = '.'; *p++ = (char)('0' + r->fps10 % 10); *p = 0;
    if (c->width > 320) {
        p = word(p, "  DRAW "); p = number(p, (unsigned)r->last_render_ms);
        p = word(p, "MS  COPY "); p = number(p, (unsigned)r->last_present_ms); word(p, "MS");
    }
    runner_text(c, 8, 24, line, 4);
    runner_text(c, 8, c->height - 40, r->map.side ? (c->width>320?"ARROWS PAN  [ ] ZOOM  +/- RIDE  TAB TRACK  SPACE GUEST  ESC":"ARROWS PAN  [ ] ZOOM  SPACE GUESTS  ESC") : c->width > 320 ? "ARROWS PAN  TAB VIEW  [ ] PAGE  +/- CROWD  SPACE PAUSE  ESC QUIT" : "ARROWS PAN  TAB VIEW  ESC QUIT", 3);
    runner_text(c, 8, c->height - 24, r->map.side ? "SAVED MAP / STATIC GUEST SNAPSHOT - NO SIMULATION" : c->width > 320 ? "B BENCHMARK  HOME RESET  G GRID / GRAPHICS TEST - NO PARK SIMULATION" : "+/- CROWD  B BENCHMARK  [ ] PAGE", 5);
    if (r->map.side) {
        char details[96],*q=details;
        if(r->map_ride_selected>=0) {
            const uint8_t *ride=r->map.ride_records+r->map_ride_selected*PARK_MAP_RIDE_RECORD;
            q=word(q,"RIDE ");q=number(q,ride[0]);q=word(q," TYPE ");q=number(q,ride[1]);
            q=word(q," STATUS ");q=number(q,ride[3]);q=word(q," STATIONS ");number(q,ride[7]);
        } else {q=word(q,"RIDES ");q=number(q,r->map.ride_count);q=word(q," TRACK TILES ");q=number(q,r->map.track_count);word(q," / DIAGNOSTIC FOOTPRINTS");}
        runner_text(c,8,c->height-10,details,4);
    }
    else if (r->diagnostic) runner_text(c, 8, c->height - 10, "DIAGNOSTIC / NO RCT2 DATA", 4);
    else runner_text(c, 8, c->height - 10, "ORIGINAL G1 SPRITES / SCRIPTED CROWD", 4);
    runner_mark(r, (Rect){0, 0, c->width, 48});
    runner_mark(r, (Rect){0, c->height - 48, c->width, 48});
}

void runner_render(Runner *r) {
    Canvas *c = &r->canvas;
    int i;
    r->blits = r->rejected = 0;
    if (r->full_redraw) {
        make_background(r);
        memcpy(c->pixels, r->background, (size_t)c->width * c->height);
        memset(r->dirty, 1, (size_t)(c->width / TILE_W) * (c->height / TILE_H));
        r->previous_count = 0; r->full_redraw = 0;
    } else {
        for (i = 0; i < r->previous_count; i++) {
            Rect q = r->previous[i]; int yy;
            if (q.x < 0) { q.w += q.x; q.x = 0; }
            if (q.y < 48) { q.h -= 48 - q.y; q.y = 48; }
            if (q.x + q.w > c->width) q.w = c->width - q.x;
            if (q.y + q.h > c->height - 48) q.h = c->height - 48 - q.y;
            if (q.w <= 0 || q.h <= 0) continue;
            for (yy = q.y; yy < q.y + q.h; yy++) memcpy(c->pixels + yy * c->width + q.x, r->background + yy * c->width + q.x, (size_t)q.w);
            runner_mark(r, q);
        }
    }
    r->previous_count = 0;
    if (r->atlas) atlas(r);
    else {
        /* Bucket by projected depth, so overlapping visitors draw back to front.
         * Bounded stack storage, one bucket per screen row. */
        int16_t head[MAX_HEIGHT], next[1024];
        for (i = 0; i < MAX_HEIGHT; i++) head[i] = -1;
        for (i = 0; i < r->guests; i++) {
            Rect q = guest(r, i, 0);
            int b = q.y + q.h;
            if (b < 0) b = 0;
            if (b >= MAX_HEIGHT) b = MAX_HEIGHT - 1;
            next[i] = head[b]; head[b] = (int16_t)i;
        }
        for (i = 0; i < MAX_HEIGHT; i++) {
            int n;
            for (n = head[i]; n >= 0; n = next[n]) {
                Rect q = guest(r, n, 1);
                r->previous[r->previous_count++] = q; runner_mark(r, q);
            }
        }
    }
    hud(r); r->frames++;
}

static uint32_t milliseconds(uint32_t ticks, uint32_t hz) {
    return hz ? (ticks / hz) * 1000u + (ticks % hz) / (hz / 1000u ? hz / 1000u : 1u) : 0;
}
static void report(Runner *r, uint32_t hz, uint32_t ticks, unsigned frames) {
    char text[512], *p = text;
    p = word(p, r->map.side ? "OpenRCT2 imported park viewer 0.10\nView-only terrain/path benchmark; no park simulation.\nResolution: " : "OpenRCT2 Amiga graphics runner 0.1\nGraphics benchmark only; no OpenRCT2 simulation.\nResolution: ");
    p = number(p, (unsigned)r->canvas.width); p = word(p, " x "); p = number(p, (unsigned)r->canvas.height);
    p = word(p, "\nSprites: "); p = number(p, r->g1.count);
    p = word(p, r->map.side ? "\nImported map elements: " : "\nScripted visitors: "); p = number(p, r->map.side ? r->map.elements : (unsigned)r->guests);
    if(r->map.side){p=word(p,"\nSaved guest snapshots: ");p=number(p,r->map.guest_count);}
    if(r->map.side){p=word(p,"\nRide snapshots: ");p=number(p,r->map.ride_count);p=word(p,"\nTrack footprints: ");p=number(p,r->map.track_count);}
    p = word(p, "\nFrames: "); p = number(p, frames);
    p = word(p, "\nElapsed ms: "); p = number(p, milliseconds(ticks, hz));
    p = word(p, "\nFPS x10: ");
    /* Dividing the time first bounds this even during a long benchmark. */
    p = number(p, ticks / 100u ? frames * 10u * (hz / 100u) / (ticks / 100u) : 0);
    p = word(p, "\nLast render ms: "); p = number(p, (unsigned)r->last_render_ms);
    p = word(p, "\nLast RTG copy ms: "); p = number(p, (unsigned)r->last_present_ms);
    p = word(p, "\nLast rejected sprites: "); p = number(p, (unsigned)r->rejected);
    word(p, "\n");
    platform_log(text);
    if (!platform_write_log(text)) platform_log("Could not write PROGDIR:OpenRCT2Runner.bench\n");
}

/* CLI parsing is platform-specific; options are passed via these globals. */
Options runner_options;
int runner_main(void) {
    static Runner state;
    uint8_t *frame = 0, *back = 0, *file = 0, *map_file = 0;
    uint32_t size = 0, map_size = 0, bytes, rgb[256], hz, now, last, sample, bench_start = 0;
    unsigned count = 0, bench_frames = 0;
    Options *o = &runner_options;
    int rc = 20;
    bytes = (uint32_t)o->width * (uint32_t)o->height;
    frame = platform_alloc(bytes); back = platform_alloc(bytes);
    if (!frame || !back) { platform_log("Not enough Fast RAM for the display buffers.\n"); goto cleanup; }
    runner_init(&state, frame, back, o->width, o->height);
    if (o->g1_path[0] != '-') {
        int loaded;
        if (o->g1_path[0]) loaded = platform_load(o->g1_path, &file, &size);
        else {
            loaded = platform_load("PROGDIR:Data/g1.dat", &file, &size);
            if (!loaded) loaded = platform_load("PROGDIR:g1.dat", &file, &size);
        }
        if (loaded) {
            if (!g1_open(&state.g1, file, size)) { platform_log("Invalid or unsupported G1.DAT. Use original RCT2 data.\n"); goto cleanup; }
            /* RCT Classic uses a different ordering; do not silently draw wrong sprites. */
            if (state.g1.count != 29294) { platform_log("Unsupported G1 edition: requires original RCT2's 29294-entry G1.DAT.\n"); goto cleanup; }
            state.diagnostic = 0;
        } else if (o->g1_path[0]) { platform_log("Cannot read the specified G1 file (missing, too large, or out of Fast RAM).\n"); goto cleanup; }
        else platform_log("No G1.DAT found. Starting labelled diagnostic mode.\n");
    }
    if(o->map_path[0]) {
        if(!platform_load(o->map_path,&map_file,&map_size) || !runner_set_map(&state,map_file,map_size)) {
            platform_log("Cannot load imported MAP (missing, corrupt, or unsupported version).\n");goto cleanup;
        }
        platform_log("Imported map/guest snapshot loaded. No rides or simulation.\n");
    }
    if (!platform_open(o)) goto cleanup;
    g1_palette(&state.g1, rgb); if(state.map.side){rgb[2]=0x267db4;rgb[5]=0x64b4cf;} platform_palette(rgb);
    hz = platform_frequency(); if (hz < 1000u) { platform_log("No usable E-clock timer.\n"); goto cleanup; }
    last = sample = platform_ticks();
    if (o->bench) { bench_start = last; bench_frames = 0; }
    while (!state.quit) {
        int key;
        while ((key = platform_input()) != KEY_NONE) {
            if (key == KEY_BENCH) { bench_start = platform_ticks(); bench_frames = 0; o->bench = 1; }
            else runner_input(&state, key);
        }
        if (state.quit) break;
        now = platform_ticks(); runner_step(&state, now - last, hz); last = now;
        if(o->bench&&state.map.side&&!state.atlas)state.full_redraw=1;
        runner_render(&state);
        state.last_render_ms = (int)milliseconds(platform_ticks() - now, hz);
        now = platform_ticks(); platform_present(&state.canvas, state.dirty);
        state.last_present_ms = (int)milliseconds(platform_ticks() - now, hz);
        count++; now = platform_ticks();
        if (now - sample >= hz) {
            state.fps10 = (int)(count * 10u * (hz / 100u) / ((now - sample) / 100u)); sample = now; count = 0;
        }
        if (o->bench && ++bench_frames && now - bench_start >= hz * 10u) {
            report(&state, hz, now - bench_start, bench_frames); o->bench = 0;
        }
        platform_wait();
    }
    rc = 0;
cleanup:
    platform_close();
    if (map_file) platform_free(map_file,map_size);
    if (file) platform_free(file, size);
    if (back) platform_free(back, bytes);
    if (frame) platform_free(frame, bytes);
    return rc;
}
