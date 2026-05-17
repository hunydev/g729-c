#include "g729_pitch.h"

#include <stddef.h>

#include "g729_fixed.h"

enum {
    LINTER = 10
};

static const int16_t pitch_interp_fir[31] = {
    29443,
    25207, 14701, 3143,
    -4402, -5850, -2783,
    1211, 3130, 2259,
    0, -1652, -1666,
    -464, 756, 1099,
    550, -245, -634,
    -451, 0, 308,
    296, 78, -120,
    -165, -79, 34,
    91, 70, 0,
};

g729_pitch_delay g729_pitch_decode_subframe1(uint8_t p1) {
    g729_pitch_delay out;
    if (p1 < 197u) {
        int x = (int)p1 + 2;
        out.t_int = 19 + x / 3;
        out.t_frac = x % 3 - 1;
        return out;
    }
    out.t_int = (int)p1 - 112;
    out.t_frac = 0;
    return out;
}

g729_pitch_delay g729_pitch_decode_subframe2(uint8_t p2, int t1_int) {
    int t_min = t1_int - 5;
    int y;
    g729_pitch_delay out;
    if (t_min < 20) {
        t_min = 20;
    } else if (t_min > 134) {
        t_min = 134;
    }
    y = (int)p2 + 2;
    out.t_int = t_min + y / 3 - 1;
    out.t_frac = y % 3 - 1;
    return out;
}

uint8_t g729_pitch_parity(uint8_t p1) {
    uint8_t bits = (uint8_t)((p1 >> 2) & 0x3Fu);
    uint8_t x = (uint8_t)(bits ^ (bits >> 4));
    x = (uint8_t)(x ^ (x >> 2));
    x = (uint8_t)(x ^ (x >> 1));
    return (uint8_t)((x & 1u) ^ 1u);
}

int g729_pitch_check_parity(uint8_t p1, uint8_t p0) {
    return (int)((p0 & 1u) == g729_pitch_parity(p1));
}

static int16_t adaptive_source(int relative,
                               const int16_t *past_exc,
                               int past_exc_len,
                               const int16_t v[G729_SUBFRAME_SAMPLES]) {
    if (relative < 0) {
        int idx = past_exc_len + relative;
        if (past_exc != NULL && idx >= 0 && idx < past_exc_len) {
            return past_exc[idx];
        }
        return 0;
    }
    if (relative < G729_SUBFRAME_SAMPLES) {
        return v[relative];
    }
    return 0;
}

void g729_pitch_adaptive_codebook(int t_int,
                                  int t_frac,
                                  const int16_t *past_exc,
                                  int past_exc_len,
                                  int16_t v[G729_SUBFRAME_SAMPLES]) {
    int k;
    int pos_phase;
    int neg_phase;
    int n;
    if (v == NULL) {
        return;
    }
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        v[n] = 0;
    }

    if (t_frac == 0) {
        k = t_int;
        pos_phase = 0;
        neg_phase = 3;
    } else if (t_frac < 0) {
        k = t_int;
        pos_phase = 1;
        neg_phase = 2;
    } else {
        k = t_int + 1;
        pos_phase = 2;
        neg_phase = 1;
    }

    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int i;
        int32_t acc = 0;
        for (i = 0; i < LINTER; ++i) {
            int16_t back = adaptive_source(n - k - i, past_exc, past_exc_len, v);
            int16_t fwd = adaptive_source(n - k + 1 + i, past_exc, past_exc_len, v);
            acc = g729_l_mac(acc, pitch_interp_fir[pos_phase + 3 * i], back);
            acc = g729_l_mac(acc, pitch_interp_fir[neg_phase + 3 * i], fwd);
        }
        v[n] = g729_round(acc);
    }
}
