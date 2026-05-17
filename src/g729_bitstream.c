#include "g729_bitstream.h"

#include <stddef.h>
#include <string.h>

typedef struct bit_writer {
    uint8_t *buf;
    int bit_pos;
} bit_writer;

typedef struct bit_reader {
    const uint8_t *buf;
    int bit_pos;
} bit_reader;

static void bit_writer_write(bit_writer *w, uint16_t value, int bits) {
    int i;
    for (i = bits - 1; i >= 0; --i) {
        if (((value >> (unsigned)i) & 1u) != 0u) {
            const int byte_idx = w->bit_pos >> 3;
            const int bit_idx = 7 - (w->bit_pos & 7);
            if (byte_idx < G729_FRAME_BYTES) {
                w->buf[byte_idx] |= (uint8_t)(1u << (unsigned)bit_idx);
            }
        }
        ++w->bit_pos;
    }
}

static uint16_t bit_reader_read(bit_reader *r, int bits) {
    uint16_t out = 0;
    int i;
    for (i = 0; i < bits; ++i) {
        const int byte_idx = r->bit_pos >> 3;
        const int bit_idx = 7 - (r->bit_pos & 7);
        uint16_t bit = 0;
        if (byte_idx < G729_FRAME_BYTES) {
            bit = (uint16_t)((r->buf[byte_idx] >> (unsigned)bit_idx) & 1u);
        }
        out = (uint16_t)((out << 1) | bit);
        ++r->bit_pos;
    }
    return out;
}

int g729_bitstream_pack(const g729_bitstream_frame *frame,
                        uint8_t out[G729_FRAME_BYTES]) {
    bit_writer w;
    if (frame == NULL || out == NULL) {
        return G729_ERR_NULL;
    }

    memset(out, 0, G729_FRAME_BYTES);
    w.buf = out;
    w.bit_pos = 0;

    bit_writer_write(&w, frame->l0, 1);
    bit_writer_write(&w, frame->l1, 7);
    bit_writer_write(&w, frame->l2, 5);
    bit_writer_write(&w, frame->l3, 5);
    bit_writer_write(&w, frame->p1, 8);
    bit_writer_write(&w, frame->p0, 1);
    bit_writer_write(&w, frame->c1, 13);
    bit_writer_write(&w, frame->s1, 4);
    bit_writer_write(&w, frame->ga1, 3);
    bit_writer_write(&w, frame->gb1, 4);
    bit_writer_write(&w, frame->p2, 5);
    bit_writer_write(&w, frame->c2, 13);
    bit_writer_write(&w, frame->s2, 4);
    bit_writer_write(&w, frame->ga2, 3);
    bit_writer_write(&w, frame->gb2, 4);

    return G729_OK;
}

int g729_bitstream_unpack(const uint8_t bits[G729_FRAME_BYTES],
                          g729_bitstream_frame *frame) {
    bit_reader r;
    if (bits == NULL || frame == NULL) {
        return G729_ERR_NULL;
    }

    r.buf = bits;
    r.bit_pos = 0;

    frame->l0 = bit_reader_read(&r, 1);
    frame->l1 = bit_reader_read(&r, 7);
    frame->l2 = bit_reader_read(&r, 5);
    frame->l3 = bit_reader_read(&r, 5);
    frame->p1 = bit_reader_read(&r, 8);
    frame->p0 = bit_reader_read(&r, 1);
    frame->c1 = bit_reader_read(&r, 13);
    frame->s1 = bit_reader_read(&r, 4);
    frame->ga1 = bit_reader_read(&r, 3);
    frame->gb1 = bit_reader_read(&r, 4);
    frame->p2 = bit_reader_read(&r, 5);
    frame->c2 = bit_reader_read(&r, 13);
    frame->s2 = bit_reader_read(&r, 4);
    frame->ga2 = bit_reader_read(&r, 3);
    frame->gb2 = bit_reader_read(&r, 4);

    return G729_OK;
}

uint16_t g729_bitstream_parity(uint16_t p1) {
    uint16_t x = (uint16_t)((p1 >> 2) & 0x3Fu);
    uint16_t p = 0;
    int i;
    for (i = 0; i < 6; ++i) {
        p = (uint16_t)(p ^ ((x >> (unsigned)i) & 1u));
    }
    return p;
}
