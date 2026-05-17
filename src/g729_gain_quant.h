#ifndef G729_GAIN_QUANT_H
#define G729_GAIN_QUANT_H

#include <stdint.h>

#include "g729_fcb.h"

#define G729_GAIN_QUANT_EXC_MEM_SAMPLES 154
#define G729_GAIN_QUANT_GP_CLIP_Q14 ((int16_t)15565)
#define G729_GAIN_QUANT_TAME_ENERGY_THRESHOLD_Q0 ((int64_t)1 << 33)

typedef struct g729_gain_quant_search_result {
    uint8_t ga;
    uint8_t gb;
    uint8_t ga_bits;
    uint8_t gb_bits;
    int16_t gp_q14;
    int32_t gamma_c_q13;
} g729_gain_quant_search_result;

typedef struct g729_gain_quant_reconstruct_result {
    int16_t gp_q14;
    int16_t gc_mant_q14;
    int8_t gc_exp;
} g729_gain_quant_reconstruct_result;

int32_t g729_gain_quant_predicted_gc_q12(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES]);
int32_t g729_gain_quant_predicted_gc_q12_wide(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES]);
g729_gain_quant_search_result g729_gain_quant_search_conjugate(
    const int16_t x[G729_SUBFRAME_SAMPLES],
    const int16_t y[G729_SUBFRAME_SAMPLES],
    const int16_t z[G729_SUBFRAME_SAMPLES],
    int32_t gpc_pred_q12);
g729_gain_quant_search_result
g729_gain_quant_search_conjugate_target_bits(
    const int16_t x[G729_SUBFRAME_SAMPLES],
    const int16_t y[G729_SUBFRAME_SAMPLES],
    const int16_t z[G729_SUBFRAME_SAMPLES],
    int32_t gpc_pred_q12,
    unsigned target_bits);
g729_gain_quant_search_result g729_gain_quant_search_conjugate_float_center(
    const int16_t x[G729_SUBFRAME_SAMPLES],
    const int16_t y[G729_SUBFRAME_SAMPLES],
    const int16_t z[G729_SUBFRAME_SAMPLES],
    int32_t gpc_pred_q12);
g729_gain_quant_reconstruct_result g729_gain_quant_reconstruct(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES],
    uint8_t ga,
    uint8_t gb);
g729_gain_quant_reconstruct_result g729_gain_quant_reconstruct_wide(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES],
    uint8_t ga,
    uint8_t gb);
void g729_gain_quant_update_past_qua_en(int16_t past_qua_en[4],
                                        int32_t gamma_c_q13);
int16_t g729_gain_quant_tame(
    int16_t gp_q14,
    const int16_t old_exc[G729_GAIN_QUANT_EXC_MEM_SAMPLES]);
void g729_gain_quant_pack_gains(uint8_t ga,
                                uint8_t gb,
                                uint8_t *ga_bits,
                                uint8_t *gb_bits);

#endif
