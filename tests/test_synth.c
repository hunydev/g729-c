#include "g729_synth.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/synth_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int vec40_equal(const int16_t a[G729_SUBFRAME_SAMPLES],
                       const int16_t b[G729_SUBFRAME_SAMPLES]) {
    int i;
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    g729_synthesizer synth;
    int16_t a[11] = {4096, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int16_t v[G729_SUBFRAME_SAMPLES] = {0};
    int16_t c[G729_SUBFRAME_SAMPLES] = {0};
    int16_t u[G729_SUBFRAME_SAMPLES];
    int16_t out[G729_SUBFRAME_SAMPLES];
    int i;

    memset(&synth, 0, sizeof(synth));
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        v[i] = (int16_t)(500 + i * 10);
    }
    g729_synth_synthesize(&synth, a, v, c, 16384, 0, 0, out);
    CHECK(vec40_equal(out, v));

    memset(u, 0, sizeof(u));
    g729_synth_build_excitation(8192, 8192, 0, v, c, u);
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        CHECK(u[i] >= v[i] / 2 - 1);
        CHECK(u[i] <= v[i] / 2 + 1);
    }

    memset(&synth, 0, sizeof(synth));
    for (i = 0; i < SYNTH_ORACLE_VECTOR_COUNT; ++i) {
        int j;
        for (j = 0; j < 10; ++j) {
            synth.past_synth[j] = SYNTH_ORACLE_VECTORS[i].past_in[j];
        }
        synth.last_excitation_scale_shift = 0;
        g729_synth_build_excitation(SYNTH_ORACLE_VECTORS[i].gp_q14,
                                    SYNTH_ORACLE_VECTORS[i].gc_mant_q14,
                                    SYNTH_ORACLE_VECTORS[i].gc_exp,
                                    SYNTH_ORACLE_VECTORS[i].v,
                                    SYNTH_ORACLE_VECTORS[i].c,
                                    u);
        CHECK(vec40_equal(u, SYNTH_ORACLE_VECTORS[i].u));
        g729_synth_filter(&synth,
                          SYNTH_ORACLE_VECTORS[i].a,
                          u,
                          out);
        CHECK(vec40_equal(out, SYNTH_ORACLE_VECTORS[i].out));
        for (j = 0; j < 10; ++j) {
            CHECK(synth.past_synth[j] == SYNTH_ORACLE_VECTORS[i].past_out[j]);
        }
        CHECK(synth.last_excitation_scale_shift ==
              SYNTH_ORACLE_VECTORS[i].scale_shift);
    }

    g729_synth_reset(&synth);
    for (i = 0; i < 10; ++i) {
        CHECK(synth.past_synth[i] == 0);
    }
    CHECK(synth.last_excitation_scale_shift == 0);

    return 0;
}
