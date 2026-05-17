#include "g729_synth.h"

#include <stddef.h>
#include <string.h>

#include "g729_fixed.h"

void g729_synth_reset(g729_synthesizer *synth) {
    if (synth == NULL) {
        return;
    }
    memset(synth, 0, sizeof(*synth));
}

void g729_synth_build_excitation(int16_t gp_q14,
                                 int16_t gc_mant_q14,
                                 int8_t gc_exp,
                                 const int16_t v[G729_SUBFRAME_SAMPLES],
                                 const int16_t c[G729_SUBFRAME_SAMPLES],
                                 int16_t u[G729_SUBFRAME_SAMPLES]) {
    int n;
    int shift_r = 13 - (int)gc_exp;
    if (u == NULL) {
        return;
    }
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int16_t vn = v == NULL ? 0 : v[n];
        int16_t cn = c == NULL ? 0 : c[n];
        int32_t l_pitch = g729_l_mult(gp_q14, vn);
        int32_t l_code = 0;
        int32_t l_sum;
        if (gc_mant_q14 != 0) {
            int32_t prod = g729_l_mult(gc_mant_q14, cn);
            if (shift_r >= 0) {
                l_code = g729_l_shr(prod, (int16_t)shift_r);
            } else {
                l_code = g729_l_shl(prod, (int16_t)(-shift_r));
            }
        }
        l_sum = g729_l_add(l_pitch, l_code);
        u[n] = g729_round(g729_l_shl(l_sum, 1));
    }
}

static void one_pass(const int16_t a[11],
                     const int16_t u[G729_SUBFRAME_SAMPLES],
                     int16_t work[50]) {
    int n;
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int i;
        int32_t l_temp = g729_l_mult(u[n], a[0]);
        for (i = 1; i <= 10; ++i) {
            l_temp = g729_l_msu(l_temp, a[i], work[10 + n - i]);
        }
        l_temp = g729_l_shl(l_temp, 3);
        work[10 + n] = g729_round(l_temp);
    }
}

void g729_synth_filter(g729_synthesizer *synth,
                       const int16_t a[11],
                       const int16_t u[G729_SUBFRAME_SAMPLES],
                       int16_t out[G729_SUBFRAME_SAMPLES]) {
    int16_t work[50];
    int i;
    if (synth == NULL || a == NULL || u == NULL || out == NULL) {
        return;
    }
    synth->last_excitation_scale_shift = 0;
    for (i = 0; i < 10; ++i) {
        work[i] = synth->past_synth[i];
    }
    for (i = 10; i < 50; ++i) {
        work[i] = 0;
    }

    g729_fixed_clear_overflow();
    one_pass(a, u, work);
    if (!g729_fixed_overflow()) {
        memcpy(out, &work[10], G729_SUBFRAME_SAMPLES * sizeof(out[0]));
        memcpy(synth->past_synth, &work[40], 10 * sizeof(synth->past_synth[0]));
        return;
    }

    {
        int16_t work2[50];
        int16_t u_scaled[G729_SUBFRAME_SAMPLES];
        synth->last_excitation_scale_shift = 2;
        for (i = 0; i < 10; ++i) {
            work2[i] = synth->past_synth[i];
        }
        for (i = 10; i < 50; ++i) {
            work2[i] = 0;
        }
        for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
            u_scaled[i] = (int16_t)g729_l_shr(u[i], 2);
        }
        g729_fixed_clear_overflow();
        one_pass(a, u_scaled, work2);
        memcpy(out, &work2[10], G729_SUBFRAME_SAMPLES * sizeof(out[0]));
        memcpy(synth->past_synth, &work2[40], 10 * sizeof(synth->past_synth[0]));
    }
}

void g729_synth_synthesize(g729_synthesizer *synth,
                           const int16_t a[11],
                           const int16_t v[G729_SUBFRAME_SAMPLES],
                           const int16_t c[G729_SUBFRAME_SAMPLES],
                           int16_t gp_q14,
                           int16_t gc_mant_q14,
                           int8_t gc_exp,
                           int16_t out[G729_SUBFRAME_SAMPLES]) {
    int16_t u[G729_SUBFRAME_SAMPLES];
    if (out == NULL) {
        return;
    }
    g729_synth_build_excitation(gp_q14, gc_mant_q14, gc_exp, v, c, u);
    g729_synth_filter(synth, a, u, out);
}
