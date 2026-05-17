#include "g729_hp.h"

#include <stddef.h>
#include <string.h>

#include "g729_fixed.h"

enum {
    HP_B0_Q13 = 7699,
    HP_B1_Q13 = -15398,
    HP_B2_Q13 = 7699,
    HP_FEEDBACK_A1_Q13 = 15836,
    HP_FEEDBACK_A2_Q13 = -7667
};

static void hp_l_extract(int32_t x, int16_t *hi, int16_t *lo) {
    int16_t h = g729_extract_h(x);
    int32_t l = g729_l_msu(g729_l_shr(x, 1), h, 16384);
    *hi = h;
    *lo = g729_extract_l(l);
}

static int32_t hp_mpy32_16(int32_t x, int16_t n) {
    int16_t hi;
    int16_t lo;
    hp_l_extract(x, &hi, &lo);
    return g729_l_mac(g729_l_mult(hi, n), g729_mult(lo, n), 1);
}

static int16_t hp_final_from_acc_native(int32_t acc) {
    return g729_round(g729_l_shl(acc, 1));
}

void g729_hp_reset(g729_hp_filter_state *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void g729_hp_filter(g729_hp_filter_state *state,
                    const int16_t in[G729_SUBFRAME_SAMPLES],
                    int16_t pre[G729_SUBFRAME_SAMPLES],
                    int16_t final[G729_SUBFRAME_SAMPLES]) {
    int n;
    int16_t x1;
    int16_t x2;
    int32_t y1;
    int32_t y2;

    if (state == NULL || in == NULL) {
        return;
    }

    x1 = state->x[0];
    x2 = state->x[1];
    y1 = state->y[0];
    y2 = state->y[1];

    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int16_t xn = in[n];
        int32_t ff = g729_l_mult(xn, HP_B0_Q13);
        int32_t fb;
        int32_t acc;

        ff = g729_l_mac(ff, x1, HP_B1_Q13);
        ff = g729_l_mac(ff, x2, HP_B2_Q13);

        fb = g729_l_add(hp_mpy32_16(y1, HP_FEEDBACK_A1_Q13),
                        hp_mpy32_16(y2, HP_FEEDBACK_A2_Q13));
        acc = g729_l_shl(g729_l_add(ff, fb), 2);

        if (pre != NULL) {
            pre[n] = g729_round(acc);
        }
        if (final != NULL) {
            final[n] = hp_final_from_acc_native(acc);
        }

        x2 = x1;
        x1 = xn;
        y2 = y1;
        y1 = acc;
    }

    state->x[0] = x1;
    state->x[1] = x2;
    state->y[0] = y1;
    state->y[1] = y2;
}

void g729_hp_filter_pre(g729_hp_filter_state *state,
                        const int16_t in[G729_SUBFRAME_SAMPLES],
                        int16_t pre[G729_SUBFRAME_SAMPLES]) {
    g729_hp_filter(state, in, pre, NULL);
}

void g729_hp_filter_final(g729_hp_filter_state *state,
                          const int16_t in[G729_SUBFRAME_SAMPLES],
                          int16_t final[G729_SUBFRAME_SAMPLES]) {
    g729_hp_filter(state, in, NULL, final);
}
