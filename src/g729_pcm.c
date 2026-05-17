#include "g729_pcm.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "g729_fixed.h"

enum {
    PRE_A1_Q13 = 15614,
    PRE_A2_Q13 = -7466,
    PRE_B0_Q13 = 3798,
    PRE_B1_Q13 = -7596,
    PRE_B2_Q13 = 3798,
    PRE_BQ = 13
};

static int64_t shr64_floor(int64_t x, unsigned n) {
    uint64_t mag;
    uint64_t denom;
    if (n == 0u) {
        return x;
    }
    if (x >= 0) {
        return (int64_t)((uint64_t)x >> n);
    }
    mag = (uint64_t)(-(x + 1)) + 1u;
    denom = (uint64_t)1 << n;
    return -(int64_t)((mag + denom - 1u) / denom);
}

static int32_t saturate32_i64(int64_t x) {
    if (x > (int64_t)G729_MAX32) {
        return G729_MAX32;
    }
    if (x < (int64_t)G729_MIN32) {
        return G729_MIN32;
    }
    return (int32_t)x;
}

static int32_t scale_feedback(int16_t a, int32_t y) {
    int64_t p = shr64_floor((int64_t)a * (int64_t)y, PRE_BQ);
    return saturate32_i64(p);
}

void g729_preprocessor_reset(g729_preprocessor *pre) {
    if (pre == NULL) {
        return;
    }
    memset(pre, 0, sizeof(*pre));
}

void g729_preprocessor_process(g729_preprocessor *pre,
                               const int16_t *in,
                               int16_t *out,
                               size_t len) {
    size_t i;
    if (pre == NULL || in == NULL || out == NULL) {
        return;
    }

    for (i = 0; i < len; ++i) {
        int16_t x0 = in[i];
        int32_t acc = 0;
        int16_t y0;

        acc = g729_l_mac(acc, PRE_B0_Q13, x0);
        acc = g729_l_mac(acc, PRE_B1_Q13, (int16_t)pre->x1);
        acc = g729_l_mac(acc, PRE_B2_Q13, (int16_t)pre->x2);

        acc = g729_l_add(acc, scale_feedback(PRE_A1_Q13, pre->y1));
        acc = g729_l_add(acc, scale_feedback(PRE_A2_Q13, pre->y2));

        y0 = g729_round(g729_l_shl(acc, 2));
        out[i] = y0;

        pre->x2 = pre->x1;
        pre->x1 = x0;
        pre->y2 = pre->y1;
        pre->y1 = acc;
    }
}

void g729_preprocessor_process_frame(g729_preprocessor *pre,
                                     const int16_t in[G729_FRAME_SAMPLES],
                                     int16_t out[G729_FRAME_SAMPLES]) {
    g729_preprocessor_process(pre, in, out, G729_FRAME_SAMPLES);
}
