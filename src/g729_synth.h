#ifndef G729_SYNTH_H
#define G729_SYNTH_H

#include <stdint.h>

#include "g729_fcb.h"

typedef struct g729_synthesizer {
    int16_t past_synth[10];
    unsigned last_excitation_scale_shift;
} g729_synthesizer;

void g729_synth_reset(g729_synthesizer *synth);
void g729_synth_build_excitation(int16_t gp_q14,
                                 int16_t gc_mant_q14,
                                 int8_t gc_exp,
                                 const int16_t v[G729_SUBFRAME_SAMPLES],
                                 const int16_t c[G729_SUBFRAME_SAMPLES],
                                 int16_t u[G729_SUBFRAME_SAMPLES]);
void g729_synth_filter(g729_synthesizer *synth,
                       const int16_t a[11],
                       const int16_t u[G729_SUBFRAME_SAMPLES],
                       int16_t out[G729_SUBFRAME_SAMPLES]);
void g729_synth_synthesize(g729_synthesizer *synth,
                           const int16_t a[11],
                           const int16_t v[G729_SUBFRAME_SAMPLES],
                           const int16_t c[G729_SUBFRAME_SAMPLES],
                           int16_t gp_q14,
                           int16_t gc_mant_q14,
                           int8_t gc_exp,
                           int16_t out[G729_SUBFRAME_SAMPLES]);

#endif
