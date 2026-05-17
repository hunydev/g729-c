#ifndef G729_GAIN_TABLES_H
#define G729_GAIN_TABLES_H

#include <stdint.h>

extern const int16_t g729_gain_gbk1[8][2];
extern const int16_t g729_gain_gbk2[16][2];
extern const uint8_t g729_gain_map1[8];
extern const uint8_t g729_gain_map2[16];
extern const uint8_t g729_gain_imap1[8];
extern const uint8_t g729_gain_imap2[16];
extern const int16_t g729_gain_ma_predictor[4];
extern const int16_t g729_gain_pow2_table[33];
extern const int16_t g729_gain_log2_table[33];

#endif
