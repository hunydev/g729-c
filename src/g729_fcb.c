#include "g729_fcb.h"

#include <stddef.h>

#include "g729_fixed.h"

enum {
    BETA_LOWER_Q14 = 3277,
    BETA_UPPER_Q14 = 13017
};

void g729_fcb_decode_positions(uint16_t code, int positions[4]) {
    int i0;
    int i1;
    int i2;
    int jx;
    int i3;
    if (positions == NULL) {
        return;
    }
    i0 = (int)(code & 0x07u);
    i1 = (int)((code >> 3) & 0x07u);
    i2 = (int)((code >> 6) & 0x07u);
    jx = (int)((code >> 9) & 0x01u);
    i3 = (int)((code >> 10) & 0x07u);
    positions[0] = 5 * i0;
    positions[1] = 5 * i1 + 1;
    positions[2] = 5 * i2 + 2;
    positions[3] = 5 * i3 + 3 + jx;
}

void g729_fcb_place_pulses(const int positions[4],
                           uint8_t signs,
                           int16_t code[G729_SUBFRAME_SAMPLES]) {
    int i;
    if (positions == NULL || code == NULL) {
        return;
    }
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        code[i] = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (((signs >> (unsigned)i) & 1u) != 0u) {
            code[positions[i]] = G729_FCB_PULSE_AMPLITUDE;
        } else {
            code[positions[i]] = G729_FCB_NEGATIVE_PULSE_AMPLITUDE;
        }
    }
}

int16_t g729_fcb_clamp_pitch_gain_for_enhancement(int16_t gp_prev_q14) {
    if (gp_prev_q14 < BETA_LOWER_Q14) {
        return BETA_LOWER_Q14;
    }
    if (gp_prev_q14 > BETA_UPPER_Q14) {
        return BETA_UPPER_Q14;
    }
    return gp_prev_q14;
}

void g729_fcb_apply_pitch_enhancement(int16_t code[G729_SUBFRAME_SAMPLES],
                                      int t,
                                      int16_t beta_q14) {
    int n;
    if (code == NULL || t < 1 || t >= G729_SUBFRAME_SAMPLES || beta_q14 == 0) {
        return;
    }
    for (n = t; n < G729_SUBFRAME_SAMPLES; ++n) {
        int32_t prod = g729_l_mult(beta_q14, code[n - t]);
        int16_t delta;
        prod = g729_l_shl(prod, 1);
        delta = g729_extract_h(prod);
        code[n] = g729_add(code[n], delta);
    }
}

void g729_fcb_decode(g729_fcb_indices idx,
                     int t,
                     int16_t beta_q14,
                     int16_t code[G729_SUBFRAME_SAMPLES]) {
    int positions[4];
    if (code == NULL) {
        return;
    }
    g729_fcb_decode_positions(idx.positions, positions);
    g729_fcb_place_pulses(positions, idx.signs, code);
    g729_fcb_apply_pitch_enhancement(code, t, beta_q14);
}
