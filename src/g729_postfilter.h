#ifndef G729_POSTFILTER_H
#define G729_POSTFILTER_H

#include <stdint.h>

#include "g729_fcb.h"

#define G729_POSTFILTER_LPC_ORDER 10
#define G729_POSTFILTER_PITCH_MAX 143
#define G729_POSTFILTER_RESIDUAL_LEN \
    (G729_POSTFILTER_PITCH_MAX + G729_SUBFRAME_SAMPLES)

typedef struct g729_postfilter {
    int16_t past_s[G729_POSTFILTER_LPC_ORDER];
    int16_t past_residual[G729_POSTFILTER_RESIDUAL_LEN];
    int16_t past_synth_post[G729_POSTFILTER_LPC_ORDER];
    int16_t past_tilt_input;
    int32_t agc_gain_prev;
    int initialized;
} g729_postfilter;

void g729_postfilter_reset(g729_postfilter *pf);
void g729_postfilter_filter(g729_postfilter *pf,
                            const int16_t a[G729_POSTFILTER_LPC_ORDER + 1],
                            int t_int,
                            const int16_t s[G729_SUBFRAME_SAMPLES],
                            int16_t out[G729_SUBFRAME_SAMPLES]);

#endif
