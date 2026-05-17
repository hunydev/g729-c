#include "g729_openloop.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "g729_fixed.h"

enum {
    OPENLOOP_SUBFRAME_SAMPLES = 40,
    OPENLOOP_CORR_SAMPLES = 40,
    OPENLOOP_GAMMA07_Q15 = 22938,
    OPENLOOP_SUBMULTIPLE_LIFT_NUM = 11,
    OPENLOOP_SUBMULTIPLE_LIFT_DEN = 10,
    OPENLOOP_SUBMULTIPLE_TOLERANCE = 2
};

static const int16_t gamma_pow_q15[G729_LPC_ORDER + 1] = {
    32767, 24576, 18432, 13824, 10368, 7776,
    5832,  4374,  3281,  2460,  1845,
};

static int bit_len_u64(uint64_t v) {
    int n = 0;
    while (v != 0u) {
        ++n;
        v >>= 1;
    }
    return n;
}

static int16_t wrap16_from_i32(int32_t x) {
    uint16_t u = (uint16_t)((uint32_t)x & 0xffffu);
    if (u <= 32767u) {
        return (int16_t)u;
    }
    return (int16_t)((int32_t)u - 65536);
}

static void gamma_weight_lp(const int16_t a[G729_LPC_ORDER + 1],
                            int16_t out[G729_LPC_ORDER + 1]) {
    int i;
    out[0] = a[0];
    for (i = 1; i <= G729_LPC_ORDER; ++i) {
        out[i] = g729_mult(a[i], gamma_pow_q15[i]);
    }
}

static void combine_with_07(const int16_t aw[G729_LPC_ORDER + 1],
                            int16_t out[G729_LPC_ORDER + 1]) {
    int i;
    out[0] = aw[0];
    for (i = 1; i <= G729_LPC_ORDER; ++i) {
        out[i] =
            wrap16_from_i32((int32_t)aw[i] -
                            (int32_t)g729_mult_r(OPENLOOP_GAMMA07_Q15,
                                                 aw[i - 1]));
    }
}

static void lp_residual_subframe(const int16_t speech[OPENLOOP_SUBFRAME_SAMPLES],
                                 const int16_t a_hat[G729_LPC_ORDER + 1],
                                 const int16_t mem[G729_LPC_ORDER],
                                 int16_t residual[OPENLOOP_SUBFRAME_SAMPLES]) {
    int n;
    for (n = 0; n < OPENLOOP_SUBFRAME_SAMPLES; ++n) {
        int i;
        int32_t acc = g729_l_mult(speech[n], a_hat[0]);
        for (i = 1; i <= G729_LPC_ORDER; ++i) {
            int16_t sni;
            if (n - i >= 0) {
                sni = speech[n - i];
            } else {
                sni = mem[G729_LPC_ORDER + n - i];
            }
            acc = g729_l_mac(acc, a_hat[i], sni);
        }
        residual[n] = g729_round(g729_l_shl(acc, 3));
    }
}

static void lp_residual_split(const int16_t speech[G729_FRAME_SAMPLES],
                              const int16_t a_hat_sf1[G729_LPC_ORDER + 1],
                              const int16_t a_hat_sf2[G729_LPC_ORDER + 1],
                              const int16_t mem[G729_LPC_ORDER],
                              int16_t residual[G729_FRAME_SAMPLES]) {
    int16_t mem2[G729_LPC_ORDER];
    memcpy(mem2, &speech[OPENLOOP_SUBFRAME_SAMPLES - G729_LPC_ORDER],
           sizeof(mem2));
    lp_residual_subframe(speech, a_hat_sf1, mem, residual);
    lp_residual_subframe(&speech[OPENLOOP_SUBFRAME_SAMPLES], a_hat_sf2,
                         mem2, &residual[OPENLOOP_SUBFRAME_SAMPLES]);
}

static void lowpass_weighted_speech_subframe(
    const int16_t residual[OPENLOOP_SUBFRAME_SAMPLES],
    const int16_t a_prime[G729_LPC_ORDER + 1],
    const int16_t mem[G729_LPC_ORDER],
    int16_t sw[OPENLOOP_SUBFRAME_SAMPLES]) {
    int n;
    for (n = 0; n < OPENLOOP_SUBFRAME_SAMPLES; ++n) {
        int i;
        int32_t acc = g729_l_mult(residual[n], a_prime[0]);
        for (i = 1; i <= G729_LPC_ORDER; ++i) {
            int16_t swni;
            if (n - i >= 0) {
                swni = sw[n - i];
            } else {
                swni = mem[G729_LPC_ORDER + n - i];
            }
            acc = g729_l_msu(acc, a_prime[i], swni);
        }
        sw[n] = g729_round(g729_l_shl(acc, 3));
    }
}

static void lowpass_weighted_speech_split(
    const int16_t residual[G729_FRAME_SAMPLES],
    const int16_t a_prime_sf1[G729_LPC_ORDER + 1],
    const int16_t a_prime_sf2[G729_LPC_ORDER + 1],
    const int16_t mem[G729_LPC_ORDER],
    int16_t sw[G729_FRAME_SAMPLES]) {
    int16_t mem2[G729_LPC_ORDER];
    lowpass_weighted_speech_subframe(residual, a_prime_sf1, mem, sw);
    memcpy(mem2, &sw[OPENLOOP_SUBFRAME_SAMPLES - G729_LPC_ORDER],
           sizeof(mem2));
    lowpass_weighted_speech_subframe(&residual[OPENLOOP_SUBFRAME_SAMPLES],
                                     a_prime_sf2, mem2,
                                     &sw[OPENLOOP_SUBFRAME_SAMPLES]);
}

static void slide_old_wspeech(
    int16_t old_wspeech[G729_OPENLOOP_OLD_WSPEECH_LEN],
    const int16_t fresh[G729_FRAME_SAMPLES]) {
    memmove(old_wspeech, &old_wspeech[G729_FRAME_SAMPLES],
            (G729_OPENLOOP_OLD_WSPEECH_LEN - G729_FRAME_SAMPLES) *
                sizeof(old_wspeech[0]));
    memcpy(&old_wspeech[G729_OPENLOOP_OLD_WSPEECH_LEN - G729_FRAME_SAMPLES],
           fresh, G729_FRAME_SAMPLES * sizeof(fresh[0]));
}

static int32_t correlate_at(const int16_t wsp[G729_OPENLOOP_WSPEECH_LEN],
                            int k) {
    int n;
    int32_t acc = 0;
    for (n = 0; n < OPENLOOP_CORR_SAMPLES; ++n) {
        acc = g729_l_mac(acc,
                         wsp[G729_OPENLOOP_OLD_WSPEECH_LEN + 2 * n],
                         wsp[G729_OPENLOOP_OLD_WSPEECH_LEN + 2 * n - k]);
    }
    return acc;
}

static g729_openloop_range_score
pick_best_full_scan(const int16_t wsp[G729_OPENLOOP_WSPEECH_LEN],
                    int k_min,
                    int k_max);

static int32_t energy_at(const int16_t wsp[G729_OPENLOOP_WSPEECH_LEN],
                         int k) {
    int n;
    int32_t acc = 0;
    for (n = 0; n < OPENLOOP_CORR_SAMPLES; ++n) {
        int32_t s = wsp[G729_OPENLOOP_OLD_WSPEECH_LEN + 2 * n - k];
        int32_t sq = (int32_t)((int64_t)s * (int64_t)s);
        acc = g729_l_add(acc, sq);
    }
    return acc;
}

static g729_openloop_range_score
pick_best_full_scan(const int16_t wsp[G729_OPENLOOP_WSPEECH_LEN],
                    int k_min,
                    int k_max) {
    int k;
    g729_openloop_range_score best;
    best.lag = (int16_t)k_min;
    best.r = G729_MIN32;
    best.e = 0;
    for (k = k_min; k <= k_max; ++k) {
        int32_t rk = correlate_at(wsp, k);
        if (rk > best.r) {
            best.r = rk;
            best.lag = (int16_t)k;
        }
    }
    best.e = energy_at(wsp, best.lag);
    return best;
}

static g729_openloop_range_score
pick_best_even_with_refinement(
    const int16_t wsp[G729_OPENLOOP_WSPEECH_LEN]) {
    int k;
    int best_even;
    int lo;
    int hi;
    g729_openloop_range_score best;

    best.lag = 80;
    best.r = correlate_at(wsp, 80);
    best.e = 0;
    for (k = 82; k <= 142; k += 2) {
        int32_t rk = correlate_at(wsp, k);
        if (rk > best.r) {
            best.r = rk;
            best.lag = (int16_t)k;
        }
    }

    best_even = best.lag;
    lo = best_even - 1;
    hi = best_even + 1;
    if (lo < 80) {
        lo = 80;
    }
    if (hi > 143) {
        hi = 143;
    }

    best.lag = (int16_t)lo;
    best.r = correlate_at(wsp, lo);
    for (k = lo + 1; k <= hi; ++k) {
        int32_t rk = correlate_at(wsp, k);
        if (rk > best.r) {
            best.r = rk;
            best.lag = (int16_t)k;
        }
    }
    best.e = energy_at(wsp, best.lag);
    return best;
}

static g729_openloop_range_score
pick_best_in_range(const int16_t wsp[G729_OPENLOOP_WSPEECH_LEN],
                   int k_min,
                   int k_max) {
    if (k_min == 80 && k_max == 143) {
        return pick_best_even_with_refinement(wsp);
    }
    return pick_best_full_scan(wsp, k_min, k_max);
}

static int compare_normalized(int32_t r1, int32_t e1, int32_t r2, int32_t e2) {
    int score1_zero = e1 <= 0 || r1 <= 0;
    int score2_zero = e2 <= 0 || r2 <= 0;
    int64_t rr1;
    int64_t rr2;
    int64_t max_r;
    int shift = 0;
    if (score1_zero && score2_zero) {
        return 1;
    }
    if (score1_zero) {
        return 0;
    }
    if (score2_zero) {
        return 1;
    }

    rr1 = r1;
    rr2 = r2;
    max_r = rr1;
    if (rr2 > max_r) {
        max_r = rr2;
    }
    {
        int len = bit_len_u64((uint64_t)max_r);
        if (len > 15) {
            shift = len - 15;
        }
    }
    rr1 >>= shift;
    rr2 >>= shift;
    return rr1 * rr1 * (int64_t)e2 >= rr2 * rr2 * (int64_t)e1;
}

static int strict_greater(int32_t r_h, int32_t e_h, int32_t r_op,
                          int32_t e_op) {
    return !compare_normalized(r_op, e_op, r_h, e_h);
}

static int lifted_strict_greater(int32_t r_h,
                                 int32_t e_h,
                                 int32_t r_op,
                                 int32_t e_op,
                                 int lift_num,
                                 int lift_den) {
    int64_t rh;
    int64_t ro;
    int64_t max_r;
    int shift = 0;
    if (e_h <= 0 || r_h <= 0) {
        return 0;
    }
    if (e_op <= 0 || r_op <= 0) {
        return 1;
    }
    rh = r_h;
    ro = r_op;
    max_r = rh;
    if (ro > max_r) {
        max_r = ro;
    }
    {
        int len = bit_len_u64((uint64_t)max_r);
        if (len > 13) {
            shift = len - 13;
        }
    }
    rh >>= shift;
    ro >>= shift;
    return rh * rh * (int64_t)e_op * (int64_t)lift_den >
           ro * ro * (int64_t)e_h * (int64_t)lift_num;
}

static int is_near_submultiple(int higher, int lower) {
    int k;
    if (lower <= 0) {
        return 0;
    }
    for (k = 2; k <= 7; ++k) {
        int multiple = k * lower;
        int d = higher - multiple;
        if (d < 0) {
            d = -d;
        }
        if (d <= OPENLOOP_SUBMULTIPLE_TOLERANCE) {
            return 1;
        }
        if (multiple > higher + OPENLOOP_SUBMULTIPLE_TOLERANCE) {
            return 0;
        }
    }
    return 0;
}

static int should_override(int32_t r_h,
                           int32_t e_h,
                           int16_t lag_h,
                           int32_t r_op,
                           int32_t e_op,
                           int16_t lag_op) {
    if (is_near_submultiple(lag_h, lag_op)) {
        return lifted_strict_greater(r_h, e_h, r_op, e_op,
                                     OPENLOOP_SUBMULTIPLE_LIFT_NUM,
                                     OPENLOOP_SUBMULTIPLE_LIFT_DEN);
    }
    return strict_greater(r_h, e_h, r_op, e_op);
}

static int should_override_range3(g729_openloop_range_score r3,
                                  g729_openloop_range_score current,
                                  g729_openloop_range_score r1,
                                  g729_openloop_range_score r2) {
    if (is_near_submultiple(r3.lag, r1.lag) &&
        !lifted_strict_greater(r3.r, r3.e, r1.r, r1.e,
                               OPENLOOP_SUBMULTIPLE_LIFT_NUM,
                               OPENLOOP_SUBMULTIPLE_LIFT_DEN)) {
        return 0;
    }
    if (is_near_submultiple(r3.lag, r2.lag) &&
        !lifted_strict_greater(r3.r, r3.e, r2.r, r2.e,
                               OPENLOOP_SUBMULTIPLE_LIFT_NUM,
                               OPENLOOP_SUBMULTIPLE_LIFT_DEN)) {
        return 0;
    }
    return should_override(r3.r, r3.e, r3.lag, current.r, current.e,
                           current.lag);
}

static int16_t merge_three_ranges(g729_openloop_range_score r1,
                                  g729_openloop_range_score r2,
                                  g729_openloop_range_score r3) {
    g729_openloop_range_score best = r1;
    if (should_override(r2.r, r2.e, r2.lag, best.r, best.e, best.lag)) {
        best = r2;
    }
    if (should_override_range3(r3, best, r1, r2)) {
        best = r3;
    }
    return best.lag;
}

g729_openloop_search_result
g729_openloop_search_with_ranges(
    const int16_t wsp[G729_OPENLOOP_WSPEECH_LEN]) {
    g729_openloop_search_result result;
    memset(&result, 0, sizeof(result));
    if (wsp == NULL) {
        return result;
    }

    result.range3 = pick_best_in_range(wsp, 80, 143);
    result.range2 = pick_best_in_range(wsp, 40, 79);
    result.range1 = pick_best_in_range(wsp, 20, 39);
    result.top =
        merge_three_ranges(result.range1, result.range2, result.range3);
    return result;
}

g729_openloop_search_result
g729_openloop_step_split_search(
    const int16_t a_hat_sf1[G729_LPC_ORDER + 1],
    const int16_t a_hat_sf2[G729_LPC_ORDER + 1],
    const int16_t speech[G729_FRAME_SAMPLES],
    int16_t residual_mem[G729_LPC_ORDER],
    int16_t sw_mem[G729_LPC_ORDER],
    int16_t old_wspeech[G729_OPENLOOP_OLD_WSPEECH_LEN]) {
    int16_t aw1[G729_LPC_ORDER + 1];
    int16_t aw2[G729_LPC_ORDER + 1];
    int16_t a_prime1[G729_LPC_ORDER + 1];
    int16_t a_prime2[G729_LPC_ORDER + 1];
    int16_t residual[G729_FRAME_SAMPLES];
    int16_t fresh_sw[G729_FRAME_SAMPLES];
    int16_t wsp[G729_OPENLOOP_WSPEECH_LEN];
    g729_openloop_search_result result;

    memset(&result, 0, sizeof(result));
    if (a_hat_sf1 == NULL || a_hat_sf2 == NULL || speech == NULL ||
        residual_mem == NULL || sw_mem == NULL || old_wspeech == NULL) {
        return result;
    }

    gamma_weight_lp(a_hat_sf1, aw1);
    gamma_weight_lp(a_hat_sf2, aw2);
    combine_with_07(aw1, a_prime1);
    combine_with_07(aw2, a_prime2);

    lp_residual_split(speech, a_hat_sf1, a_hat_sf2, residual_mem, residual);
    lowpass_weighted_speech_split(residual, a_prime1, a_prime2, sw_mem,
                                  fresh_sw);

    memcpy(wsp, old_wspeech, G729_OPENLOOP_OLD_WSPEECH_LEN * sizeof(wsp[0]));
    memcpy(&wsp[G729_OPENLOOP_OLD_WSPEECH_LEN], fresh_sw,
           G729_FRAME_SAMPLES * sizeof(wsp[0]));
    result = g729_openloop_search_with_ranges(wsp);

    memcpy(residual_mem, &speech[G729_FRAME_SAMPLES - G729_LPC_ORDER],
           G729_LPC_ORDER * sizeof(residual_mem[0]));
    memcpy(sw_mem, &fresh_sw[G729_FRAME_SAMPLES - G729_LPC_ORDER],
           G729_LPC_ORDER * sizeof(sw_mem[0]));
    slide_old_wspeech(old_wspeech, fresh_sw);

    return result;
}
