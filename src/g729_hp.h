#ifndef G729_HP_H
#define G729_HP_H

#include <stdint.h>

#include "g729_fcb.h"

typedef struct g729_hp_filter_state {
    int16_t x[2];
    int32_t y[2];
} g729_hp_filter_state;

void g729_hp_reset(g729_hp_filter_state *state);
void g729_hp_filter(g729_hp_filter_state *state,
                    const int16_t in[G729_SUBFRAME_SAMPLES],
                    int16_t pre[G729_SUBFRAME_SAMPLES],
                    int16_t final[G729_SUBFRAME_SAMPLES]);
void g729_hp_filter_pre(g729_hp_filter_state *state,
                        const int16_t in[G729_SUBFRAME_SAMPLES],
                        int16_t pre[G729_SUBFRAME_SAMPLES]);
void g729_hp_filter_final(g729_hp_filter_state *state,
                          const int16_t in[G729_SUBFRAME_SAMPLES],
                          int16_t final[G729_SUBFRAME_SAMPLES]);

#endif
