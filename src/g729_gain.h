#ifndef G729_GAIN_H
#define G729_GAIN_H

#include <stdint.h>

#include "g729_fcb.h"

#define G729_GAIN_PAST_ERROR_DEFAULT (-14336)

typedef struct g729_gain_indices {
    uint8_t ga;
    uint8_t gb;
} g729_gain_indices;

typedef struct g729_gain_decoder {
    int16_t past_errors[4];
    int initialized;
} g729_gain_decoder;

typedef struct g729_gain_result {
    int16_t gp_q14;
    int16_t gc_mant_q14;
    int8_t gc_exp;
} g729_gain_result;

void g729_gain_decoder_reset(g729_gain_decoder *dec);
void g729_gain_mark_erasure(g729_gain_decoder *dec);
g729_gain_result g729_gain_decode(g729_gain_decoder *dec,
                                  g729_gain_indices idx,
                                  const int16_t code[G729_SUBFRAME_SAMPLES]);
int16_t g729_gain_quantized_prediction_error_q10(int32_t gamma_c_q13);

#endif
