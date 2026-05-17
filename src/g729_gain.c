#include "g729_gain.h"

#include <stddef.h>

#include "g729_fixed.h"
#include "g729_gain_tables.h"

enum {
    GAIN_MEAN_ENERGY_Q10 = 30720,
    DB_PER_LOG2_Q13 = 24660,
    INV_DB_SCALE_Q15 = 5439,
    GAIN_LOG_EC_BAR_BIAS_Q14 = 2085632
};

static int64_t shr64_floor(int64_t x, unsigned n) {
    int64_t denom;
    int64_t mag;
    if (n == 0u) {
        return x;
    }
    denom = (int64_t)1 << n;
    if (x >= 0) {
        return x / denom;
    }
    mag = -x;
    return -((mag + denom - 1) / denom);
}

static void gain_init(g729_gain_decoder *dec) {
    int i;
    if (dec == NULL || dec->initialized) {
        return;
    }
    for (i = 0; i < 4; ++i) {
        dec->past_errors[i] = G729_GAIN_PAST_ERROR_DEFAULT;
    }
    dec->initialized = 1;
}

void g729_gain_decoder_reset(g729_gain_decoder *dec) {
    int i;
    if (dec == NULL) {
        return;
    }
    for (i = 0; i < 4; ++i) {
        dec->past_errors[i] = 0;
    }
    dec->initialized = 0;
}

static int64_t predicted_energy_q24(const int16_t past_errors[4]) {
    int i;
    int64_t acc = 0;
    for (i = 0; i < 4; ++i) {
        acc += (int64_t)2 * (int64_t)g729_gain_ma_predictor[i] *
               (int64_t)past_errors[i];
    }
    return acc;
}

static int32_t fixed_codebook_energy(const int16_t code[G729_SUBFRAME_SAMPLES]) {
    int n;
    int32_t acc = 0;
    if (code == NULL) {
        return 0;
    }
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int32_t sq = g729_l_shr(g729_l_mult(code[n], code[n]), 1);
        acc = g729_l_add(acc, sq);
    }
    return acc;
}

static int32_t log2_fixed_q15(int32_t x) {
    int16_t s;
    int32_t norm_x;
    int32_t int_part;
    int64_t frac30;
    int32_t idx;
    int32_t a;
    int32_t t0;
    int32_t t1;
    int32_t frac_log2_q15;
    if (x <= 0) {
        return 0;
    }
    s = g729_norm_l(x);
    norm_x = g729_l_shl(x, s);
    int_part = 30 - (int32_t)s;
    frac30 = (int64_t)norm_x - ((int64_t)1 << 30);
    idx = (int32_t)(frac30 >> 25);
    a = (int32_t)((frac30 >> 10) & 0x7FFF);
    t0 = g729_gain_log2_table[idx];
    t1 = g729_gain_log2_table[idx + 1];
    frac_log2_q15 = t0 + (int32_t)(((int64_t)(t1 - t0) * a) >> 15);
    return (int_part << 15) + frac_log2_q15;
}

static int16_t pow2_frac_q14_from_q15(int32_t frac) {
    int32_t idx = frac >> 10;
    int32_t a = frac & 0x3FF;
    int32_t t0 = g729_gain_pow2_table[idx];
    int32_t t1 = g729_gain_pow2_table[idx + 1];
    return (int16_t)(t0 + (((t1 - t0) * a + (1 << 9)) >> 10));
}

static int64_t log2_db_product_q14(int32_t int_part,
                                   int32_t frac_q15,
                                   int32_t multiplier_q13) {
    int64_t int_term = (int64_t)int_part * (int64_t)multiplier_q13 * 2;
    int64_t frac_term =
        (int64_t)g729_mult((int16_t)frac_q15, (int16_t)multiplier_q13) * 2;
    return int_term + frac_term;
}

static int32_t log_gain_db_q10_from_energy_q24(const int16_t past_errors[4],
                                               int32_t ec_energy) {
    int32_t raw_log2_q15 = log2_fixed_q15(g729_l_shl(ec_energy, 1));
    int32_t int_part = g729_l_shr(raw_log2_q15, 15);
    int32_t frac_q15 = raw_log2_q15 - (int_part << 15);
    int64_t ec_bar_q14 =
        (int64_t)GAIN_LOG_EC_BAR_BIAS_Q14 +
        log2_db_product_q14(int_part, frac_q15, -DB_PER_LOG2_Q13);
    int64_t log_gain_q24 =
        (ec_bar_q14 << 10) + predicted_energy_q24(past_errors);
    return (int32_t)shr64_floor(log_gain_q24, 14);
}

static int32_t log_gain_to_log2_q15(int32_t log_gain_db_q10) {
    int64_t log_gain_q8 = g729_l_shr(log_gain_db_q10, 2);
    return (int32_t)shr64_floor(log_gain_q8 * (int64_t)INV_DB_SCALE_Q15, 8);
}

static int64_t fixed_gain_q14_from_log2_gamma(int32_t log2_gc_q15,
                                              int32_t gamma_c_q13) {
    int32_t int_part = g729_l_shr(log2_gc_q15, 15);
    int32_t frac_q15 = log2_gc_q15 - (int_part << 15);
    int64_t gc0_q14 = pow2_frac_q14_from_q15(frac_q15);
    int64_t gamma_c_q12 = (int64_t)(gamma_c_q13 >> 1);
    int64_t base_q14 = (gamma_c_q12 * gc0_q14) >> 12;
    if (int_part >= 0) {
        if (int_part >= 62) {
            return INT64_MAX;
        }
        return base_q14 << (unsigned)int_part;
    }
    {
        int32_t shift = -int_part;
        if (shift >= 63) {
            return 0;
        }
        return base_q14 >> (unsigned)shift;
    }
}

static int64_t quantize_fixed_gain_q1(int64_t gain_q14) {
    int64_t gain_q1;
    if (gain_q14 <= 0) {
        return 0;
    }
    gain_q1 = gain_q14 >> 13;
    if (gain_q1 > 32767) {
        gain_q1 = 32767;
    }
    return gain_q1 << 13;
}

static void split_gain_q14(int64_t gain_q14, int16_t *mant, int8_t *exp) {
    int e = 0;
    if (gain_q14 <= 0) {
        *mant = 0;
        *exp = 0;
        return;
    }
    while (gain_q14 > 32767 && e < 127) {
        gain_q14 >>= 1;
        ++e;
    }
    while (gain_q14 < 16384 && e > -128) {
        gain_q14 <<= 1;
        --e;
    }
    *mant = (int16_t)gain_q14;
    *exp = (int8_t)e;
}

static void decode_vq(g729_gain_indices idx, int16_t *gp_q14, int32_t *gamma_c_q13) {
    uint8_t ga = g729_gain_imap1[idx.ga & 0x07u];
    uint8_t gb = g729_gain_imap2[idx.gb & 0x0Fu];
    *gp_q14 = g729_add(g729_gain_gbk1[ga][0], g729_gain_gbk2[gb][0]);
    *gamma_c_q13 = (int32_t)g729_gain_gbk1[ga][1] +
                   (int32_t)g729_gain_gbk2[gb][1];
}

int16_t g729_gain_quantized_prediction_error_q10(int32_t gamma_c_q13) {
    int32_t gamma_log2_q15 = log2_fixed_q15(gamma_c_q13) - 13 * (1 << 15);
    int32_t log2_q13;
    int64_t val;
    if (gamma_log2_q15 == 0) {
        return 0;
    }
    log2_q13 = g729_l_shr(gamma_log2_q15, 2);
    val = shr64_floor((int64_t)log2_q13 * (int64_t)DB_PER_LOG2_Q13, 15);
    if (val > 32767) {
        return 32767;
    }
    if (val < -32768) {
        return -32768;
    }
    return (int16_t)val;
}

void g729_gain_mark_erasure(g729_gain_decoder *dec) {
    int32_t avg;
    int32_t u_current;
    if (dec == NULL) {
        return;
    }
    gain_init(dec);
    avg = (int32_t)shr64_floor((int64_t)dec->past_errors[0] +
                                   (int64_t)dec->past_errors[1] +
                                   (int64_t)dec->past_errors[2] +
                                   (int64_t)dec->past_errors[3],
                               2);
    u_current = avg - 4096;
    if (u_current < G729_GAIN_PAST_ERROR_DEFAULT) {
        u_current = G729_GAIN_PAST_ERROR_DEFAULT;
    }
    dec->past_errors[3] = dec->past_errors[2];
    dec->past_errors[2] = dec->past_errors[1];
    dec->past_errors[1] = dec->past_errors[0];
    dec->past_errors[0] = (int16_t)u_current;
}

g729_gain_result g729_gain_decode(g729_gain_decoder *dec,
                                  g729_gain_indices idx,
                                  const int16_t code[G729_SUBFRAME_SAMPLES]) {
    g729_gain_result out = {0, 0, 0};
    int32_t ec_energy;
    int16_t gp;
    int32_t gamma_c;
    if (dec == NULL) {
        return out;
    }
    gain_init(dec);
    ec_energy = fixed_codebook_energy(code);
    decode_vq(idx, &gp, &gamma_c);
    out.gp_q14 = gp;

    if (ec_energy <= 0) {
        dec->past_errors[3] = dec->past_errors[2];
        dec->past_errors[2] = dec->past_errors[1];
        dec->past_errors[1] = dec->past_errors[0];
        dec->past_errors[0] = G729_GAIN_PAST_ERROR_DEFAULT;
        return out;
    }

    if (gamma_c > 0) {
        int32_t log_gain_db_q10 =
            log_gain_db_q10_from_energy_q24(dec->past_errors, ec_energy);
        int32_t log2_gc_q15 = log_gain_to_log2_q15(log_gain_db_q10);
        int64_t gain_q14 =
            quantize_fixed_gain_q1(fixed_gain_q14_from_log2_gamma(log2_gc_q15,
                                                                  gamma_c));
        split_gain_q14(gain_q14, &out.gc_mant_q14, &out.gc_exp);
    }

    dec->past_errors[3] = dec->past_errors[2];
    dec->past_errors[2] = dec->past_errors[1];
    dec->past_errors[1] = dec->past_errors[0];
    dec->past_errors[0] =
        gamma_c > 0 ? g729_gain_quantized_prediction_error_q10(gamma_c)
                    : G729_GAIN_PAST_ERROR_DEFAULT;
    return out;
}
