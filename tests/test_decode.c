#include "g729.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/decode_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int frame_equal(const int16_t a[G729_FRAME_SAMPLES],
                       const int16_t b[G729_FRAME_SAMPLES]) {
    int i;
    for (i = 0; i < G729_FRAME_SAMPLES; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    int v;

    for (v = 0; v < DECODE_ORACLE_VECTOR_COUNT; ++v) {
        g729_decoder dec;
        int f;
        g729_decoder_init(&dec);
        for (f = 0; f < DECODE_ORACLE_FRAMES_PER_VECTOR; ++f) {
            int16_t pcm[G729_FRAME_SAMPLES];
            int rc = g729_decode_frame(&dec,
                                       DECODE_ORACLE_VECTORS[v].frames[f].bits,
                                       pcm);
            CHECK(rc == G729_OK);
            if (!frame_equal(pcm, DECODE_ORACLE_VECTORS[v].frames[f].pcm)) {
                fprintf(stderr, "decode mismatch: vector=%s frame=%d\n",
                        DECODE_ORACLE_VECTORS[v].name, f);
                return 1;
            }
        }
    }

    {
        g729_decoder warmed;
        g729_decoder fresh;
        int16_t scratch[G729_FRAME_SAMPLES];
        int16_t got[G729_FRAME_SAMPLES];
        int16_t want[G729_FRAME_SAMPLES];
        const uint8_t *bits = DECODE_ORACLE_VECTORS[2].frames[0].bits;

        g729_decoder_init(&warmed);
        CHECK(g729_decode_frame(&warmed, bits, scratch) == G729_OK);
        g729_decoder_reset(&warmed);
        CHECK(g729_decode_frame(&warmed, bits, got) == G729_OK);

        g729_decoder_init(&fresh);
        CHECK(g729_decode_frame(&fresh, bits, want) == G729_OK);
        CHECK(frame_equal(got, want));
    }

    {
        g729_decoder a;
        g729_decoder b;
        int16_t out_a[G729_FRAME_SAMPLES];
        int16_t out_b[G729_FRAME_SAMPLES];
        const uint8_t *bits = DECODE_ORACLE_VECTORS[3].frames[1].bits;

        g729_decoder_init(&a);
        g729_decoder_init(&b);
        CHECK(g729_decode_frame(&a, bits, out_a) == G729_OK);
        CHECK(g729_decode_frame(&b, bits, out_b) == G729_OK);
        CHECK(frame_equal(out_a, out_b));
    }

    return 0;
}
