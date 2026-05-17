#ifndef G729_PITCH_H
#define G729_PITCH_H

#include <stdint.h>

#include "g729_fcb.h"

typedef struct g729_pitch_delay {
    int t_int;
    int t_frac;
} g729_pitch_delay;

g729_pitch_delay g729_pitch_decode_subframe1(uint8_t p1);
g729_pitch_delay g729_pitch_decode_subframe2(uint8_t p2, int t1_int);
uint8_t g729_pitch_parity(uint8_t p1);
int g729_pitch_check_parity(uint8_t p1, uint8_t p0);
void g729_pitch_adaptive_codebook(int t_int,
                                  int t_frac,
                                  const int16_t *past_exc,
                                  int past_exc_len,
                                  int16_t v[G729_SUBFRAME_SAMPLES]);

#endif
