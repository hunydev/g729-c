#include "g729_gain.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/gain_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

int main(void) {
    g729_gain_decoder dec;
    int16_t code[G729_SUBFRAME_SAMPLES] = {0};
    g729_gain_indices idx;
    g729_gain_result got;
    int i;

    CHECK(g729_gain_quantized_prediction_error_q10(8360) == 179);
    CHECK(g729_gain_quantized_prediction_error_q10(7339) == -980);
    CHECK(g729_gain_quantized_prediction_error_q10(41438) == 14416);

    memset(&dec, 0, sizeof(dec));
    code[5] = 8192;
    idx.ga = 3;
    idx.gb = 7;
    got = g729_gain_decode(&dec, idx, code);
    CHECK(got.gp_q14 > 0);
    CHECK(got.gp_q14 <= 20000);
    CHECK(got.gc_mant_q14 > 0);
    CHECK(dec.initialized);

    memset(code, 0, sizeof(code));
    got = g729_gain_decode(&dec, idx, code);
    CHECK(got.gc_mant_q14 == 0);
    CHECK(got.gc_exp == 0);

    memset(&dec, 0, sizeof(dec));
    for (i = 0; i < GAIN_ORACLE_VECTOR_COUNT; ++i) {
        int j;
        idx.ga = GAIN_ORACLE_VECTORS[i].ga;
        idx.gb = GAIN_ORACLE_VECTORS[i].gb;
        for (j = 0; j < G729_SUBFRAME_SAMPLES; ++j) {
            code[j] = GAIN_ORACLE_VECTORS[i].code[j];
        }
        got = g729_gain_decode(&dec, idx, code);
        CHECK(got.gp_q14 == GAIN_ORACLE_VECTORS[i].gp_q14);
        CHECK(got.gc_mant_q14 == GAIN_ORACLE_VECTORS[i].gc_mant_q14);
        CHECK(got.gc_exp == GAIN_ORACLE_VECTORS[i].gc_exp);
        for (j = 0; j < 4; ++j) {
            CHECK(dec.past_errors[j] == GAIN_ORACLE_VECTORS[i].past_errors[j]);
        }
    }

    g729_gain_decoder_reset(&dec);
    CHECK(!dec.initialized);
    g729_gain_mark_erasure(&dec);
    CHECK(dec.initialized);
    CHECK(dec.past_errors[0] == G729_GAIN_PAST_ERROR_DEFAULT);

    return 0;
}
