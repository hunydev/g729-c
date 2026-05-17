#include "g729_postfilter.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/postfilter_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int vec_equal16(const int16_t *a, const int16_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    g729_postfilter pf;
    int i;
    memset(&pf, 0, sizeof(pf));

    for (i = 0; i < POSTFILTER_ORACLE_VECTOR_COUNT; ++i) {
        int16_t out[G729_SUBFRAME_SAMPLES];
        g729_postfilter_filter(&pf,
                               POSTFILTER_ORACLE_VECTORS[i].a,
                               POSTFILTER_ORACLE_VECTORS[i].t_int,
                               POSTFILTER_ORACLE_VECTORS[i].s,
                               out);
        CHECK(vec_equal16(out, POSTFILTER_ORACLE_VECTORS[i].out,
                          G729_SUBFRAME_SAMPLES));
        CHECK(vec_equal16(pf.past_s, POSTFILTER_ORACLE_VECTORS[i].past_s,
                          G729_POSTFILTER_LPC_ORDER));
        CHECK(vec_equal16(pf.past_residual,
                          POSTFILTER_ORACLE_VECTORS[i].past_residual,
                          G729_POSTFILTER_RESIDUAL_LEN));
        CHECK(vec_equal16(pf.past_synth_post,
                          POSTFILTER_ORACLE_VECTORS[i].past_synth_post,
                          G729_POSTFILTER_LPC_ORDER));
        CHECK(pf.past_tilt_input ==
              POSTFILTER_ORACLE_VECTORS[i].past_tilt_input);
        CHECK(pf.agc_gain_prev == POSTFILTER_ORACLE_VECTORS[i].agc_gain_prev);
        CHECK(pf.initialized == POSTFILTER_ORACLE_VECTORS[i].initialized);
    }

    g729_postfilter_reset(&pf);
    for (i = 0; i < G729_POSTFILTER_LPC_ORDER; ++i) {
        CHECK(pf.past_s[i] == 0);
        CHECK(pf.past_synth_post[i] == 0);
    }
    for (i = 0; i < G729_POSTFILTER_RESIDUAL_LEN; ++i) {
        CHECK(pf.past_residual[i] == 0);
    }
    CHECK(pf.past_tilt_input == 0);
    CHECK(pf.agc_gain_prev == 0);
    CHECK(pf.initialized == 0);

    return 0;
}
