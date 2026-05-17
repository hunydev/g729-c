#include "g729_lpc.h"
#include "g729_lsp.h"
#include "g729_pcm.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/lsp_encode_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int vec_equal(const int16_t *a, const int16_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int freq_equal(const int16_t *a, const int16_t *b) {
    int i;
    for (i = 0; i < 40; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    g729_preprocessor pre;
    int16_t old_speech[G729_LPC_WINDOW_SAMPLES] = {0};
    int16_t freq_prev[4][10];
    int frame;

    memset(&pre, 0, sizeof(pre));
    g729_lsp_init_freq_prev(freq_prev);

    for (frame = 0; frame < LSP_ENCODE_ORACLE_FRAME_COUNT; ++frame) {
        int16_t processed[G729_FRAME_SAMPLES];
        int16_t a[G729_LPC_ORDER + 1];
        int16_t lsp_q15[10];
        int16_t lsf_q13[10];
        g729_lsp_indices idx;
        int rc;

        g729_preprocessor_process_frame(&pre,
                                        LSP_ENCODE_ORACLE_FRAMES[frame].pcm,
                                        processed);
        memmove(old_speech, &old_speech[G729_FRAME_SAMPLES],
                (G729_LPC_WINDOW_SAMPLES - G729_FRAME_SAMPLES) *
                    sizeof(old_speech[0]));
        memcpy(&old_speech[G729_LPC_WINDOW_SAMPLES - G729_FRAME_SAMPLES],
               processed, sizeof(processed));

        g729_lpc_analyze(old_speech, a);
        CHECK(vec_equal(a, LSP_ENCODE_ORACLE_FRAMES[frame].a,
                        G729_LPC_ORDER + 1));

        rc = g729_lsp_lp_to_lsp(a, lsp_q15);
        CHECK(rc == 0);
        CHECK(vec_equal(lsp_q15, LSP_ENCODE_ORACLE_FRAMES[frame].lsp_q15, 10));

        g729_lsp_lsp_vector_to_lsf(lsp_q15, lsf_q13);
        CHECK(vec_equal(lsf_q13, LSP_ENCODE_ORACLE_FRAMES[frame].lsf_q13, 10));

        idx = g729_lsp_quantize(lsf_q13, freq_prev);
        CHECK(idx.l0 == LSP_ENCODE_ORACLE_FRAMES[frame].l0);
        CHECK(idx.l1 == LSP_ENCODE_ORACLE_FRAMES[frame].l1);
        CHECK(idx.l2 == LSP_ENCODE_ORACLE_FRAMES[frame].l2);
        CHECK(idx.l3 == LSP_ENCODE_ORACLE_FRAMES[frame].l3);
        CHECK(freq_equal(&freq_prev[0][0],
                         &LSP_ENCODE_ORACLE_FRAMES[frame].freq_prev_after[0][0]));
    }

    {
        int16_t freq[4][10];
        int i;
        g729_lsp_init_freq_prev(freq);
        for (i = 0; i < 4; ++i) {
            CHECK(freq[i][0] == 2339);
            CHECK(freq[i][9] == 23396);
        }
    }

    return 0;
}
