#ifndef G729_FCB_H
#define G729_FCB_H

#include <stdint.h>

#define G729_SUBFRAME_SAMPLES 40
#define G729_FCB_PULSE_AMPLITUDE 8191
#define G729_FCB_NEGATIVE_PULSE_AMPLITUDE (-8192)
#define G729_FCB_INITIAL_PITCH_ENHANCEMENT_Q14 3277

typedef struct g729_fcb_indices {
    uint16_t positions;
    uint8_t signs;
} g729_fcb_indices;

void g729_fcb_decode_positions(uint16_t code, int positions[4]);
void g729_fcb_place_pulses(const int positions[4],
                           uint8_t signs,
                           int16_t code[G729_SUBFRAME_SAMPLES]);
int16_t g729_fcb_clamp_pitch_gain_for_enhancement(int16_t gp_prev_q14);
void g729_fcb_apply_pitch_enhancement(int16_t code[G729_SUBFRAME_SAMPLES],
                                      int t,
                                      int16_t beta_q14);
void g729_fcb_decode(g729_fcb_indices idx,
                     int t,
                     int16_t beta_q14,
                     int16_t code[G729_SUBFRAME_SAMPLES]);

#endif
