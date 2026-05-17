#ifndef G729_CLOSEDLOOP_H
#define G729_CLOSEDLOOP_H

#include <stdint.h>

#include "g729_lpc.h"

#define G729_CLOSEDLOOP_SUBFRAME_LEN 40
#define G729_CLOSEDLOOP_PITCH_MIN 20
#define G729_CLOSEDLOOP_PITCH_MAX 143
#define G729_CLOSEDLOOP_PITCH_LINTER 10
#define G729_CLOSEDLOOP_SEARCH_HISTORY \
    (G729_CLOSEDLOOP_PITCH_MAX + G729_CLOSEDLOOP_PITCH_LINTER)
#define G729_CLOSEDLOOP_SEARCH_LEN \
    (G729_CLOSEDLOOP_SEARCH_HISTORY + G729_CLOSEDLOOP_SUBFRAME_LEN)
#define G729_CLOSEDLOOP_GP_UPPER_Q14 19661

typedef struct g729_closedloop_pitch_result {
    int16_t int_lag;
    int8_t frac;
    int32_t rn;
} g729_closedloop_pitch_result;

void g729_closedloop_lp_residual_subframe(
    const int16_t speech[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t a_hat[G729_LPC_ORDER + 1],
    const int16_t mem[G729_LPC_ORDER],
    int16_t residual[G729_CLOSEDLOOP_SUBFRAME_LEN]);

void g729_closedloop_target_signal(
    const int16_t a_hat[G729_LPC_ORDER + 1],
    const int16_t residual[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t sw_mem[G729_LPC_ORDER],
    int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN]);

void g729_closedloop_impulse_response(
    const int16_t a_hat[G729_LPC_ORDER + 1],
    int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN]);

void g729_closedloop_backward_filter(
    const int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN],
    int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN]);

void g729_closedloop_subframe1_window(int16_t top,
                                      int16_t *tmin,
                                      int16_t *tmax);
void g729_closedloop_subframe2_window(int16_t int_t1,
                                      int16_t *tmin,
                                      int16_t *tmax);

g729_closedloop_pitch_result g729_closedloop_search_integer(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t centre,
    int subframe);

int16_t g729_closedloop_interpolate3(
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int8_t frac);

int8_t g729_closedloop_refine_fraction(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int allow_frac);

void g729_closedloop_refine_fraction_subframe1(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int16_t *out_lag,
    int8_t *out_frac);

void g729_closedloop_refine_fraction_subframe2(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int16_t int_t1,
    int16_t *out_lag,
    int8_t *out_frac);

void g729_closedloop_adaptive_vector(
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int8_t frac,
    int16_t v[G729_CLOSEDLOOP_SUBFRAME_LEN]);

int16_t g729_closedloop_gp_and_y(
    const int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t v[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN],
    int16_t y[G729_CLOSEDLOOP_SUBFRAME_LEN]);

uint8_t g729_closedloop_encode_p1(int16_t int_lag, int8_t frac);
uint8_t g729_closedloop_encode_p2(int16_t int_lag,
                                  int8_t frac,
                                  int16_t tmin);
uint8_t g729_closedloop_encode_p0(uint8_t p1);

#endif
