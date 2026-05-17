#ifndef G729_FCB_SEARCH_H
#define G729_FCB_SEARCH_H

#include <stdint.h>

#include "g729_fcb.h"

void g729_fcb_adjusted_target(const int16_t x[G729_SUBFRAME_SAMPLES],
                              const int16_t y[G729_SUBFRAME_SAMPLES],
                              int16_t gp_q14,
                              int16_t x_prime[G729_SUBFRAME_SAMPLES]);
void g729_fcb_correlation_d(const int16_t x_prime[G729_SUBFRAME_SAMPLES],
                            const int16_t h[G729_SUBFRAME_SAMPLES],
                            int32_t d[G729_SUBFRAME_SAMPLES]);
void g729_fcb_signs_from_d(const int32_t d[G729_SUBFRAME_SAMPLES],
                           int16_t signs[G729_SUBFRAME_SAMPLES],
                           int32_t d_abs[G729_SUBFRAME_SAMPLES]);
void g729_fcb_phi_prime(
    const int16_t h[G729_SUBFRAME_SAMPLES],
    const int16_t signs[G729_SUBFRAME_SAMPLES],
    int32_t phi[G729_SUBFRAME_SAMPLES][G729_SUBFRAME_SAMPLES]);
void g729_fcb_search_depth_first(
    const int32_t d_abs[G729_SUBFRAME_SAMPLES],
    int32_t phi[G729_SUBFRAME_SAMPLES][G729_SUBFRAME_SAMPLES],
    int8_t positions[4],
    int64_t sum_out[2]);
int g729_fcb_search_depth_first_threshold_scan(
    const int32_t d_abs[G729_SUBFRAME_SAMPLES],
    int32_t phi[G729_SUBFRAME_SAMPLES][G729_SUBFRAME_SAMPLES],
    int8_t positions[4],
    int64_t sum_out[2],
    int limit);
void g729_fcb_build_sparse_code(
    const int8_t positions[4],
    const int16_t signs[G729_SUBFRAME_SAMPLES],
    int16_t code[G729_SUBFRAME_SAMPLES]);
void g729_fcb_build_code(const int8_t positions[4],
                         const int16_t signs[G729_SUBFRAME_SAMPLES],
                         int16_t int_lag,
                         int16_t prev_gp_q14,
                         int16_t code[G729_SUBFRAME_SAMPLES]);
void g729_fcb_filter_code(const int16_t code[G729_SUBFRAME_SAMPLES],
                          const int16_t h[G729_SUBFRAME_SAMPLES],
                          int16_t z[G729_SUBFRAME_SAMPLES]);
uint8_t g729_fcb_pack_s(const int8_t positions[4],
                        const int16_t signs[G729_SUBFRAME_SAMPLES]);
uint16_t g729_fcb_pack_c(const int8_t positions[4]);

#endif
