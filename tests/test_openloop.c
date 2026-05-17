#include "g729_openloop.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "g729_lpc.h"
#include "g729_lsp.h"
#include "g729_pcm.h"
#include "fixtures/openloop_oracle_vectors.h"

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

static int range_equal(g729_openloop_range_score got,
                       openloop_oracle_range_score want) {
    return got.lag == want.lag && got.r == want.r && got.e == want.e;
}

int main(void) {
    {
        int16_t wsp[G729_OPENLOOP_WSPEECH_LEN] = {0};
        g729_openloop_search_result got =
            g729_openloop_search_with_ranges(wsp);
        CHECK(got.range1.lag == 20);
        CHECK(got.range2.lag == 40);
        CHECK(got.range3.lag == 80);
        CHECK(got.top == 20);
    }

    {
        int i;
        int16_t wsp[G729_OPENLOOP_WSPEECH_LEN];
        g729_openloop_search_result got;
        for (i = 0; i < G729_OPENLOOP_WSPEECH_LEN; ++i) {
            wsp[i] = 1024;
        }
        got = g729_openloop_search_with_ranges(wsp);
        CHECK(got.range1.lag == 20);
        CHECK(got.range2.lag == 40);
        CHECK(got.range3.lag == 80);
        CHECK(got.range1.e == 41943040);
        CHECK(got.top == 20);
    }

    {
        g729_preprocessor pre;
        g729_lsp_decoder lsp_dec;
        int16_t old_speech[G729_LPC_WINDOW_SAMPLES] = {0};
        int16_t freq_prev[4][10];
        int16_t residual_mem[G729_LPC_ORDER] = {0};
        int16_t sw_mem[G729_LPC_ORDER] = {0};
        int16_t old_wspeech[G729_OPENLOOP_OLD_WSPEECH_LEN] = {0};
        int frame;

        memset(&pre, 0, sizeof(pre));
        memset(&lsp_dec, 0, sizeof(lsp_dec));
        g729_lsp_init_freq_prev(freq_prev);

        for (frame = 0; frame < OPENLOOP_ORACLE_FRAME_COUNT; ++frame) {
            int16_t processed[G729_FRAME_SAMPLES];
            int16_t a[G729_LPC_ORDER + 1];
            int16_t lsp_q15[10];
            int16_t lsf_q13[10];
            g729_lsp_indices idx;
            int16_t a_hat_sf1[G729_LPC_ORDER + 1];
            int16_t a_hat_sf2[G729_LPC_ORDER + 1];
            g729_openloop_search_result result;
            int rc;

            g729_preprocessor_process_frame(
                &pre, OPENLOOP_ORACLE_FRAMES[frame].pcm, processed);
            memmove(old_speech, &old_speech[G729_FRAME_SAMPLES],
                    (G729_LPC_WINDOW_SAMPLES - G729_FRAME_SAMPLES) *
                        sizeof(old_speech[0]));
            memcpy(&old_speech[G729_LPC_WINDOW_SAMPLES -
                               G729_FRAME_SAMPLES],
                   processed, sizeof(processed));

            g729_lpc_analyze(old_speech, a);
            rc = g729_lsp_lp_to_lsp(a, lsp_q15);
            CHECK(rc == 0);
            g729_lsp_lsp_vector_to_lsf(lsp_q15, lsf_q13);
            idx = g729_lsp_quantize(lsf_q13, freq_prev);
            g729_lsp_decode(&lsp_dec, idx, a_hat_sf1, a_hat_sf2);

            CHECK(vec16_equal(&old_speech[120],
                              OPENLOOP_ORACLE_FRAMES[frame].speech, 80));
            CHECK(vec16_equal(a_hat_sf1,
                              OPENLOOP_ORACLE_FRAMES[frame].a_hat_sf1, 11));
            CHECK(vec16_equal(a_hat_sf2,
                              OPENLOOP_ORACLE_FRAMES[frame].a_hat_sf2, 11));

            result = g729_openloop_step_split_search(
                a_hat_sf1, a_hat_sf2, &old_speech[120], residual_mem,
                sw_mem, old_wspeech);

            CHECK(range_equal(result.range1,
                              OPENLOOP_ORACLE_FRAMES[frame].range1));
            CHECK(range_equal(result.range2,
                              OPENLOOP_ORACLE_FRAMES[frame].range2));
            CHECK(range_equal(result.range3,
                              OPENLOOP_ORACLE_FRAMES[frame].range3));
            CHECK(result.top == OPENLOOP_ORACLE_FRAMES[frame].top);
            CHECK(vec16_equal(residual_mem,
                              OPENLOOP_ORACLE_FRAMES[frame].residual_mem_after,
                              G729_LPC_ORDER));
            CHECK(vec16_equal(sw_mem,
                              OPENLOOP_ORACLE_FRAMES[frame].sw_mem_after,
                              G729_LPC_ORDER));
            CHECK(vec16_equal(old_wspeech,
                              OPENLOOP_ORACLE_FRAMES[frame].old_wspeech_after,
                              G729_OPENLOOP_OLD_WSPEECH_LEN));
        }
    }

    return 0;
}
