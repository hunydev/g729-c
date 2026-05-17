#include "g729.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/encode_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int bits_equal(const uint8_t a[G729_FRAME_BYTES],
                      const uint8_t b[G729_FRAME_BYTES]) {
    int i;
    for (i = 0; i < G729_FRAME_BYTES; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static void print_bits(const uint8_t bits[G729_FRAME_BYTES]) {
    int i;
    for (i = 0; i < G729_FRAME_BYTES; ++i) {
        fprintf(stderr, "%02x", bits[i]);
    }
}

int main(void) {
    int v;

    for (v = 0; v < ENCODE_ORACLE_VECTOR_COUNT; ++v) {
        g729_encoder enc;
        int f;
        g729_encoder_init(&enc);
        for (f = 0; f < ENCODE_ORACLE_FRAMES_PER_VECTOR; ++f) {
            uint8_t bits[G729_FRAME_BYTES];
            int rc = g729_encode_frame(&enc,
                                       ENCODE_ORACLE_VECTORS[v].frames[f].pcm,
                                       bits);
            CHECK(rc == G729_OK);
            if (!bits_equal(bits, ENCODE_ORACLE_VECTORS[v].frames[f].bits)) {
                fprintf(stderr, "encode mismatch: vector=%s frame=%d got=",
                        ENCODE_ORACLE_VECTORS[v].name, f);
                print_bits(bits);
                fprintf(stderr, " want=");
                print_bits(ENCODE_ORACLE_VECTORS[v].frames[f].bits);
                fprintf(stderr, "\n");
                return 1;
            }
        }
    }

    {
        g729_encoder warmed;
        g729_encoder fresh;
        uint8_t scratch[G729_FRAME_BYTES];
        uint8_t got[G729_FRAME_BYTES];
        uint8_t want[G729_FRAME_BYTES];
        const int16_t *pcm = ENCODE_ORACLE_VECTORS[2].frames[0].pcm;

        g729_encoder_init(&warmed);
        CHECK(g729_encode_frame(&warmed, pcm, scratch) == G729_OK);
        g729_encoder_reset(&warmed);
        CHECK(g729_encode_frame(&warmed, pcm, got) == G729_OK);

        g729_encoder_init(&fresh);
        CHECK(g729_encode_frame(&fresh, pcm, want) == G729_OK);
        CHECK(bits_equal(got, want));
    }

    {
        g729_encoder a;
        g729_encoder b;
        uint8_t out_a[G729_FRAME_BYTES];
        uint8_t out_b[G729_FRAME_BYTES];
        const int16_t *pcm = ENCODE_ORACLE_VECTORS[3].frames[1].pcm;

        g729_encoder_init(&a);
        g729_encoder_init(&b);
        CHECK(g729_encode_frame(&a, pcm, out_a) == G729_OK);
        CHECK(g729_encode_frame(&b, pcm, out_b) == G729_OK);
        CHECK(bits_equal(out_a, out_b));
    }

    return 0;
}
