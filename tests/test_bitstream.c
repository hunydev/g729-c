#include "g729_bitstream.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int bytes_equal(const uint8_t *a, const uint8_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int frame_equal(const g729_bitstream_frame *a,
                       const g729_bitstream_frame *b) {
    return a->l0 == b->l0 && a->l1 == b->l1 &&
           a->l2 == b->l2 && a->l3 == b->l3 &&
           a->p1 == b->p1 && a->p0 == b->p0 &&
           a->c1 == b->c1 && a->s1 == b->s1 &&
           a->ga1 == b->ga1 && a->gb1 == b->gb1 &&
           a->p2 == b->p2 && a->c2 == b->c2 &&
           a->s2 == b->s2 && a->ga2 == b->ga2 &&
           a->gb2 == b->gb2;
}

int main(void) {
    g729_bitstream_frame frame;
    g729_bitstream_frame got;
    uint8_t out[G729_FRAME_BYTES];
    uint8_t want[G729_FRAME_BYTES];
    int i;

    memset(&frame, 0, sizeof(frame));
    memset(out, 0xFF, sizeof(out));
    memset(want, 0, sizeof(want));
    CHECK(g729_bitstream_pack(&frame, out) == G729_OK);
    CHECK(bytes_equal(out, want, G729_FRAME_BYTES));

    frame.l0 = 1;
    memset(out, 0, sizeof(out));
    memset(want, 0, sizeof(want));
    want[0] = 0x80;
    CHECK(g729_bitstream_pack(&frame, out) == G729_OK);
    CHECK(bytes_equal(out, want, G729_FRAME_BYTES));

    memset(&frame, 0, sizeof(frame));
    frame.l1 = 0x55;
    memset(out, 0, sizeof(out));
    memset(want, 0, sizeof(want));
    want[0] = 0x55;
    CHECK(g729_bitstream_pack(&frame, out) == G729_OK);
    CHECK(bytes_equal(out, want, G729_FRAME_BYTES));

    frame.l0 = 1;
    frame.l1 = 0x7F;
    frame.l2 = 0x1F;
    frame.l3 = 0x1F;
    frame.p1 = 0xFF;
    frame.p0 = 1;
    frame.c1 = 0x1FFF;
    frame.s1 = 0xF;
    frame.ga1 = 7;
    frame.gb1 = 0xF;
    frame.p2 = 0x1F;
    frame.c2 = 0x1FFF;
    frame.s2 = 0xF;
    frame.ga2 = 7;
    frame.gb2 = 0xF;
    CHECK(g729_bitstream_pack(&frame, out) == G729_OK);
    for (i = 0; i < G729_FRAME_BYTES; ++i) {
        CHECK(out[i] == 0xFF);
    }
    memset(&got, 0, sizeof(got));
    CHECK(g729_bitstream_unpack(out, &got) == G729_OK);
    CHECK(frame_equal(&got, &frame));

    frame.l0 = 0;
    frame.l1 = 42;
    frame.l2 = 17;
    frame.l3 = 9;
    frame.p1 = 128;
    frame.p0 = 0;
    frame.c1 = 0x1234;
    frame.s1 = 5;
    frame.ga1 = 3;
    frame.gb1 = 11;
    frame.p2 = 7;
    frame.c2 = 0x0ABC;
    frame.s2 = 9;
    frame.ga2 = 2;
    frame.gb2 = 6;
    CHECK(g729_bitstream_pack(&frame, out) == G729_OK);
    memset(&got, 0, sizeof(got));
    CHECK(g729_bitstream_unpack(out, &got) == G729_OK);
    CHECK(frame_equal(&got, &frame));

    CHECK(g729_bitstream_pack(NULL, out) == G729_ERR_NULL);
    CHECK(g729_bitstream_pack(&frame, NULL) == G729_ERR_NULL);
    CHECK(g729_bitstream_unpack(NULL, &got) == G729_ERR_NULL);
    CHECK(g729_bitstream_unpack(out, NULL) == G729_ERR_NULL);

    CHECK(g729_bitstream_parity(0) == 0);
    CHECK(g729_bitstream_parity(0xFC) == 0);
    CHECK(g729_bitstream_parity(0x04) == 1);
    CHECK(g729_bitstream_parity(0x08) == 1);

    return 0;
}
