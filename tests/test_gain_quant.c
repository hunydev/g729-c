#include "g729_gain_quant.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "g729_gain.h"
#include "fixtures/gain_quant_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int vec16_equal(const int16_t *a, const int16_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int search_equal(g729_gain_quant_search_result got,
                        gain_quant_search_oracle want) {
    return got.ga == want.ga && got.gb == want.gb &&
           got.ga_bits == want.ga_bits && got.gb_bits == want.gb_bits &&
           got.gp_q14 == want.gp_q14 &&
           got.gamma_c_q13 == want.gamma_c_q13;
}

static int recon_equal(g729_gain_quant_reconstruct_result got,
                       gain_quant_recon_oracle want) {
    return got.gp_q14 == want.gp_q14 &&
           got.gc_mant_q14 == want.gc_mant_q14 &&
           got.gc_exp == want.gc_exp;
}

int main(void) {
    int i;
    uint8_t ga_bits = 0;
    uint8_t gb_bits = 0;
    int16_t past_zero_gamma[4] = {1, 2, 3, 4};

    g729_gain_quant_pack_gains(0, 0, &ga_bits, &gb_bits);
    CHECK(ga_bits == 5);
    CHECK(gb_bits == 4);
    CHECK(g729_gain_quant_predicted_gc_q12(NULL, NULL) == 0);
    CHECK(g729_gain_quant_tame(1234, NULL) == 1234);

    g729_gain_quant_update_past_qua_en(past_zero_gamma, 0);
    CHECK(past_zero_gamma[0] == G729_GAIN_PAST_ERROR_DEFAULT);
    CHECK(past_zero_gamma[1] == 1);
    CHECK(past_zero_gamma[2] == 2);
    CHECK(past_zero_gamma[3] == 3);

    for (i = 0; i < GAIN_QUANT_ORACLE_VECTOR_COUNT; ++i) {
        const gain_quant_oracle_vector *tc = &GAIN_QUANT_ORACLE_VECTORS[i];
        g729_gain_quant_search_result search;
        g729_gain_quant_search_result target_search;
        g729_gain_quant_search_result float_search;
        g729_gain_quant_reconstruct_result recon;
        g729_gain_quant_reconstruct_result recon_wide;
        int16_t past[4];

        CHECK(g729_gain_quant_predicted_gc_q12(tc->past, tc->code) ==
              tc->pred_gc_q12);
        CHECK(g729_gain_quant_predicted_gc_q12_wide(tc->past, tc->code) ==
              tc->pred_gc_q12_wide);

        search = g729_gain_quant_search_conjugate(tc->x, tc->y, tc->z,
                                                  tc->gpc_pred_q12);
        CHECK(search_equal(search, tc->search));

        target_search = g729_gain_quant_search_conjugate_target_bits(
            tc->x, tc->y, tc->z, tc->gpc_pred_q12, tc->target_bits);
        CHECK(search_equal(target_search, tc->target_bits_search));

        float_search = g729_gain_quant_search_conjugate_float_center(
            tc->x, tc->y, tc->z, tc->gpc_pred_q12);
        CHECK(search_equal(float_search, tc->float_search));

        g729_gain_quant_pack_gains(search.ga, search.gb, &ga_bits, &gb_bits);
        CHECK(ga_bits == tc->search.ga_bits);
        CHECK(gb_bits == tc->search.gb_bits);

        recon = g729_gain_quant_reconstruct(tc->past, tc->code, search.ga,
                                            search.gb);
        CHECK(recon_equal(recon, tc->recon));

        recon_wide = g729_gain_quant_reconstruct_wide(tc->past, tc->code,
                                                      search.ga, search.gb);
        CHECK(recon_equal(recon_wide, tc->recon_wide));

        memcpy(past, tc->past, sizeof(past));
        g729_gain_quant_update_past_qua_en(past, search.gamma_c_q13);
        CHECK(vec16_equal(past, tc->updated_past, 4));

        CHECK(g729_gain_quant_tame(tc->tame_input_q14, tc->old_exc) ==
              tc->tame_q14);
    }

    return 0;
}
