#ifndef G729_BITSTREAM_H
#define G729_BITSTREAM_H

#include <stdint.h>

#include "g729.h"

#define G729_BITSTREAM_BITS 80

typedef struct g729_bitstream_frame {
    uint16_t l0;
    uint16_t l1;
    uint16_t l2;
    uint16_t l3;
    uint16_t p1;
    uint16_t p0;
    uint16_t c1;
    uint16_t s1;
    uint16_t ga1;
    uint16_t gb1;
    uint16_t p2;
    uint16_t c2;
    uint16_t s2;
    uint16_t ga2;
    uint16_t gb2;
} g729_bitstream_frame;

int g729_bitstream_pack(const g729_bitstream_frame *frame,
                        uint8_t out[G729_FRAME_BYTES]);
int g729_bitstream_unpack(const uint8_t bits[G729_FRAME_BYTES],
                          g729_bitstream_frame *frame);
uint16_t g729_bitstream_parity(uint16_t p1);

#endif
