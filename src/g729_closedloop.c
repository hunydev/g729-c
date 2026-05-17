#include "g729_closedloop.h"

#include <stddef.h>
#include <stdint.h>

#include "g729_fixed.h"
#include "g729_pitch.h"

static const int16_t gamma_pow_q15[G729_LPC_ORDER + 1] = {
    32767, 24576, 18432, 13824, 10368, 7776,
    5832,  4374,  3281,  2460,  1845,
};

static const int16_t pitch_interp_fir[31] = {
    29443,
    25207, 14701, 3143,
    -4402, -5850, -2783,
    1211, 3130, 2259,
    0, -1652, -1666,
    -464, 756, 1099,
    550, -245, -634,
    -451, 0, 308,
    296, 78, -120,
    -165, -79, 34,
    91, 70, 0,
};

static int32_t saturate32_from_i64(int64_t x) {
    if (x > (int64_t)G729_MAX32) {
        return G729_MAX32;
    }
    if (x < (int64_t)G729_MIN32) {
        return G729_MIN32;
    }
    return (int32_t)x;
}

static int16_t saturate16_from_i64(int64_t x) {
    if (x > (int64_t)G729_MAX16) {
        return G729_MAX16;
    }
    if (x < (int64_t)G729_MIN16) {
        return G729_MIN16;
    }
    return (int16_t)x;
}

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

static int64_t round_shift64(int64_t x, unsigned n) {
    int64_t add;
    if (n == 0u) {
        return x;
    }
    add = (int64_t)1 << (n - 1u);
    if (x >= 0) {
        return (x + add) >> n;
    }
    return -(((-x) + add) >> n);
}

static void gamma_weight_lp(const int16_t a[G729_LPC_ORDER + 1],
                            int16_t out[G729_LPC_ORDER + 1]) {
    int i;
    out[0] = a[0];
    for (i = 1; i <= G729_LPC_ORDER; ++i) {
        out[i] = g729_mult(a[i], gamma_pow_q15[i]);
    }
}

void g729_closedloop_lp_residual_subframe(
    const int16_t speech[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t a_hat[G729_LPC_ORDER + 1],
    const int16_t mem[G729_LPC_ORDER],
    int16_t residual[G729_CLOSEDLOOP_SUBFRAME_LEN]) {
    int n;
    if (speech == NULL || a_hat == NULL || mem == NULL || residual == NULL) {
        return;
    }
    for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
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

void g729_closedloop_target_signal(
    const int16_t a_hat[G729_LPC_ORDER + 1],
    const int16_t residual[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t sw_mem[G729_LPC_ORDER],
    int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN]) {
    int16_t aw[G729_LPC_ORDER + 1];
    int n;
    if (a_hat == NULL || residual == NULL || sw_mem == NULL || x == NULL) {
        return;
    }
    gamma_weight_lp(a_hat, aw);
    for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
        int i;
        int32_t acc = g729_l_mult(residual[n], aw[0]);
        for (i = 1; i <= G729_LPC_ORDER; ++i) {
            int16_t xni;
            if (n - i >= 0) {
                xni = x[n - i];
            } else {
                xni = sw_mem[G729_LPC_ORDER + n - i];
            }
            acc = g729_l_msu(acc, aw[i], xni);
        }
        x[n] = g729_round(g729_l_shl(acc, 3));
    }
}

void g729_closedloop_impulse_response(
    const int16_t a_hat[G729_LPC_ORDER + 1],
    int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN]) {
    int16_t aw[G729_LPC_ORDER + 1];
    int n;
    if (a_hat == NULL || h == NULL) {
        return;
    }
    gamma_weight_lp(a_hat, aw);
    for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
        int i;
        int limit = n;
        int32_t acc = 0;
        if (n == 0) {
            acc = g729_l_mult(4096, aw[0]);
        }
        if (limit > G729_LPC_ORDER) {
            limit = G729_LPC_ORDER;
        }
        for (i = 1; i <= limit; ++i) {
            acc = g729_l_msu(acc, aw[i], h[n - i]);
        }
        h[n] = g729_round(g729_l_shl(acc, 3));
    }
}

void g729_closedloop_backward_filter(
    const int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN],
    int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN]) {
    int n;
    if (x == NULL || h == NULL || xb == NULL) {
        return;
    }
    for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
        int m;
        int64_t acc = 0;
        for (m = n; m < G729_CLOSEDLOOP_SUBFRAME_LEN; ++m) {
            acc += (int64_t)x[m] * (int64_t)h[m - n];
        }
        xb[n] = saturate16_from_i64(shr64_floor(acc, 12));
    }
}

void g729_closedloop_subframe1_window(int16_t top,
                                      int16_t *tmin,
                                      int16_t *tmax) {
    int16_t lo = (int16_t)(top - 3);
    int16_t hi;
    if (lo < G729_CLOSEDLOOP_PITCH_MIN) {
        lo = G729_CLOSEDLOOP_PITCH_MIN;
    }
    hi = (int16_t)(lo + 6);
    if (hi > G729_CLOSEDLOOP_PITCH_MAX) {
        hi = G729_CLOSEDLOOP_PITCH_MAX;
        lo = (int16_t)(hi - 6);
    }
    if (tmin != NULL) {
        *tmin = lo;
    }
    if (tmax != NULL) {
        *tmax = hi;
    }
}

void g729_closedloop_subframe2_window(int16_t int_t1,
                                      int16_t *tmin,
                                      int16_t *tmax) {
    int16_t lo = (int16_t)(int_t1 - 5);
    int16_t hi;
    if (lo < G729_CLOSEDLOOP_PITCH_MIN) {
        lo = G729_CLOSEDLOOP_PITCH_MIN;
    }
    hi = (int16_t)(lo + 9);
    if (hi > G729_CLOSEDLOOP_PITCH_MAX) {
        hi = G729_CLOSEDLOOP_PITCH_MAX;
        lo = (int16_t)(hi - 9);
    }
    if (tmin != NULL) {
        *tmin = lo;
    }
    if (tmax != NULL) {
        *tmax = hi;
    }
}

g729_closedloop_pitch_result g729_closedloop_search_integer(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t centre,
    int subframe) {
    g729_closedloop_pitch_result result;
    int16_t tmin;
    int16_t tmax;
    int base = G729_CLOSEDLOOP_SEARCH_LEN - G729_CLOSEDLOOP_SUBFRAME_LEN;
    int k;
    int64_t best = INT64_MIN;

    result.int_lag = 0;
    result.frac = 0;
    result.rn = 0;
    if (xb == NULL || exc == NULL) {
        return result;
    }

    if (subframe == 0) {
        g729_closedloop_subframe1_window(centre, &tmin, &tmax);
    } else {
        g729_closedloop_subframe2_window(centre, &tmin, &tmax);
    }
    result.int_lag = tmin;
    for (k = tmin; k <= tmax; ++k) {
        int n;
        int exc_base = base - k;
        int64_t acc = 0;
        for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
            acc += 2 * (int64_t)xb[n] * (int64_t)exc[exc_base + n];
        }
        if (acc > best) {
            best = acc;
            result.int_lag = (int16_t)k;
        }
    }
    result.rn = saturate32_from_i64(best);
    return result;
}

int16_t g729_closedloop_interpolate3(
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int8_t frac) {
    int anchor = G729_CLOSEDLOOP_SEARCH_LEN - G729_CLOSEDLOOP_SUBFRAME_LEN;
    int k;
    int pos_phase;
    int neg_phase;
    int base;
    int i;
    int32_t acc = 0;
    if (exc == NULL) {
        return 0;
    }
    if (frac == 0) {
        return exc[anchor - int_lag];
    }
    if (frac < 0) {
        k = int_lag;
        pos_phase = 1;
        neg_phase = 2;
    } else {
        k = int_lag + 1;
        pos_phase = 2;
        neg_phase = 1;
    }

    base = anchor - k;
    for (i = 0; i < G729_CLOSEDLOOP_PITCH_LINTER; ++i) {
        int back_idx = base - i;
        int fwd_idx = base + 1 + i;
        int16_t back = 0;
        int16_t fwd = 0;
        if (back_idx >= 0 && back_idx < G729_CLOSEDLOOP_SEARCH_LEN) {
            back = exc[back_idx];
        }
        if (fwd_idx >= 0 && fwd_idx < G729_CLOSEDLOOP_SEARCH_LEN) {
            fwd = exc[fwd_idx];
        }
        acc = g729_l_mac(acc, pitch_interp_fir[pos_phase + 3 * i], back);
        acc = g729_l_mac(acc, pitch_interp_fir[neg_phase + 3 * i], fwd);
    }
    return g729_round(acc);
}

static int64_t correlate_at_frac(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int8_t frac) {
    int n;
    int64_t acc = 0;
    for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
        int16_t s =
            g729_closedloop_interpolate3(exc, (int16_t)(int_lag - n), frac);
        acc += 2 * (int64_t)xb[n] * (int64_t)s;
    }
    return acc;
}

int8_t g729_closedloop_refine_fraction(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int allow_frac) {
    int8_t best_frac = -1;
    int64_t best_rn;
    int8_t frac;
    if (xb == NULL || exc == NULL) {
        return 0;
    }
    if (!allow_frac) {
        return 0;
    }
    best_rn = correlate_at_frac(xb, exc, int_lag, -1);
    for (frac = 0; frac <= 1; ++frac) {
        int64_t rn = correlate_at_frac(xb, exc, int_lag, frac);
        if (rn > best_rn) {
            best_rn = rn;
            best_frac = frac;
        }
    }
    return best_frac;
}

void g729_closedloop_refine_fraction_subframe1(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int16_t *out_lag,
    int8_t *out_frac) {
    int16_t best_lag = int_lag;
    int8_t best_frac = -1;
    int best_set = 0;
    int64_t best_rn = 0;

#define CONSIDER(lag_value, frac_value) do { \
    int64_t rn__ = correlate_at_frac(xb, exc, (lag_value), (frac_value)); \
    if (!best_set || rn__ > best_rn) { \
        best_set = 1; \
        best_rn = rn__; \
        best_lag = (lag_value); \
        best_frac = (frac_value); \
    } \
} while (0)

    if (xb == NULL || exc == NULL) {
        best_lag = int_lag;
        best_frac = 0;
    } else if (int_lag >= 85) {
        best_lag = int_lag;
        best_frac = 0;
    } else {
        if (int_lag == G729_CLOSEDLOOP_PITCH_MIN) {
            CONSIDER((int16_t)(G729_CLOSEDLOOP_PITCH_MIN - 1), 1);
        }
        CONSIDER(int_lag, -1);
        CONSIDER(int_lag, 0);
        CONSIDER(int_lag, 1);
        if (int_lag == 84) {
            CONSIDER(85, -1);
        }
    }

#undef CONSIDER

    if (out_lag != NULL) {
        *out_lag = best_lag;
    }
    if (out_frac != NULL) {
        *out_frac = best_frac;
    }
}

void g729_closedloop_refine_fraction_subframe2(
    const int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int16_t int_t1,
    int16_t *out_lag,
    int8_t *out_frac) {
    int16_t tmin;
    int16_t tmax;
    int16_t best_lag = int_lag;
    int8_t best_frac = -1;
    int best_set = 0;
    int64_t best_rn = 0;

#define CONSIDER(lag_value, frac_value) do { \
    int64_t rn__ = correlate_at_frac(xb, exc, (lag_value), (frac_value)); \
    if (!best_set || rn__ > best_rn) { \
        best_set = 1; \
        best_rn = rn__; \
        best_lag = (lag_value); \
        best_frac = (frac_value); \
    } \
} while (0)

    if (xb == NULL || exc == NULL) {
        best_lag = int_lag;
        best_frac = 0;
    } else {
        g729_closedloop_subframe2_window(int_t1, &tmin, &tmax);
        if (int_lag == tmin) {
            CONSIDER((int16_t)(tmin - 1), 1);
        }
        CONSIDER(int_lag, -1);
        CONSIDER(int_lag, 0);
        CONSIDER(int_lag, 1);
        if (int_lag == tmax) {
            CONSIDER((int16_t)(tmax + 1), -1);
        }
    }

#undef CONSIDER

    if (out_lag != NULL) {
        *out_lag = best_lag;
    }
    if (out_frac != NULL) {
        *out_frac = best_frac;
    }
}

void g729_closedloop_adaptive_vector(
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int8_t frac,
    int16_t v[G729_CLOSEDLOOP_SUBFRAME_LEN]) {
    int n;
    int base = G729_CLOSEDLOOP_SEARCH_LEN - G729_CLOSEDLOOP_SUBFRAME_LEN -
               int_lag;
    if (exc == NULL || v == NULL) {
        return;
    }
    if (frac == 0) {
        for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
            v[n] = exc[base + n];
        }
        return;
    }
    for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
        v[n] =
            g729_closedloop_interpolate3(exc, (int16_t)(int_lag - n), frac);
    }
}

int16_t g729_closedloop_gp_and_y(
    const int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t v[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN],
    int16_t y[G729_CLOSEDLOOP_SUBFRAME_LEN]) {
    int n;
    int64_t num = 0;
    int64_t den = 0;
    if (x == NULL || v == NULL || h == NULL || y == NULL) {
        return 0;
    }
    for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
        int i;
        int64_t acc = 0;
        for (i = 0; i <= n; ++i) {
            acc += (int64_t)v[i] * (int64_t)h[n - i];
        }
        y[n] = saturate16_from_i64(round_shift64(acc, 12));
    }
    for (n = 0; n < G729_CLOSEDLOOP_SUBFRAME_LEN; ++n) {
        int64_t yn = y[n];
        num += (int64_t)x[n] * yn;
        den += yn * yn;
    }
    if (den <= 0 || num <= 0) {
        return 0;
    }
    if (num * 5 >= den * 6) {
        return G729_CLOSEDLOOP_GP_UPPER_Q14;
    }
    return (int16_t)((num << 14) / den);
}

uint8_t g729_closedloop_encode_p1(int16_t int_lag, int8_t frac) {
    if (int_lag < 85 || (int_lag == 85 && frac <= 0)) {
        return (uint8_t)(3 * (int_lag - 19) + (int16_t)frac - 1);
    }
    return (uint8_t)(int_lag + 112);
}

uint8_t g729_closedloop_encode_p2(int16_t int_lag,
                                  int8_t frac,
                                  int16_t tmin) {
    return (uint8_t)(3 * (int_lag - tmin) + (int16_t)frac + 2);
}

uint8_t g729_closedloop_encode_p0(uint8_t p1) {
    return g729_pitch_parity(p1);
}
