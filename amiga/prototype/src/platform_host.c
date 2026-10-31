/* SPDX-License-Identifier: GPL-3.0-only
 * Headless host preview of the same renderer, not an Amiga emulator.
 */
#include "runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *platform_alloc(uint32_t n) { return calloc(1, n); }
void platform_free(void *p, uint32_t n) { (void)n; free(p); }
void platform_log(const char *s) { fputs(s, stderr); }
int platform_load(const char *path, uint8_t **out, uint32_t *size) {
    FILE *f = fopen(path, "rb"); long n;
    *out = NULL; *size = 0;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 8 || n > MAX_G1_BYTES || fseek(f, 0, SEEK_SET)) { fclose(f); return 0; }
    *out = malloc((size_t)n);
    if (!*out) { fclose(f); return 0; }
    if (fread(*out, 1, (size_t)n, f) != (size_t)n) { free(*out); *out = NULL; fclose(f); return 0; }
    *size = (uint32_t)n; fclose(f); return 1;
}
int platform_open(const Options *o) { (void)o; return 0; }
void platform_close(void) {}
void platform_palette(const uint32_t p[256]) { (void)p; }
int platform_input(void) { return KEY_QUIT; }
void platform_wait(void) {}
uint32_t platform_ticks(void) { return 0; }
uint32_t platform_frequency(void) { return 1000000; }
void platform_present(const Canvas *c, uint8_t *d) { (void)c; (void)d; }
int platform_write_log(const char *s) { (void)s; return 0; }

int main(int argc, char **argv) {
    static Runner r;
    uint32_t rgb[256], size = 0, map_size = 0;
    uint8_t *frame, *back, *file = NULL, *map_file = NULL;
    int w = 640, h = 480, x, y, i;
    FILE *out;
    if (argc < 2 || argc > 6) { fputs("usage: runner_host output.ppm [MAP=imported.amap] [original-g1.dat] [LOW] [MARKERS]\n", stderr); return 2; }
    for (i = 2; i < argc; i++) if (!strcmp(argv[i], "LOW")) { w=320; h=240; }
    frame = calloc((size_t)w, (size_t)h); back = calloc((size_t)w, (size_t)h);
    if (!frame || !back) return 2;
    runner_init(&r, frame, back, w, h);
    for (i=2;i<argc;i++) {
        if(!strcmp(argv[i],"LOW")||!strcmp(argv[i],"MARKERS"))continue;
        if(!strncmp(argv[i],"MAP=",4)) {
            if(map_file||!platform_load(argv[i]+4,&map_file,&map_size)||!runner_set_map(&r,map_file,map_size)) {
                fputs("invalid MAP file\n",stderr);free(map_file);free(file);free(back);free(frame);return 2;
            }
        } else {
            if(file||!platform_load(argv[i],&file,&size)||!g1_open(&r.g1,file,size)) {
                fputs("invalid G1 file\n",stderr);free(map_file);free(file);free(back);free(frame);return 2;
            }
            r.diagnostic=0;
        }
    }
    for(i=2;i<argc;i++)if(!strcmp(argv[i],"MARKERS"))r.map_guest_sprites=0;
    for (i=0;i<80;i++) { runner_step(&r,40000,1000000); runner_render(&r); memset(r.dirty,0,sizeof(r.dirty)); }
    g1_palette(&r.g1,rgb);if(r.map.side){rgb[2]=0x267db4;rgb[5]=0x64b4cf;}
    out=fopen(argv[1],"wb");
    if (!out) { free(map_file); free(file); free(back); free(frame); return 2; }
    fprintf(out,"P6\n%d %d\n255\n",w,h);
    for (y=0;y<h;y++) for (x=0;x<w;x++) {
        uint32_t colour=rgb[frame[y*w+x]];
        uint8_t pixel[3]={(uint8_t)(colour>>16),(uint8_t)(colour>>8),(uint8_t)colour};
        if (fwrite(pixel,1,3,out)!=3) { fclose(out); free(map_file); free(file); free(back); free(frame); return 2; }
    }
    if (fclose(out)) { free(map_file); free(file); free(back); free(frame); return 2; }
    printf("Rendered %dx%d (%s). This is a host preview, not a hardware benchmark.\n",w,h,r.diagnostic?"diagnostic":"G1 sprites");
    free(map_file); free(file); free(back); free(frame); return 0;
}
