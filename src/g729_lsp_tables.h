#ifndef G729_LSP_TABLES_H
#define G729_LSP_TABLES_H

#include <stdint.h>

extern const int16_t g729_lsp_codebook_l1[128][10];
extern const int16_t g729_lsp_codebook_l2[32][5];
extern const int16_t g729_lsp_codebook_l3[32][5];
extern const int16_t g729_lsp_ma_predictors[2][4][10];
extern const int16_t g729_lsp_ma_predictor_inv_sum[2][10];
extern const int16_t g729_lsp_cos[65];
extern const int16_t g729_lsp_cos_slope[64];

#endif
