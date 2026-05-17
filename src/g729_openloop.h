#ifndef G729_OPENLOOP_H
#define G729_OPENLOOP_H

#include <stdint.h>

#include "g729.h"
#include "g729_lpc.h"

#define G729_OPENLOOP_OLD_WSPEECH_LEN 143
#define G729_OPENLOOP_WSPEECH_LEN \
    (G729_OPENLOOP_OLD_WSPEECH_LEN + G729_FRAME_SAMPLES)

typedef struct g729_openloop_range_score {
    int16_t lag;
    int32_t r;
    int32_t e;
} g729_openloop_range_score;

typedef struct g729_openloop_search_result {
    g729_openloop_range_score range1;
    g729_openloop_range_score range2;
    g729_openloop_range_score range3;
    int16_t top;
} g729_openloop_search_result;

g729_openloop_search_result
g729_openloop_search_with_ranges(
    const int16_t wsp[G729_OPENLOOP_WSPEECH_LEN]);

g729_openloop_search_result
g729_openloop_step_split_search(
    const int16_t a_hat_sf1[G729_LPC_ORDER + 1],
    const int16_t a_hat_sf2[G729_LPC_ORDER + 1],
    const int16_t speech[G729_FRAME_SAMPLES],
    int16_t residual_mem[G729_LPC_ORDER],
    int16_t sw_mem[G729_LPC_ORDER],
    int16_t old_wspeech[G729_OPENLOOP_OLD_WSPEECH_LEN]);

#endif
