/* SPDX-License-Identifier: GPL-3.0-only */
#include "park_map.h"
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
unsigned park_map_u16(const uint8_t *p) { return p[0] | (unsigned)p[1]<<8; }
unsigned park_map_guest_key(const uint8_t *p) {
    unsigned x=park_map_u16(p+2)/32,y=park_map_u16(p+4)/32;
    return (x+y)*256+x;
}
uint32_t park_map_offset(const ParkMap *map, unsigned tile) { return le32(map->offsets + tile*4u); }
int park_map_open(ParkMap *map, const void *data, uint32_t size) {
    const uint8_t *p = data;
    uint32_t cells, count, expected, hash=2166136261u, i, start=0;
    unsigned side, version, guests;
    ParkMap candidate;
    *map = (ParkMap){0};
    if (size < PARK_MAP_HEADER || !p || p[0]!='A' || p[1]!='M' || p[2]!='A' || p[3]!='P' || (p[4]!=1 && p[4]!=2) || p[5]) return 0;
    side = p[6] | (unsigned)p[7]<<8;
    if (side<5 || side>256) return 0;
    cells = side*side; count=le32(p+12);
    if (le32(p+8)!=cells || count<cells || count>PARK_MAP_MAX_ELEMENTS) return 0;
    version=p[4];guests=version==2?le32(p+24):0;
    if(guests>PARK_MAP_MAX_GUESTS)return 0;
    for (i=version==2?28:24;i<32;i++) if (p[i]) return 0;
    expected=PARK_MAP_HEADER+(cells+1)*4u+count*PARK_MAP_RECORD+guests*PARK_MAP_GUEST_RECORD;
    if (size!=expected) return 0;
    for (i=PARK_MAP_HEADER;i<size;i++) hash=(hash^p[i])*16777619u;
    if (hash!=le32(p+20)) return 0;
    candidate=(ParkMap){p+PARK_MAP_HEADER,p+PARK_MAP_HEADER+(cells+1)*4u,
        p+PARK_MAP_HEADER+(cells+1)*4u+count*PARK_MAP_RECORD,count,le32(p+16),guests,side};
    if (park_map_offset(&candidate,0)!=0) return 0;
    for (i=0;i<cells;i++) {
        uint32_t end=park_map_offset(&candidate,i+1), n;
        unsigned surfaces=0;
        if (end<=start || end>count) return 0;
        for (n=start;n<end;n++) {
            const uint8_t *r=candidate.records+n*PARK_MAP_RECORD;
            if (r[0]>1 || r[1]==255 || (r[2]&~0x30u)) return 0;
            if (!r[0]) { if ((r[3]&3)==3 || ((r[3]>>2)&3)==3 || ((r[3]>>4)&3)==3 || (r[3]>>6)==3 || r[4]>31 || r[5]>62 || r[7]>15) return 0; surfaces++; }
            else if ((r[4]&~31u) || r[5]>15 || r[6] || r[7]) return 0;
        }
        if (surfaces!=1) return 0;
        start=end;
    }
    if (start!=count) return 0;
    {
        uint8_t seen[PARK_MAP_MAX_GUESTS/8]={0};
        unsigned previous_key=0,previous_depth=0,previous_slot=0;
        for(i=0;i<guests;i++) {
            const uint8_t *g=candidate.guest_records+i*PARK_MAP_GUEST_RECORD;
            unsigned slot=park_map_u16(g),x=park_map_u16(g+2),y=park_map_u16(g+4),z=park_map_u16(g+6);
            unsigned key=park_map_guest_key(g),depth=x+y;
            if(slot>=PARK_MAP_MAX_GUESTS || (seen[slot/8]&(1u<<(slot&7))) || x>=side*32 || y>=side*32 || z>2040
                || g[8]>31 || g[9]>23 || g[10]>1 || g[11]>31 || g[12]>31 || g[14] || g[15])return 0;
            if(i && (key<previous_key || (key==previous_key && (depth<previous_depth
                || (depth==previous_depth && slot<=previous_slot)))))return 0;
            seen[slot/8]|=(uint8_t)(1u<<(slot&7));previous_key=key;previous_depth=depth;previous_slot=slot;
        }
    }
    *map=candidate; return 1;
}
