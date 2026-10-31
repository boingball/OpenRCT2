/* SPDX-License-Identifier: GPL-3.0-only
 * Experimental Amiga graphics runner; not the OpenRCT2 game simulation.
 */
#ifndef AMIGA_RUNNER_H
#define AMIGA_RUNNER_H
#include <stdint.h>
#include <stddef.h>
#include "park_map.h"

#define MAX_WIDTH 640
#define MAX_HEIGHT 480
#define MAX_G1_BYTES (40u * 1024u * 1024u)
#define MAX_G1_ENTRIES 65535u
#define TILE_W 32
#define TILE_H 16
#define MAX_DIRTY ((MAX_WIDTH / TILE_W) * (MAX_HEIGHT / TILE_H))

typedef struct {
    const uint8_t *file, *table, *data;
    uint32_t size, count, data_size;
} G1;
typedef struct {
    uint32_t offset;
    int width, height, x, y;
    uint16_t flags;
} Sprite;
typedef struct {
    uint8_t *pixels;
    int width, height, top, bottom;
} Canvas;
typedef struct { int x, y, w, h; } Rect;
typedef struct {
    int width, height, bench;
    uint32_t mode;
    char g1_path[512], map_path[512];
} Options;
enum {
    KEY_NONE, KEY_QUIT, KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_VIEW, KEY_PREV, KEY_NEXT, KEY_PAUSE, KEY_HOME,
    KEY_MORE, KEY_LESS, KEY_GRID, KEY_BENCH
};
typedef struct {
    G1 g1;
    ParkMap map;
    int map_zoom, map_base;
    Canvas canvas;
    uint8_t *background;
    uint8_t dirty[MAX_DIRTY];
    Rect previous[1024];
    int previous_count, guests, pan_x, pan_y, atlas, page, paused, grid;
    int full_redraw, quit, frames, last_render_ms, last_present_ms, fps10;
    int blits, rejected, diagnostic;
    uint32_t phase, elapsed_ticks;
} Runner;

int g1_open(G1 *g, const void *bytes, uint32_t size);
int g1_sprite(const G1 *g, uint32_t id, Sprite *s);
int g1_draw(const G1 *g, uint32_t id, Canvas *c, int x, int y, uint8_t colour);
void g1_palette(const G1 *g, uint32_t rgb[256]);
void default_palette(uint32_t rgb[256]);
void options_init(Options *o);
int options_parse(Options *o, const char *args, int length);
void runner_init(Runner *r, uint8_t *frame, uint8_t *background, int w, int h);
int runner_set_map(Runner *r, const void *data, uint32_t size);
void runner_map_background(Runner *r, Canvas *c);
void runner_input(Runner *r, int key);
void runner_render(Runner *r);
void runner_step(Runner *r, uint32_t elapsed_ticks, uint32_t frequency);
void runner_mark(Runner *r, Rect rect);
void runner_fill(Canvas *c, int x, int y, int w, int h, uint8_t colour);
void runner_text(Canvas *c, int x, int y, const char *text, uint8_t colour);
int runner_main(void);

void *platform_alloc(uint32_t bytes);
void platform_free(void *p, uint32_t bytes);
void platform_log(const char *text);
int platform_load(const char *path, uint8_t **out, uint32_t *size);
int platform_open(const Options *o);
void platform_close(void);
void platform_palette(const uint32_t rgb[256]);
int platform_input(void);
void platform_wait(void);
uint32_t platform_ticks(void);
uint32_t platform_frequency(void);
void platform_present(const Canvas *c, uint8_t *dirty);
int platform_write_log(const char *text);
void *memcpy(void *dst, const void *src, size_t size);
void *memset(void *dst, int value, size_t size);
#endif
