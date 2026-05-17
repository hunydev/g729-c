#include "g729.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/decode_oracle_vectors.h"
#include "fixtures/encode_oracle_vectors.h"

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

static int first_mismatch(const int16_t a[G729_FRAME_SAMPLES],
                          const int16_t b[G729_FRAME_SAMPLES]) {
    int i;
    for (i = 0; i < G729_FRAME_SAMPLES; ++i) {
        if (a[i] != b[i]) {
            return i;
        }
    }
    return -1;
}

int main(void) {
    int v;

    CHECK(ENCODE_ORACLE_VECTOR_COUNT == DECODE_ORACLE_VECTOR_COUNT);
    CHECK(ENCODE_ORACLE_FRAMES_PER_VECTOR == DECODE_ORACLE_FRAMES_PER_VECTOR);

    for (v = 0; v < ENCODE_ORACLE_VECTOR_COUNT; ++v) {
        g729_encoder enc;
        g729_decoder dec;
        int f;

        CHECK(strcmp(ENCODE_ORACLE_VECTORS[v].name,
                     DECODE_ORACLE_VECTORS[v].name) == 0);

        g729_encoder_init(&enc);
        g729_decoder_init(&dec);

        for (f = 0; f < ENCODE_ORACLE_FRAMES_PER_VECTOR; ++f) {
            uint8_t bits[G729_FRAME_BYTES];
            int16_t pcm[G729_FRAME_SAMPLES];
            int rc;

            rc = g729_encode_frame(&enc,
                                   ENCODE_ORACLE_VECTORS[v].frames[f].pcm,
                                   bits);
            CHECK(rc == G729_OK);
            rc = g729_decode_frame(&dec, bits, pcm);
            CHECK(rc == G729_OK);

            if (!frame_equal(pcm, DECODE_ORACLE_VECTORS[v].frames[f].pcm)) {
                int index = first_mismatch(
                    pcm, DECODE_ORACLE_VECTORS[v].frames[f].pcm);
                fprintf(stderr,
                        "loopback mismatch: vector=%s frame=%d sample=%d got=%d want=%d\n",
                        ENCODE_ORACLE_VECTORS[v].name,
                        f,
                        index,
                        (int)pcm[index],
                        (int)DECODE_ORACLE_VECTORS[v].frames[f].pcm[index]);
                return 1;
            }
        }
    }

    return 0;
}
