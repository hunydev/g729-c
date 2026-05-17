#include "g729_gain_quant.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "g729_fixed.h"
#include "g729_gain.h"
#include "g729_gain_tables.h"

enum {
    GAIN_MEAN_ENERGY_Q10 = 30720,
    DB_PER_LOG2_Q13 = 24660,
    TEN_LOG10_40_REFERENCE_Q10 = 16404,
    INV_DB_SCALE_Q15 = 5439,
    GAIN_LOG_EC_BAR_BIAS_Q14 = 2085632,
    GAIN_PRESELECT_DEFAULT_TARGET_BITS = 14,
    GAIN_PRESELECT_MAX_TARGET_BITS = 24
};

static int64_t shr64_floor(int64_t x, unsigned n) {
    int64_t denom;
    int64_t mag;
    if (n == 0u) {
        return x;
    }
    if (n >= 63u) {
        return x < 0 ? -1 : 0;
    }
    denom = (int64_t)1 << n;
    if (x >= 0) {
        return x / denom;
    }
    mag = -x;
    return -((mag + denom - 1) / denom);
}

static int64_t mul_pow2_i64(int64_t x, unsigned n) {
    return x * ((int64_t)1 << n);
}

static int64_t abs_i64(int64_t x) {
    if (x == INT64_MIN) {
        return INT64_MAX;
    }
    return x < 0 ? -x : x;
}

static unsigned bit_len_u64(uint64_t x) {
    unsigned n = 0;
    while (x != 0u) {
        ++n;
        x >>= 1u;
    }
    return n;
}

static unsigned bit_len_abs_i64(int64_t x) {
    return bit_len_u64((uint64_t)abs_i64(x));
}

static unsigned max_u(unsigned a, unsigned b) {
    return b > a ? b : a;
}

static int16_t predicted_log_gain_sat16(const int16_t past_errors[4]) {
    int i;
    int32_t acc = 0;
    int16_t predicted;
    for (i = 0; i < 4; ++i) {
        acc = g729_l_mac(acc, g729_gain_ma_predictor[i], past_errors[i]);
    }
    predicted = g729_extract_h(g729_l_shl(acc, 2));
    return g729_add((int16_t)GAIN_MEAN_ENERGY_Q10, predicted);
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

static int32_t fixed_codebook_energy(
    const int16_t code[G729_SUBFRAME_SAMPLES]) {
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

static int32_t log_gain_db_q10_from_energy_q24(
    const int16_t past_errors[4],
    int32_t ec_energy) {
    int32_t raw_log2_q15 = log2_fixed_q15(g729_l_shl(ec_energy, 1));
    int32_t int_part = g729_l_shr(raw_log2_q15, 15);
    int32_t frac_q15 = raw_log2_q15 - (int_part << 15);
    int64_t ec_bar_q14 =
        (int64_t)GAIN_LOG_EC_BAR_BIAS_Q14 +
        log2_db_product_q14(int_part, frac_q15, -DB_PER_LOG2_Q13);
    int64_t log_gain_q24 =
        mul_pow2_i64(ec_bar_q14, 10) + predicted_energy_q24(past_errors);
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
        return mul_pow2_i64(base_q14, (unsigned)int_part);
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

static int predicted_log2_gc_q15_search(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES],
    int32_t *log2_gc_q15) {
    int32_t ec_energy;
    int32_t log_input;
    int32_t log2_q15;
    int32_t ec_db_q10;
    int32_t log_gain_db_q10;
    if (past_qua_en == NULL || code == NULL || log2_gc_q15 == NULL) {
        return 0;
    }
    ec_energy = fixed_codebook_energy(code);
    if (ec_energy <= 0) {
        return 0;
    }
    log_input = g729_l_shl(ec_energy, 1);
    log2_q15 = log2_fixed_q15(log_input) - 27 * (1 << 15);
    ec_db_q10 =
        (int32_t)shr64_floor((int64_t)log2_q15 * DB_PER_LOG2_Q13, 18);
    log_gain_db_q10 = (int32_t)predicted_log_gain_sat16(past_qua_en) +
                      TEN_LOG10_40_REFERENCE_Q10 - ec_db_q10;
    *log2_gc_q15 = log_gain_to_log2_q15(log_gain_db_q10);
    return 1;
}

static int predicted_log2_gc_q15_wide(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES],
    int32_t *log2_gc_q15) {
    int32_t ec_energy;
    int32_t log_gain_db_q10;
    if (past_qua_en == NULL || code == NULL || log2_gc_q15 == NULL) {
        return 0;
    }
    ec_energy = fixed_codebook_energy(code);
    if (ec_energy <= 0) {
        return 0;
    }
    log_gain_db_q10 =
        log_gain_db_q10_from_energy_q24(past_qua_en, ec_energy);
    *log2_gc_q15 = log_gain_to_log2_q15(log_gain_db_q10);
    return 1;
}

static int32_t predicted_gc_q12_from_log2_q15(int32_t log2_gc_q15) {
    return (int32_t)(fixed_gain_q14_from_log2_gamma(log2_gc_q15, 8192) >> 2);
}

int32_t g729_gain_quant_predicted_gc_q12(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES]) {
    int32_t log2_gc_q15 = 0;
    if (!predicted_log2_gc_q15_search(past_qua_en, code, &log2_gc_q15)) {
        return 0;
    }
    return predicted_gc_q12_from_log2_q15(log2_gc_q15);
}

int32_t g729_gain_quant_predicted_gc_q12_wide(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES]) {
    int32_t log2_gc_q15 = 0;
    if (!predicted_log2_gc_q15_wide(past_qua_en, code, &log2_gc_q15)) {
        return 0;
    }
    return predicted_gc_q12_from_log2_q15(log2_gc_q15);
}

static void dequant_gc(int32_t log2_gc_pred_q15,
                       int ok,
                       int32_t gamma_c_q13,
                       int16_t *gc_mant_q14,
                       int8_t *gc_exp) {
    int64_t gain_q14;
    if (!ok || gamma_c_q13 <= 0) {
        *gc_mant_q14 = 0;
        *gc_exp = 0;
        return;
    }
    gain_q14 = quantize_fixed_gain_q1(
        fixed_gain_q14_from_log2_gamma(log2_gc_pred_q15, gamma_c_q13));
    split_gain_q14(gain_q14, gc_mant_q14, gc_exp);
}

static void gain_search_correlations(
    const int16_t x[G729_SUBFRAME_SAMPLES],
    const int16_t y[G729_SUBFRAME_SAMPLES],
    const int16_t z[G729_SUBFRAME_SAMPLES],
    int64_t *a,
    int64_t *b,
    int64_t *c,
    int64_t *d,
    int64_t *f) {
    int i;
    *a = 0;
    *b = 0;
    *c = 0;
    *d = 0;
    *f = 0;
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        int64_t xi = x[i];
        int64_t yi = y[i];
        int64_t zi = z[i];
        *a += mul_pow2_i64(yi * yi, 24);
        *b += zi * zi;
        *c += mul_pow2_i64(yi * zi, 12);
        *d += mul_pow2_i64(xi * yi, 24);
        *f += mul_pow2_i64(xi * zi, 12);
    }
}

static void sort_by_dist8(uint8_t idx[8], int64_t dist[8]) {
    int i;
    for (i = 1; i < 8; ++i) {
        int64_t di = dist[i];
        uint8_t ii = idx[i];
        int j = i - 1;
        while (j >= 0 && dist[j] > di) {
            dist[j + 1] = dist[j];
            idx[j + 1] = idx[j];
            --j;
        }
        dist[j + 1] = di;
        idx[j + 1] = ii;
    }
}

static void sort_by_dist16(uint8_t idx[16], int64_t dist[16]) {
    int i;
    for (i = 1; i < 16; ++i) {
        int64_t di = dist[i];
        uint8_t ii = idx[i];
        int j = i - 1;
        while (j >= 0 && dist[j] > di) {
            dist[j + 1] = dist[j];
            idx[j + 1] = idx[j];
            --j;
        }
        dist[j + 1] = di;
        idx[j + 1] = ii;
    }
}

static unsigned gain_search_term_shift(int64_t corr,
                                       unsigned factor_bits,
                                       unsigned extra_shift,
                                       unsigned target_bits) {
    unsigned corr_bits = bit_len_abs_i64(corr);
    unsigned total_bits;
    if (corr_bits == 0u) {
        return 0;
    }
    total_bits = corr_bits + factor_bits + extra_shift;
    if (total_bits <= target_bits) {
        return 0;
    }
    return total_bits - target_bits;
}

static unsigned gain_search_cost_shift(int64_t a,
                                       int64_t b,
                                       int64_t c,
                                       int64_t d,
                                       int64_t f,
                                       const uint8_t ga_cands[4],
                                       const uint8_t gb_cands[8],
                                       int32_t gpc_pred_q12) {
    const unsigned target_bits = 58;
    int i;
    int j;
    int64_t max_gp = 0;
    int64_t max_gc = 0;
    unsigned gp_bits;
    unsigned gc_bits;
    unsigned shift = 0;
    for (i = 0; i < 4; ++i) {
        uint8_t gai = ga_cands[i];
        int64_t gp1 = g729_gain_gbk1[gai][0];
        int32_t gam1 = g729_gain_gbk1[gai][1];
        for (j = 0; j < 8; ++j) {
            uint8_t gbi = gb_cands[j];
            int64_t gp = gp1 + (int64_t)g729_gain_gbk2[gbi][0];
            int64_t gam = (int64_t)gam1 + (int64_t)g729_gain_gbk2[gbi][1];
            int64_t gc = shr64_floor(gam * (int64_t)gpc_pred_q12, 13);
            gp = abs_i64(gp);
            gc = abs_i64(gc);
            if (gp > max_gp) {
                max_gp = gp;
            }
            if (gc > max_gc) {
                max_gc = gc;
            }
        }
    }
    if (max_gp == 0) {
        max_gp = 1;
    }
    if (max_gc == 0) {
        max_gc = 1;
    }
    gp_bits = bit_len_abs_i64(max_gp);
    gc_bits = bit_len_abs_i64(max_gc);
    shift = max_u(shift,
                  gain_search_term_shift(a, gp_bits + gp_bits, 0,
                                         target_bits));
    shift = max_u(shift,
                  gain_search_term_shift(b, gc_bits + gc_bits, 4,
                                         target_bits));
    shift = max_u(shift,
                  gain_search_term_shift(c, gp_bits + gc_bits, 3,
                                         target_bits));
    shift = max_u(shift,
                  gain_search_term_shift(d, gp_bits, 15, target_bits));
    shift = max_u(shift,
                  gain_search_term_shift(f, gc_bits, 17, target_bits));
    return shift;
}

void g729_gain_quant_pack_gains(uint8_t ga,
                                uint8_t gb,
                                uint8_t *ga_bits,
                                uint8_t *gb_bits) {
    if (ga_bits != NULL) {
        *ga_bits = g729_gain_map1[ga & 0x07u];
    }
    if (gb_bits != NULL) {
        *gb_bits = g729_gain_map2[gb & 0x0Fu];
    }
}

static g729_gain_quant_search_result search_conjugate_select_from_center(
    int64_t a,
    int64_t b,
    int64_t c,
    int64_t d,
    int64_t f,
    int32_t gpc_pred_q12,
    int64_t gp_opt_q14,
    int64_t gc_opt_q12) {
    uint8_t ga_idx[8];
    int64_t ga_dist[8];
    uint8_t gb_idx[16];
    int64_t gb_dist[16];
    uint8_t ga_cands[4];
    uint8_t gb_cands[8];
    unsigned cost_shift;
    int64_t cost_a;
    int64_t cost_b;
    int64_t cost_c;
    int64_t cost_d;
    int64_t cost_f;
    int i;
    int j;
    int64_t best_cost = (int64_t)1 << 62;
    int32_t best_gp = 0;
    int32_t best_gam = 0;
    g729_gain_quant_search_result out;

    for (i = 0; i < 8; ++i) {
        int64_t cand;
        int64_t dist;
        ga_idx[i] = (uint8_t)i;
        cand = shr64_floor((int64_t)g729_gain_gbk1[i][1] *
                               (int64_t)gpc_pred_q12,
                           13);
        dist = cand - gc_opt_q12;
        ga_dist[i] = abs_i64(dist);
    }
    sort_by_dist8(ga_idx, ga_dist);
    for (i = 0; i < 4; ++i) {
        ga_cands[i] = ga_idx[i];
    }

    for (j = 0; j < 16; ++j) {
        int64_t dist;
        gb_idx[j] = (uint8_t)j;
        dist = (int64_t)g729_gain_gbk2[j][0] - gp_opt_q14;
        gb_dist[j] = abs_i64(dist);
    }
    sort_by_dist16(gb_idx, gb_dist);
    for (j = 0; j < 8; ++j) {
        gb_cands[j] = gb_idx[j];
    }

    cost_shift = gain_search_cost_shift(a, b, c, d, f, ga_cands, gb_cands,
                                        gpc_pred_q12);
    cost_a = shr64_floor(a, cost_shift);
    cost_b = shr64_floor(b, cost_shift);
    cost_c = shr64_floor(c, cost_shift);
    cost_d = shr64_floor(d, cost_shift);
    cost_f = shr64_floor(f, cost_shift);

    out.ga = 0;
    out.gb = 0;
    out.ga_bits = 0;
    out.gb_bits = 0;
    out.gp_q14 = 0;
    out.gamma_c_q13 = 0;

    for (i = 0; i < 4; ++i) {
        uint8_t gai = ga_cands[i];
        int64_t gp1 = g729_gain_gbk1[gai][0];
        int32_t gam1 = g729_gain_gbk1[gai][1];
        for (j = 0; j < 8; ++j) {
            uint8_t gbi = gb_cands[j];
            int64_t gp_q = gp1 + (int64_t)g729_gain_gbk2[gbi][0];
            int64_t gam = (int64_t)gam1 + (int64_t)g729_gain_gbk2[gbi][1];
            int64_t gc_q = shr64_floor(gam * (int64_t)gpc_pred_q12, 13);
            int64_t cost = gp_q * gp_q * cost_a;
            cost += mul_pow2_i64(gc_q * gc_q * cost_b, 4);
            cost += mul_pow2_i64(2 * gp_q * gc_q * cost_c, 2);
            cost -= mul_pow2_i64(2 * gp_q * cost_d, 14);
            cost -= mul_pow2_i64(2 * gc_q * cost_f, 16);

            if (cost < best_cost) {
                best_cost = cost;
                out.ga = gai;
                out.gb = gbi;
                best_gp = (int32_t)gp_q;
                best_gam = (int32_t)gam;
            }
        }
    }

    out.gp_q14 = (int16_t)best_gp;
    out.gamma_c_q13 = best_gam;
    g729_gain_quant_pack_gains(out.ga, out.gb, &out.ga_bits, &out.gb_bits);
    return out;
}

g729_gain_quant_search_result
g729_gain_quant_search_conjugate_target_bits(
    const int16_t x[G729_SUBFRAME_SAMPLES],
    const int16_t y[G729_SUBFRAME_SAMPLES],
    const int16_t z[G729_SUBFRAME_SAMPLES],
    int32_t gpc_pred_q12,
    unsigned target_bits) {
    int64_t a;
    int64_t b;
    int64_t c;
    int64_t d;
    int64_t f;
    int64_t raw_a;
    int64_t raw_b;
    int64_t raw_c;
    int64_t raw_d;
    int64_t raw_f;
    int64_t max_abs;
    unsigned nshift = 0;
    int64_t det;
    int64_t gp_opt_q14 = 0;
    int64_t gc_opt_q12 = 0;
    g729_gain_quant_search_result zero = {0, 0, 0, 0, 0, 0};

    if (x == NULL || y == NULL || z == NULL) {
        return zero;
    }
    if (target_bits == 0u) {
        target_bits = 1;
    }
    if (target_bits > GAIN_PRESELECT_MAX_TARGET_BITS) {
        target_bits = GAIN_PRESELECT_MAX_TARGET_BITS;
    }

    gain_search_correlations(x, y, z, &a, &b, &c, &d, &f);
    raw_a = a;
    raw_b = b;
    raw_c = c;
    raw_d = d;
    raw_f = f;

    max_abs = abs_i64(a);
    if (abs_i64(b) > max_abs) {
        max_abs = abs_i64(b);
    }
    if (abs_i64(c) > max_abs) {
        max_abs = abs_i64(c);
    }
    if (abs_i64(d) > max_abs) {
        max_abs = abs_i64(d);
    }
    if (abs_i64(f) > max_abs) {
        max_abs = abs_i64(f);
    }
    if (max_abs > 0) {
        unsigned blen = bit_len_u64((uint64_t)max_abs);
        if (blen > target_bits) {
            nshift = blen - target_bits;
        }
    }
    if (nshift > 0u) {
        a = shr64_floor(a, nshift);
        b = shr64_floor(b, nshift);
        c = shr64_floor(c, nshift);
        d = shr64_floor(d, nshift);
        f = shr64_floor(f, nshift);
    }

    det = a * b - c * c;
    if (det > 0) {
        int64_t num_gp = d * b - f * c;
        int64_t num_gc = f * a - d * c;
        gp_opt_q14 = mul_pow2_i64(num_gp, 14) / det;
        gc_opt_q12 = mul_pow2_i64(num_gc, 12) / det;
    } else if (a > 0) {
        gp_opt_q14 = mul_pow2_i64(d, 14) / a;
        gc_opt_q12 = 0;
    } else if (b > 0) {
        gp_opt_q14 = 0;
        gc_opt_q12 = mul_pow2_i64(f, 12) / b;
    }
    if (gp_opt_q14 < 0) {
        gp_opt_q14 = 0;
    }
    if (gc_opt_q12 < 0) {
        gc_opt_q12 = 0;
    }

    return search_conjugate_select_from_center(raw_a, raw_b, raw_c, raw_d,
                                               raw_f, gpc_pred_q12,
                                               gp_opt_q14, gc_opt_q12);
}

g729_gain_quant_search_result g729_gain_quant_search_conjugate(
    const int16_t x[G729_SUBFRAME_SAMPLES],
    const int16_t y[G729_SUBFRAME_SAMPLES],
    const int16_t z[G729_SUBFRAME_SAMPLES],
    int32_t gpc_pred_q12) {
    return g729_gain_quant_search_conjugate_target_bits(
        x, y, z, gpc_pred_q12, GAIN_PRESELECT_DEFAULT_TARGET_BITS);
}

g729_gain_quant_search_result g729_gain_quant_search_conjugate_float_center(
    const int16_t x[G729_SUBFRAME_SAMPLES],
    const int16_t y[G729_SUBFRAME_SAMPLES],
    const int16_t z[G729_SUBFRAME_SAMPLES],
    int32_t gpc_pred_q12) {
    int64_t a;
    int64_t b;
    int64_t c;
    int64_t d;
    int64_t f;
    double af;
    double bf;
    double cf;
    double df;
    double ff;
    double det;
    int64_t gp_opt_q14 = 0;
    int64_t gc_opt_q12 = 0;
    g729_gain_quant_search_result zero = {0, 0, 0, 0, 0, 0};

    if (x == NULL || y == NULL || z == NULL) {
        return zero;
    }

    gain_search_correlations(x, y, z, &a, &b, &c, &d, &f);
    af = (double)a;
    bf = (double)b;
    cf = (double)c;
    df = (double)d;
    ff = (double)f;
    det = af * bf - cf * cf;
    if (det > 0.0) {
        gp_opt_q14 = (int64_t)(((df * bf - ff * cf) * 16384.0) / det);
        gc_opt_q12 = (int64_t)(((ff * af - df * cf) * 4096.0) / det);
    } else if (a > 0) {
        gp_opt_q14 = (int64_t)(df * 16384.0 / af);
    } else if (b > 0) {
        gc_opt_q12 = (int64_t)(ff * 4096.0 / bf);
    }
    if (gp_opt_q14 < 0) {
        gp_opt_q14 = 0;
    }
    if (gc_opt_q12 < 0) {
        gc_opt_q12 = 0;
    }
    return search_conjugate_select_from_center(a, b, c, d, f, gpc_pred_q12,
                                               gp_opt_q14, gc_opt_q12);
}

g729_gain_quant_reconstruct_result g729_gain_quant_reconstruct(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES],
    uint8_t ga,
    uint8_t gb) {
    g729_gain_quant_reconstruct_result out = {0, 0, 0};
    int32_t gamma_c_q13;
    int32_t log2_gc_pred_q15 = 0;
    int ok;
    ga &= 0x07u;
    gb &= 0x0Fu;
    out.gp_q14 = g729_saturate((int32_t)g729_gain_gbk1[ga][0] +
                               (int32_t)g729_gain_gbk2[gb][0]);
    gamma_c_q13 = (int32_t)g729_gain_gbk1[ga][1] +
                  (int32_t)g729_gain_gbk2[gb][1];
    ok = predicted_log2_gc_q15_search(past_qua_en, code, &log2_gc_pred_q15);
    dequant_gc(log2_gc_pred_q15, ok, gamma_c_q13, &out.gc_mant_q14,
               &out.gc_exp);
    return out;
}

g729_gain_quant_reconstruct_result g729_gain_quant_reconstruct_wide(
    const int16_t past_qua_en[4],
    const int16_t code[G729_SUBFRAME_SAMPLES],
    uint8_t ga,
    uint8_t gb) {
    g729_gain_quant_reconstruct_result out = {0, 0, 0};
    int32_t gamma_c_q13;
    int32_t log2_gc_pred_q15 = 0;
    int ok;
    ga &= 0x07u;
    gb &= 0x0Fu;
    out.gp_q14 = g729_saturate((int32_t)g729_gain_gbk1[ga][0] +
                               (int32_t)g729_gain_gbk2[gb][0]);
    gamma_c_q13 = (int32_t)g729_gain_gbk1[ga][1] +
                  (int32_t)g729_gain_gbk2[gb][1];
    ok = predicted_log2_gc_q15_wide(past_qua_en, code, &log2_gc_pred_q15);
    dequant_gc(log2_gc_pred_q15, ok, gamma_c_q13, &out.gc_mant_q14,
               &out.gc_exp);
    return out;
}

void g729_gain_quant_update_past_qua_en(int16_t past_qua_en[4],
                                        int32_t gamma_c_q13) {
    int16_t u_current;
    if (past_qua_en == NULL) {
        return;
    }
    if (gamma_c_q13 > 0) {
        u_current = g729_gain_quantized_prediction_error_q10(gamma_c_q13);
    } else {
        u_current = G729_GAIN_PAST_ERROR_DEFAULT;
    }
    past_qua_en[3] = past_qua_en[2];
    past_qua_en[2] = past_qua_en[1];
    past_qua_en[1] = past_qua_en[0];
    past_qua_en[0] = u_current;
}

int16_t g729_gain_quant_tame(
    int16_t gp_q14,
    const int16_t old_exc[G729_GAIN_QUANT_EXC_MEM_SAMPLES]) {
    int i;
    int64_t energy = 0;
    if (old_exc == NULL) {
        return gp_q14;
    }
    for (i = 0; i < G729_GAIN_QUANT_EXC_MEM_SAMPLES; ++i) {
        int64_t s = old_exc[i];
        energy += s * s;
    }
    if (energy > G729_GAIN_QUANT_TAME_ENERGY_THRESHOLD_Q0 &&
        gp_q14 > G729_GAIN_QUANT_GP_CLIP_Q14) {
        return G729_GAIN_QUANT_GP_CLIP_Q14;
    }
    return gp_q14;
}
