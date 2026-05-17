#include "g729_postfilter.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "g729_fixed.h"

enum {
    GAMMA_NUM_Q15 = 18022,
    GAMMA_DEN_Q15 = 22938,
    TILT_LEN = 22,
    GAMMA_TILT_POSITIVE_K1_Q14 = 13107,
    LONG_TERM_IDENTITY_G0_Q14 = 16383,
    LONG_TERM_MAX_GAMMA_SCALED_GAIN_Q15 = 10923,
    LONG_TERM_GATE_THRESHOLD_Q15_SQ = 16384,
    AGC_ALPHA_Q15 = 29491,
    AGC_ALPHA_COMPLEMENT_Q15 = 3276
};

typedef struct long_term_gain_weights {
    int16_t g0_q14;
    int16_t g1_q14;
    int16_t gamma_scaled_gain_q15;
    int enabled;
} long_term_gain_weights;

static int64_t shr64_floor(int64_t x, unsigned n) {
    uint64_t mag;
    uint64_t denom;
    if (n == 0u) {
        return x;
    }
    if (x >= 0) {
        return (int64_t)((uint64_t)x >> n);
    }
    mag = (uint64_t)(-(x + 1)) + 1u;
    denom = (uint64_t)1 << n;
    return -(int64_t)((mag + denom - 1u) / denom);
}

static int16_t sat16_i64(int64_t x) {
    if (x > 32767) {
        return 32767;
    }
    if (x < -32768) {
        return -32768;
    }
    return (int16_t)x;
}

static int16_t wrap16_i64(int64_t x) {
    uint16_t u = (uint16_t)((uint64_t)x & 0xffffu);
    if (u <= 32767u) {
        return (int16_t)u;
    }
    return (int16_t)((int32_t)u - 65536);
}

static int32_t wrap32_i64(int64_t x) {
    uint32_t u = (uint32_t)((uint64_t)x & 0xffffffffu);
    if (u <= 2147483647u) {
        return (int32_t)u;
    }
    return (int32_t)((int64_t)u - 4294967296LL);
}

static void expand_bandwidth(const int16_t a[G729_POSTFILTER_LPC_ORDER + 1],
                             int16_t gamma_q15,
                             int16_t out[G729_POSTFILTER_LPC_ORDER + 1]) {
    int i;
    int16_t gamma_pow = gamma_q15;
    out[0] = a[0];
    for (i = 1; i <= G729_POSTFILTER_LPC_ORDER; ++i) {
        int64_t prod = (int64_t)a[i] * (int64_t)gamma_pow;
        int64_t gp;
        out[i] = wrap16_i64(shr64_floor(prod + (1LL << 14), 15));

        gp = (int64_t)gamma_pow * (int64_t)gamma_q15;
        gamma_pow = wrap16_i64(shr64_floor(gp + (1LL << 14), 15));
    }
}

static void compute_residual(g729_postfilter *pf,
                             const int16_t a_num[G729_POSTFILTER_LPC_ORDER + 1],
                             const int16_t s[G729_SUBFRAME_SAMPLES],
                             int16_t r[G729_SUBFRAME_SAMPLES]) {
    int16_t work[G729_POSTFILTER_LPC_ORDER + G729_SUBFRAME_SAMPLES];
    int n;
    memcpy(work, pf->past_s, sizeof(pf->past_s));
    memcpy(&work[G729_POSTFILTER_LPC_ORDER], s,
           G729_SUBFRAME_SAMPLES * sizeof(work[0]));

    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int i;
        int32_t l_temp = g729_l_mult(a_num[0],
                                     work[G729_POSTFILTER_LPC_ORDER + n]);
        for (i = 1; i <= G729_POSTFILTER_LPC_ORDER; ++i) {
            l_temp = g729_l_mac(l_temp, a_num[i],
                                work[G729_POSTFILTER_LPC_ORDER + n - i]);
        }
        l_temp = g729_l_shl(l_temp, 3);
        r[n] = g729_round(l_temp);
    }

    memcpy(pf->past_s, &work[G729_SUBFRAME_SAMPLES], sizeof(pf->past_s));
}

static int64_t long_term_scaled_product(int16_t a, int16_t b) {
    int64_t sa = g729_shr(a, 2);
    int64_t sb = g729_shr(b, 2);
    return 2 * sa * sb;
}

static int refine_pitch(const g729_postfilter *pf, int t_int) {
    const int min_t = 17;
    const int max_t = G729_POSTFILTER_PITCH_MAX;
    int center = t_int;
    int lo;
    int hi;
    int best_t;
    int t;
    int64_t best_corr = INT64_MIN;

    if (center > 140) {
        center = 140;
    }
    lo = center - 3;
    if (lo < min_t) {
        lo = min_t;
    }
    hi = center + 3;
    if (hi > max_t) {
        hi = max_t;
    }

    best_t = lo;
    for (t = lo; t <= hi; ++t) {
        int n;
        int64_t r_sum = 0;
        for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
            r_sum += long_term_scaled_product(
                pf->past_residual[G729_POSTFILTER_PITCH_MAX + n],
                pf->past_residual[G729_POSTFILTER_PITCH_MAX + n - t]);
        }
        if (r_sum > best_corr) {
            best_t = t;
            best_corr = r_sum;
        }
    }

    return best_t;
}

static int long_term_norm_shift(int64_t a, int64_t b, int64_t c) {
    int shift = 0;
    int64_t max = a;
    if (b > max) {
        max = b;
    }
    if (c > max) {
        max = c;
    }
    if (max <= 0) {
        return 0;
    }
    while (max < 0x40000000LL) {
        max <<= 1;
        ++shift;
    }
    return shift;
}

static int16_t long_term_rounded_norm_q15(int64_t v, int shift) {
    int64_t norm = shr64_floor((v << (unsigned)shift) + 0x8000, 16);
    if (norm > 32767) {
        return 32767;
    }
    return (int16_t)norm;
}

static long_term_gain_weights compute_long_term_gain_weights(
    const g729_postfilter *pf,
    const int16_t r[G729_SUBFRAME_SAMPLES],
    int t) {
    int n;
    int64_t r_corr = 0;
    int64_t delayed_e = 1;
    int64_t current_e = 1;
    long_term_gain_weights out;

    memset(&out, 0, sizeof(out));
    out.g0_q14 = LONG_TERM_IDENTITY_G0_Q14;

    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int16_t rn = pf->past_residual[G729_POSTFILTER_PITCH_MAX + n];
        int16_t rn_t = pf->past_residual[G729_POSTFILTER_PITCH_MAX + n - t];
        r_corr += long_term_scaled_product(rn, rn_t);
        delayed_e += long_term_scaled_product(rn_t, rn_t);
        current_e += long_term_scaled_product(r[n], r[n]);
    }

    if (r_corr < 0 || delayed_e == 0 || current_e == 0) {
        return out;
    }

    {
        int norm_shift = long_term_norm_shift(r_corr, delayed_e, current_e);
        int16_t r_norm_q15 = long_term_rounded_norm_q15(r_corr, norm_shift);
        int16_t delayed_e_norm_q15 =
            long_term_rounded_norm_q15(delayed_e, norm_shift);
        int16_t current_e_norm_q15 =
            long_term_rounded_norm_q15(current_e, norm_shift);
        int64_t gate_rhs =
            ((int64_t)LONG_TERM_GATE_THRESHOLD_Q15_SQ *
             (int64_t)delayed_e_norm_q15 *
             (int64_t)current_e_norm_q15) >>
            15;

        if ((int64_t)r_norm_q15 * (int64_t)r_norm_q15 < gate_rhs) {
            return out;
        }

        out.enabled = 1;
        if (r_norm_q15 == 0 || delayed_e_norm_q15 == 0) {
            return out;
        }

        out.gamma_scaled_gain_q15 = LONG_TERM_MAX_GAMMA_SCALED_GAIN_Q15;
        if (r_norm_q15 <= delayed_e_norm_q15) {
            int16_t gamma_scaled_r_q14 = g729_shr(r_norm_q15, 2);
            int16_t gain_den_q14 = (int16_t)(
                g729_shr(delayed_e_norm_q15, 1) + gamma_scaled_r_q14);
            out.gamma_scaled_gain_q15 =
                g729_div_s(gamma_scaled_r_q14, gain_den_q14);
        }

        out.g1_q14 = g729_shr(out.gamma_scaled_gain_q15, 1);
        out.g0_q14 = (int16_t)(LONG_TERM_IDENTITY_G0_Q14 - out.g1_q14);
    }

    return out;
}

static void apply_long_term_with_gain_q15(
    const g729_postfilter *pf,
    int t,
    long_term_gain_weights weights,
    int16_t r_out[G729_SUBFRAME_SAMPLES]) {
    int n;
    if (!weights.enabled) {
        memcpy(r_out, &pf->past_residual[G729_POSTFILTER_PITCH_MAX],
               G729_SUBFRAME_SAMPLES * sizeof(r_out[0]));
        return;
    }

    {
        int64_t g0_base_q15 = 32767;
        int64_t g0_q15;
        int64_t g1_q15;
        if (weights.gamma_scaled_gain_q15 ==
            LONG_TERM_MAX_GAMMA_SCALED_GAIN_Q15) {
            g0_base_q15 = 1LL << 15;
        }
        g0_q15 = g0_base_q15 - (int64_t)weights.gamma_scaled_gain_q15;
        g1_q15 = (int64_t)weights.gamma_scaled_gain_q15;
        for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
            int64_t p0 = shr64_floor(
                g0_q15 * (int64_t)pf->past_residual[G729_POSTFILTER_PITCH_MAX + n],
                15);
            int64_t p1 = shr64_floor(
                g1_q15 *
                    (int64_t)pf->past_residual[G729_POSTFILTER_PITCH_MAX + n - t],
                15);
            r_out[n] = sat16_i64(p0 + p1);
        }
    }
}

static int16_t compute_tilt_mu(
    const int16_t a_num[G729_POSTFILTER_LPC_ORDER + 1],
    const int16_t a_den[G729_POSTFILTER_LPC_ORDER + 1]) {
    int n;
    int32_t h[TILT_LEN];
    int64_t rh0 = 0;
    int64_t rh1 = 0;

    for (n = 0; n < TILT_LEN; ++n) {
        int k;
        int32_t h_num;
        int32_t acc;
        if (n == 0) {
            h_num = a_num[0];
        } else if (n <= G729_POSTFILTER_LPC_ORDER) {
            h_num = a_num[n];
        } else {
            h_num = 0;
        }
        acc = wrap32_i64((int64_t)h_num * (1LL << 12));
        for (k = 1; k <= G729_POSTFILTER_LPC_ORDER && k <= n; ++k) {
            acc = wrap32_i64((int64_t)acc - (int64_t)a_den[k] * h[n - k]);
        }
        h[n] = wrap32_i64(shr64_floor((int64_t)acc + (1LL << 11), 12));
    }

    for (n = 0; n < TILT_LEN; ++n) {
        rh0 += 2 * (int64_t)h[n] * (int64_t)h[n];
    }
    for (n = 0; n < TILT_LEN - 1; ++n) {
        rh1 += 2 * (int64_t)h[n] * (int64_t)h[n + 1];
    }

    if (rh0 == 0) {
        return 0;
    }
    {
        int16_t den = wrap16_i64(shr64_floor(rh0, 16));
        int16_t num = 0;
        int16_t scaled_num;
        if (den <= 0) {
            return 0;
        }
        if (rh1 > 0) {
            num = wrap16_i64(shr64_floor(rh1, 16));
        }
        if (num <= 0) {
            return 0;
        }
        scaled_num = (int16_t)g729_l_shr(
            (int32_t)GAMMA_TILT_POSITIVE_K1_Q14 * (int32_t)num, 14);
        return g729_div_s(scaled_num, den);
    }
}

static void apply_tilt(g729_postfilter *pf,
                       const int16_t in[G729_SUBFRAME_SAMPLES],
                       int16_t mu_q15,
                       int16_t out[G729_SUBFRAME_SAMPLES]) {
    int n;
    int16_t prev = pf->past_tilt_input;
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int32_t prod = (int32_t)mu_q15 * (int32_t)prev;
        int16_t contrib = (int16_t)g729_l_shr(prod, 15);
        int32_t sum = (int32_t)in[n] - (int32_t)contrib;
        out[n] = sat16_i64(sum);
        prev = in[n];
    }
    pf->past_tilt_input = prev;
}

static void apply_short_term(g729_postfilter *pf,
                             const int16_t a_den[G729_POSTFILTER_LPC_ORDER + 1],
                             const int16_t r_in[G729_SUBFRAME_SAMPLES],
                             int16_t s_out[G729_SUBFRAME_SAMPLES]) {
    int16_t work[G729_POSTFILTER_LPC_ORDER + G729_SUBFRAME_SAMPLES];
    int n;
    memcpy(work, pf->past_synth_post, sizeof(pf->past_synth_post));

    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int i;
        int32_t l_temp = g729_l_mult(r_in[n], a_den[0]);
        for (i = 1; i <= G729_POSTFILTER_LPC_ORDER; ++i) {
            l_temp = g729_l_msu(l_temp, a_den[i],
                                work[G729_POSTFILTER_LPC_ORDER + n - i]);
        }
        l_temp = g729_l_shl(l_temp, 3);
        work[G729_POSTFILTER_LPC_ORDER + n] = g729_round(l_temp);
    }

    memcpy(s_out, &work[G729_POSTFILTER_LPC_ORDER],
           G729_SUBFRAME_SAMPLES * sizeof(s_out[0]));
    memcpy(pf->past_synth_post, &work[G729_SUBFRAME_SAMPLES],
           sizeof(pf->past_synth_post));
}

static int64_t agc_target_energy(const int16_t x[G729_SUBFRAME_SAMPLES]) {
    int n;
    int64_t energy = 0;
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int64_t v = g729_shr(x[n], 2);
        energy += 2 * v * v;
        if (energy > (int64_t)G729_MAX32) {
            return (int64_t)G729_MAX32;
        }
    }
    return energy;
}

static int agc_target_norm_shift(int64_t v) {
    int shift = 0;
    if (v <= 0) {
        return 0;
    }
    while (v < 0x40000000LL) {
        v <<= 1;
        ++shift;
    }
    return shift;
}

static int64_t agc_target_rounded_norm(int64_t v, int shift) {
    int64_t norm;
    if (shift < 0) {
        norm = (((v >> (unsigned)(-shift)) + 0x8000) >> 16);
    } else {
        norm = ((v << (unsigned)shift) + 0x8000) >> 16;
    }
    if (norm > 32767) {
        return 32767;
    }
    return norm;
}

static int64_t isqrt64(int64_t x) {
    int64_t guess;
    if (x <= 0) {
        return 0;
    }
    guess = x;
    for (;;) {
        int64_t next = (guess + x / guess) >> 1;
        if (next >= guess) {
            return guess;
        }
        guess = next;
    }
}

static int64_t agc_inverse_sqrt_table_value(int index) {
    const int64_t table_scale = 32768;
    int64_t numerator = (int64_t)16 * table_scale * table_scale;
    int64_t denominator = (int64_t)(16 + index);
    int64_t root = isqrt64(numerator / denominator);
    while ((root + 1) * (root + 1) * denominator <= numerator) {
        ++root;
    }
    {
        int64_t lo_diff = numerator - root * root * denominator;
        int64_t hi_diff = (root + 1) * (root + 1) * denominator - numerator;
        if (hi_diff < lo_diff) {
            ++root;
        }
    }
    if (root > 32767) {
        return 32767;
    }
    return root;
}

static int64_t agc_inverse_sqrt_q12(int64_t x) {
    int norm_shift;
    int64_t norm_x;
    int64_t adjusted_x;
    int index;
    int64_t frac;
    int64_t base;
    int64_t next;
    int64_t acc;
    int denorm_shift;
    int64_t out;

    if (x <= 0) {
        return 0;
    }
    norm_shift = agc_target_norm_shift(x);
    norm_x = x << (unsigned)norm_shift;
    adjusted_x = norm_x;
    if (norm_shift % 2 == 0) {
        adjusted_x >>= 1;
    }

    index = (int)((adjusted_x >> 25) - 16);
    if (index < 0) {
        index = 0;
    }
    frac = (adjusted_x >> 10) & 0x7fff;

    base = agc_inverse_sqrt_table_value(index);
    next = agc_inverse_sqrt_table_value(index + 1);
    acc = (base << 16) - 2 * (base - next) * frac;
    denorm_shift = 16 - ((norm_shift + 1) >> 1);
    out = acc >> (unsigned)denorm_shift;
    return (out + 64) >> 7;
}

static int64_t agc_target_sqrt_input_over_post_q12(int64_t input_energy,
                                                   int64_t post_energy) {
    int post_shift = agc_target_norm_shift(post_energy);
    int input_shift = agc_target_norm_shift(input_energy);
    int64_t post_norm;
    int64_t input_norm;
    int16_t ratio_div_q15;
    int exp_delta;
    int shift;
    int64_t ratio_q28;

    --post_shift;
    post_norm = agc_target_rounded_norm(post_energy, post_shift);
    input_norm = agc_target_rounded_norm(input_energy, input_shift);
    if (post_norm <= 0 || input_norm <= 0) {
        return 0;
    }

    ratio_div_q15 = g729_div_s((int16_t)post_norm, (int16_t)input_norm);
    exp_delta = post_shift - input_shift;
    shift = 7 - exp_delta;
    ratio_q28 = ratio_div_q15;
    if (shift >= 0) {
        ratio_q28 <<= (unsigned)shift;
    } else {
        ratio_q28 >>= (unsigned)(-shift);
    }
    return agc_inverse_sqrt_q12(ratio_q28);
}

static int16_t compute_agc_target_gain(const int16_t s[G729_SUBFRAME_SAMPLES],
                                       const int16_t s_tilt[G729_SUBFRAME_SAMPLES]) {
    int64_t e_s = agc_target_energy(s);
    int64_t e_t = agc_target_energy(s_tilt);
    int64_t sqrt_q12;
    int64_t target_q12;
    if (e_s == 0 || e_t == 0) {
        return 0;
    }
    sqrt_q12 = agc_target_sqrt_input_over_post_q12(e_s, e_t);
    target_q12 = (sqrt_q12 * (int64_t)AGC_ALPHA_COMPLEMENT_Q15) >> 15;
    return (int16_t)(target_q12 << 2);
}

static void apply_agc(g729_postfilter *pf,
                      const int16_t s_tilt[G729_SUBFRAME_SAMPLES],
                      int16_t target_q14,
                      int16_t out[G729_SUBFRAME_SAMPLES]) {
    int n;
    int64_t g;
    int64_t target_q12;

    if (!pf->initialized) {
        pf->agc_gain_prev = 1 << 24;
        pf->initialized = 1;
    }
    if (target_q14 == 0 && agc_target_energy(s_tilt) == 0) {
        memcpy(out, s_tilt, G729_SUBFRAME_SAMPLES * sizeof(out[0]));
        pf->agc_gain_prev = 0;
        return;
    }

    g = pf->agc_gain_prev;
    target_q12 = g729_shr(target_q14, 2);
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int64_t mul_prev = ((int64_t)AGC_ALPHA_Q15 * g) >> 27;
        int64_t acc = mul_prev + target_q12;
        int64_t prod;
        int64_t v;
        g = acc << 12;
        prod = g * (int64_t)s_tilt[n];
        v = shr64_floor(prod, 24);
        out[n] = sat16_i64(v);
    }
    if (g < 0) {
        g = 0;
    }
    pf->agc_gain_prev = (int32_t)g;
}

void g729_postfilter_reset(g729_postfilter *pf) {
    if (pf == NULL) {
        return;
    }
    memset(pf, 0, sizeof(*pf));
}

void g729_postfilter_filter(g729_postfilter *pf,
                            const int16_t a[G729_POSTFILTER_LPC_ORDER + 1],
                            int t_int,
                            const int16_t s[G729_SUBFRAME_SAMPLES],
                            int16_t out[G729_SUBFRAME_SAMPLES]) {
    int16_t a_num[G729_POSTFILTER_LPC_ORDER + 1];
    int16_t a_den[G729_POSTFILTER_LPC_ORDER + 1];
    int16_t r[G729_SUBFRAME_SAMPLES];
    int16_t r_out[G729_SUBFRAME_SAMPLES];
    int16_t s_tilt[G729_SUBFRAME_SAMPLES];
    int16_t s_st[G729_SUBFRAME_SAMPLES];
    int t;
    long_term_gain_weights weights;
    int16_t mu_q15;
    int16_t target_q14;

    if (pf == NULL || a == NULL || s == NULL || out == NULL) {
        return;
    }
    if (!pf->initialized) {
        pf->agc_gain_prev = 1 << 24;
    }

    expand_bandwidth(a, GAMMA_NUM_Q15, a_num);
    expand_bandwidth(a, GAMMA_DEN_Q15, a_den);
    compute_residual(pf, a_num, s, r);
    memcpy(&pf->past_residual[G729_POSTFILTER_PITCH_MAX], r,
           G729_SUBFRAME_SAMPLES * sizeof(r[0]));

    t = refine_pitch(pf, t_int);
    weights = compute_long_term_gain_weights(pf, r, t);
    apply_long_term_with_gain_q15(pf, t, weights, r_out);

    mu_q15 = compute_tilt_mu(a_num, a_den);
    apply_tilt(pf, r_out, mu_q15, s_tilt);
    apply_short_term(pf, a_den, s_tilt, s_st);

    target_q14 = compute_agc_target_gain(s, s_st);
    apply_agc(pf, s_st, target_q14, out);

    memmove(pf->past_residual, &pf->past_residual[G729_SUBFRAME_SAMPLES],
            G729_POSTFILTER_PITCH_MAX * sizeof(pf->past_residual[0]));
    memcpy(&pf->past_residual[G729_POSTFILTER_PITCH_MAX], r,
           G729_SUBFRAME_SAMPLES * sizeof(r[0]));
}
