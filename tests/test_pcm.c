#include "g729_pcm.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/pcm_oracle_vectors.h"

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

    for (v = 0; v < PCM_ORACLE_VECTOR_COUNT; ++v) {
        g729_preprocessor pre;
        int frame;
        memset(&pre, 0, sizeof(pre));
        for (frame = 0; frame < PCM_ORACLE_FRAME_COUNT; ++frame) {
            int16_t out[G729_FRAME_SAMPLES];
            g729_preprocessor_process_frame(&pre,
                                            PCM_ORACLE_VECTORS[v].in[frame],
                                            out);
            if (!frame_equal(out, PCM_ORACLE_VECTORS[v].out[frame])) {
                fprintf(stderr, "preprocessor mismatch: vector=%s frame=%d\n",
                        PCM_ORACLE_VECTORS[v].name, frame);
                return 1;
            }
        }
    }

    {
        g729_preprocessor pre;
        int16_t out[G729_FRAME_SAMPLES];
        memset(&pre, 0x67, sizeof(pre));
        g729_preprocessor_reset(&pre);
        CHECK(pre.x1 == 0);
        CHECK(pre.x2 == 0);
        CHECK(pre.y1 == 0);
        CHECK(pre.y2 == 0);
        g729_preprocessor_process_frame(&pre, PCM_ORACLE_VECTORS[0].in[0], out);
        CHECK(frame_equal(out, PCM_ORACLE_VECTORS[0].out[0]));
    }

    {
        g729_preprocessor pre;
        int16_t aliased[G729_FRAME_SAMPLES];
        memset(&pre, 0, sizeof(pre));
        memcpy(aliased, PCM_ORACLE_VECTORS[5].in[0], sizeof(aliased));
        g729_preprocessor_process(&pre, aliased, aliased, G729_FRAME_SAMPLES);
        CHECK(frame_equal(aliased, PCM_ORACLE_VECTORS[5].out[0]));
    }

    return 0;
}
