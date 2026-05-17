#ifndef G729_LSP_H
#define G729_LSP_H

#include <stdint.h>

typedef struct g729_lsp_indices {
    uint8_t l0;
    uint8_t l1;
    uint8_t l2;
    uint8_t l3;
} g729_lsp_indices;

typedef struct g729_lsp_decoder {
    int16_t past_residuals[4][10];
    int16_t prev_lsp[10];
    int16_t prev_lsf[10];
    uint8_t last_selector;
    int initialized;
} g729_lsp_decoder;

void g729_lsp_decoder_reset(g729_lsp_decoder *dec);
void g729_lsp_decode(g729_lsp_decoder *dec,
                     g729_lsp_indices idx,
                     int16_t sf1[11],
                     int16_t sf2[11]);
void g729_lsp_init_freq_prev(int16_t freq_prev[4][10]);
int16_t g729_lsp_lsf_to_lsp(int16_t omega);
int16_t g729_lsp_lsp_to_lsf(int16_t q);
void g729_lsp_lsp_vector_to_lsf(const int16_t lsp[10], int16_t lsf[10]);
int g729_lsp_lp_to_lsp(const int16_t a[11], int16_t lsp[10]);
void g729_lsp_lsp_to_lp(const int16_t lsp[10], int16_t a[11]);
g729_lsp_indices g729_lsp_quantize(const int16_t omega[10],
                                   int16_t freq_prev[4][10]);

#endif
