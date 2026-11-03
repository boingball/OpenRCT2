#include "amilan/protocol.h"
uint16_t amilan_get16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] << 8 | p[1]); }
uint32_t amilan_get32(const uint8_t *p) { return (uint32_t)amilan_get16(p) << 16 | amilan_get16(p + 2); }
void amilan_put16(uint8_t *p, uint16_t n) { p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n; }
void amilan_put32(uint8_t *p, uint32_t n) { amilan_put16(p, (uint16_t)(n >> 16)); amilan_put16(p + 2, (uint16_t)n); }
int amilan_ipv4(const char *s, uint8_t ip[4])
{
    uint8_t result[4];
    unsigned i;
    if (!s) return 0;
    for (i = 0; i < 4; i++) {
        unsigned value = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            value = value * 10 + (unsigned)(*s++ - '0');
            if (++digits > 3 || value > 255) return 0;
        }
        if (!digits || (i < 3 ? *s++ != '.' : *s != 0)) return 0;
        result[i] = (uint8_t)value;
    }
    for (i = 0; i < 4; i++) ip[i] = result[i];
    return 1;
}
int amilan_packet_valid(const struct amilan_packet *p, const struct amilan_codec *codec)
{
    return codec && p && p->type && p->type <= codec->last_type && p->len <= AMILAN_PAYLOAD &&
        (!codec->validate || codec->validate(p));
}
unsigned amilan_encode(uint8_t *dst, unsigned cap, const struct amilan_packet *p, const struct amilan_codec *codec)
{
    unsigned i;
    if (!amilan_packet_valid(p, codec) || cap < AMILAN_HEADER + (unsigned)p->len) return 0;
    dst[0] = codec->magic[0]; dst[1] = codec->magic[1]; dst[2] = codec->version; dst[3] = p->type;
    amilan_put16(dst + 4, p->len); dst[6] = dst[7] = 0;
    for (i = 0; i < p->len; i++) dst[AMILAN_HEADER + i] = p->data[i];
    return AMILAN_HEADER + p->len;
}
int amilan_decode(struct amilan_packet *p, const uint8_t *src, unsigned size, unsigned *used, const struct amilan_codec *codec)
{
    unsigned i, n;
    *used = 0;
    if (size < AMILAN_HEADER) return 0;
    if (!codec || src[0] != codec->magic[0] || src[1] != codec->magic[1] || src[2] != codec->version || !src[3] || src[3] > codec->last_type || src[6] || src[7]) return -1;
    n = amilan_get16(src + 4);
    if (n > AMILAN_PAYLOAD) return -1;
    if (size < AMILAN_HEADER + n) return 0;
    p->type = src[3]; p->len = (uint16_t)n;
    for (i = 0; i < n; i++) p->data[i] = src[AMILAN_HEADER + i];
    if (!amilan_packet_valid(p, codec)) return -1;
    *used = AMILAN_HEADER + n;
    return 1;
}
uint32_t amilan_checksum(uint32_t hash, const uint8_t *data, unsigned size)
{
    unsigned i;
    for (i = 0; i < size; i++) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

