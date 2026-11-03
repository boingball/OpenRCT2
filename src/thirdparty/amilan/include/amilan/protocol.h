/* Proven eight-byte frame, with application-selected signature/version/types. */
#ifndef AMILAN_PROTOCOL_H
#define AMILAN_PROTOCOL_H
#include <stdint.h>
#define AMILAN_HEADER 8
#define AMILAN_PAYLOAD 512
#define AMILAN_FRAME (AMILAN_HEADER + AMILAN_PAYLOAD)
/* Monotonic 50 Hz ticks; unsigned subtraction handles clock wrap. */
#define AMILAN_CONNECT_TICKS 500
#define AMILAN_IDLE_TICKS 750
#define AMILAN_STALL_TICKS 500
enum amilan_reason { AMILAN_NORMAL, AMILAN_BAD_PACKET, AMILAN_TIMEOUT,
 AMILAN_FULL, AMILAN_IO_ERROR, AMILAN_BACKPRESSURE, AMILAN_REJECTED };
struct amilan_packet { uint16_t len; uint8_t type; uint8_t data[AMILAN_PAYLOAD]; };
/* Types 1..last_type (up to 255) belong to the application. No game packet IDs in AmiLAN.
 * validate is optional, called for complete incoming and outgoing packets. */
struct amilan_codec { uint8_t magic[2], version, last_type; int (*validate)(const struct amilan_packet *); };
uint16_t amilan_get16(const uint8_t *p);
uint32_t amilan_get32(const uint8_t *p);
void amilan_put16(uint8_t *p,uint16_t n);
void amilan_put32(uint8_t *p,uint32_t n);
int amilan_ipv4(const char *text,uint8_t ip[4]);
int amilan_packet_valid(const struct amilan_packet *p,const struct amilan_codec *codec);
unsigned amilan_encode(uint8_t *dst,unsigned capacity,const struct amilan_packet *p,const struct amilan_codec *codec);
/* 1 complete, 0 incomplete, -1 malformed. consumed=0 unless complete. */
int amilan_decode(struct amilan_packet *p,const uint8_t *src,unsigned size,unsigned *consumed,const struct amilan_codec *codec);
/* FNV-1a; integrity checksum, not authentication. */
uint32_t amilan_checksum(uint32_t hash,const uint8_t *data,unsigned size);
void amilan_ipv4_text(char out[16],const uint8_t ip[4]);
#endif

