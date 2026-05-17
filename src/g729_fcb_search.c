#include "g729_fcb_search.h"

#include <stddef.h>
#include <stdint.h>

#include "g729_fixed.h"

static const int8_t track0[8] = {0, 5, 10, 15, 20, 25, 30, 35};
static const int8_t track1[8] = {1, 6, 11, 16, 21, 26, 31, 36};
static const int8_t track2[8] = {2, 7, 12, 17, 22, 27, 32, 37};
static const int8_t track3[16] = {
    3, 4, 8, 9, 13, 14, 18, 19,
    23, 24, 28, 29, 33, 34, 38, 39,
};

typedef struct uint128_pair {
    uint64_t hi;
    uint64_t lo;
} uint128_pair;

static int16_t saturate16_from_i64(int64_t x) {
    if (x > (int64_t)G729_MAX16) {
        return G729_MAX16;
    }
    if (x < (int64_t)G729_MIN16) {
        return G729_MIN16;
    }
    return (int16_t)x;
}

static int32_t saturate32_from_i64(int64_t x) {
    if (x > (int64_t)G729_MAX32) {
        return G729_MAX32;
    }
    if (x < (int64_t)G729_MIN32) {
        return G729_MIN32;
    }
    return (int32_t)x;
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

static uint128_pair mul64_u128(uint64_t a, uint64_t b) {
    uint64_t a0 = a & 0xffffffffu;
    uint64_t a1 = a >> 32;
    uint64_t b0 = b & 0xffffffffu;
    uint64_t b1 = b >> 32;
    uint64_t p0 = a0 * b0;
    uint64_t p1 = a0 * b1;
    uint64_t p2 = a1 * b0;
    uint64_t p3 = a1 * b1;
    uint64_t middle = (p0 >> 32) + (p1 & 0xffffffffu) +
                      (p2 & 0xffffffffu);
    uint128_pair out;
    out.lo = (p0 & 0xffffffffu) | (middle << 32);
    out.hi = p3 + (p1 >> 32) + (p2 >> 32) + (middle >> 32);
    return out;
}

static uint128_pair saturated_u128(void) {
    uint128_pair out;
    out.hi = UINT64_MAX;
    out.lo = UINT64_MAX;
    return out;
}

static uint128_pair mul128_by64_saturating(uint128_pair x, uint64_t y) {
    uint128_pair lo_prod = mul64_u128(x.lo, y);
    uint128_pair hi_prod = mul64_u128(x.hi, y);
    uint64_t hi;
    if (hi_prod.hi != 0u) {
        return saturated_u128();
    }
    hi = lo_prod.hi + hi_prod.lo;
    if (hi < lo_prod.hi) {
        return saturated_u128();
    }
    {
        uint128_pair out;
        out.hi = hi;
        out.lo = lo_prod.lo;
        return out;
    }
}

static int cmp128(uint128_pair a, uint128_pair b) {
    if (a.hi > b.hi) {
        return 1;
    }
    if (a.hi < b.hi) {
        return -1;
    }
    if (a.lo > b.lo) {
        return 1;
    }
    if (a.lo < b.lo) {
        return -1;
    }
    return 0;
}

static uint64_t abs_i64_to_u64(int64_t x) {
    if (x >= 0) {
        return (uint64_t)x;
    }
    return (uint64_t)(-(x + 1)) + 1u;
}

static int ratio_greater(int64_t c1, int64_t e1, int64_t c2, int64_t e2) {
    uint64_t ac1 = abs_i64_to_u64(c1);
    uint64_t ac2 = abs_i64_to_u64(c2);
    uint128_pair left = mul128_by64_saturating(mul64_u128(ac1, ac1),
                                               (uint64_t)e2);
    uint128_pair right = mul128_by64_saturating(mul64_u128(ac2, ac2),
                                                (uint64_t)e1);
    return cmp128(left, right) > 0;
}

static int64_t square_saturating_i64(int64_t x) {
    uint64_t ax = abs_i64_to_u64(x);
    if (ax > 3037000499ULL) {
        return INT64_MAX;
    }
    return (int64_t)(ax * ax);
}

void g729_fcb_adjusted_target(const int16_t x[G729_SUBFRAME_SAMPLES],
                              const int16_t y[G729_SUBFRAME_SAMPLES],
                              int16_t gp_q14,
                              int16_t x_prime[G729_SUBFRAME_SAMPLES]) {
    int n;
    if (x == NULL || y == NULL || x_prime == NULL) {
        return;
    }
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int64_t prod = (int64_t)gp_q14 * (int64_t)y[n];
        x_prime[n] =
            saturate16_from_i64((int64_t)x[n] - round_shift64(prod, 14));
    }
}

void g729_fcb_correlation_d(const int16_t x_prime[G729_SUBFRAME_SAMPLES],
                            const int16_t h[G729_SUBFRAME_SAMPLES],
                            int32_t d[G729_SUBFRAME_SAMPLES]) {
    int n;
    if (x_prime == NULL || h == NULL || d == NULL) {
        return;
    }
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int i;
        int64_t acc = 0;
        for (i = n; i < G729_SUBFRAME_SAMPLES; ++i) {
            acc += (int64_t)x_prime[i] * (int64_t)h[i - n];
        }
        d[n] = saturate32_from_i64(acc);
    }
}

void g729_fcb_signs_from_d(const int32_t d[G729_SUBFRAME_SAMPLES],
                           int16_t signs[G729_SUBFRAME_SAMPLES],
                           int32_t d_abs[G729_SUBFRAME_SAMPLES]) {
    int n;
    if (d == NULL || signs == NULL || d_abs == NULL) {
        return;
    }
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        if (d[n] >= 0) {
            signs[n] = 1;
            d_abs[n] = d[n];
        } else {
            signs[n] = -1;
            d_abs[n] = d[n] == G729_MIN32 ? G729_MAX32 : -d[n];
        }
    }
}

void g729_fcb_phi_prime(
    const int16_t h[G729_SUBFRAME_SAMPLES],
    const int16_t signs[G729_SUBFRAME_SAMPLES],
    int32_t phi[G729_SUBFRAME_SAMPLES][G729_SUBFRAME_SAMPLES]) {
    int i;
    if (h == NULL || signs == NULL || phi == NULL) {
        return;
    }
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        int n;
        int j;
        int64_t diag = 0;
        for (n = i; n < G729_SUBFRAME_SAMPLES; ++n) {
            int64_t t = h[n - i];
            diag += t * t;
        }
        phi[i][i] = saturate32_from_i64(diag >> 1);
        for (j = i + 1; j < G729_SUBFRAME_SAMPLES; ++j) {
            int64_t sum = 0;
            int64_t s = (int64_t)signs[i] * (int64_t)signs[j];
            int32_t v;
            for (n = j; n < G729_SUBFRAME_SAMPLES; ++n) {
                sum += (int64_t)h[n - i] * (int64_t)h[n - j];
            }
            v = saturate32_from_i64(sum * s);
            phi[i][j] = v;
            phi[j][i] = v;
        }
    }
}

void g729_fcb_search_depth_first(
    const int32_t d_abs[G729_SUBFRAME_SAMPLES],
    int32_t phi[G729_SUBFRAME_SAMPLES][G729_SUBFRAME_SAMPLES],
    int8_t positions[4],
    int64_t sum_out[2]) {
    int t0;
    int8_t best_pos[4] = {0, 1, 2, 3};
    int64_t best_c = 0;
    int64_t best_e = 0;
    int found = 0;

    if (d_abs == NULL || phi == NULL || positions == NULL || sum_out == NULL) {
        return;
    }

    for (t0 = 0; t0 < 8; ++t0) {
        int8_t m0 = track0[t0];
        int64_t d0 = d_abs[(int)m0];
        int64_t e0 = phi[(int)m0][(int)m0];
        int t1;
        for (t1 = 0; t1 < 8; ++t1) {
            int8_t m1 = track1[t1];
            int64_t d01 = d0 + (int64_t)d_abs[(int)m1];
            int64_t e01 = e0 + (int64_t)phi[(int)m1][(int)m1] +
                          (int64_t)phi[(int)m0][(int)m1];
            int t2;
            for (t2 = 0; t2 < 8; ++t2) {
                int8_t m2 = track2[t2];
                int64_t d012 = d01 + (int64_t)d_abs[(int)m2];
                int64_t e012 = e01 + (int64_t)phi[(int)m2][(int)m2] +
                               (int64_t)phi[(int)m0][(int)m2] +
                               (int64_t)phi[(int)m1][(int)m2];
                int t3;
                for (t3 = 0; t3 < 16; ++t3) {
                    int8_t m3 = track3[t3];
                    int64_t c = d012 + (int64_t)d_abs[(int)m3];
                    int64_t e = e012 + (int64_t)phi[(int)m3][(int)m3] +
                                (int64_t)phi[(int)m0][(int)m3] +
                                (int64_t)phi[(int)m1][(int)m3] +
                                (int64_t)phi[(int)m2][(int)m3];
                    if (e <= 0) {
                        continue;
                    }
                    if (!found || ratio_greater(c, e, best_c, best_e)) {
                        found = 1;
                        best_c = c;
                        best_e = e;
                        best_pos[0] = m0;
                        best_pos[1] = m1;
                        best_pos[2] = m2;
                        best_pos[3] = m3;
                    }
                }
            }
        }
    }

    positions[0] = best_pos[0];
    positions[1] = best_pos[1];
    positions[2] = best_pos[2];
    positions[3] = best_pos[3];
    sum_out[0] = found ? square_saturating_i64(best_c) : 0;
    sum_out[1] = found ? best_e : 0;
}

int g729_fcb_search_depth_first_threshold_scan(
    const int32_t d_abs[G729_SUBFRAME_SAMPLES],
    int32_t phi[G729_SUBFRAME_SAMPLES][G729_SUBFRAME_SAMPLES],
    int8_t positions[4],
    int64_t sum_out[2],
    int limit) {
    int t0;
    int entered = 0;
    int64_t sum_c = 0;
    int64_t max_c = 0;
    int64_t first3_count = 0;
    int64_t threshold;
    int8_t best_pos[4] = {0, 1, 2, 3};
    int64_t best_c = 0;
    int64_t best_e = 0;
    int found = 0;

    if (d_abs == NULL || phi == NULL || positions == NULL || sum_out == NULL) {
        return 0;
    }
    if (limit <= 0) {
        g729_fcb_search_depth_first(d_abs, phi, positions, sum_out);
        return 0;
    }

    for (t0 = 0; t0 < 8; ++t0) {
        int8_t m0 = track0[t0];
        int64_t d0 = d_abs[(int)m0];
        int t1;
        for (t1 = 0; t1 < 8; ++t1) {
            int8_t m1 = track1[t1];
            int64_t d01 = d0 + (int64_t)d_abs[(int)m1];
            int t2;
            for (t2 = 0; t2 < 8; ++t2) {
                int8_t m2 = track2[t2];
                int64_t c = d01 + (int64_t)d_abs[(int)m2];
                sum_c += c;
                ++first3_count;
                if (c > max_c) {
                    max_c = c;
                }
            }
        }
    }
    if (first3_count == 0) {
        g729_fcb_search_depth_first(d_abs, phi, positions, sum_out);
        return 0;
    }
    threshold = (sum_c / first3_count) +
                (4 * (max_c - (sum_c / first3_count))) / 10;

    for (t0 = 0; t0 < 8; ++t0) {
        int8_t m0 = track0[t0];
        int64_t d0 = d_abs[(int)m0];
        int64_t e0 = phi[(int)m0][(int)m0];
        int t1;
        for (t1 = 0; t1 < 8; ++t1) {
            int8_t m1 = track1[t1];
            int64_t d01 = d0 + (int64_t)d_abs[(int)m1];
            int64_t e01 = e0 + (int64_t)phi[(int)m1][(int)m1] +
                          (int64_t)phi[(int)m0][(int)m1];
            int t2;
            for (t2 = 0; t2 < 8; ++t2) {
                int8_t m2 = track2[t2];
                int64_t c3 = d01 + (int64_t)d_abs[(int)m2];
                int64_t e3;
                int t3;
                if (c3 <= threshold) {
                    continue;
                }
                if (entered >= limit) {
                    goto scan_done;
                }
                ++entered;
                e3 = e01 + (int64_t)phi[(int)m2][(int)m2] +
                     (int64_t)phi[(int)m0][(int)m2] +
                     (int64_t)phi[(int)m1][(int)m2];
                for (t3 = 0; t3 < 16; ++t3) {
                    int8_t m3 = track3[t3];
                    int64_t c = c3 + (int64_t)d_abs[(int)m3];
                    int64_t e = e3 + (int64_t)phi[(int)m3][(int)m3] +
                                (int64_t)phi[(int)m0][(int)m3] +
                                (int64_t)phi[(int)m1][(int)m3] +
                                (int64_t)phi[(int)m2][(int)m3];
                    if (e <= 0) {
                        continue;
                    }
                    if (!found || ratio_greater(c, e, best_c, best_e)) {
                        found = 1;
                        best_c = c;
                        best_e = e;
                        best_pos[0] = m0;
                        best_pos[1] = m1;
                        best_pos[2] = m2;
                        best_pos[3] = m3;
                    }
                }
            }
        }
    }

scan_done:
    if (!found) {
        g729_fcb_search_depth_first(d_abs, phi, positions, sum_out);
        return entered;
    }
    positions[0] = best_pos[0];
    positions[1] = best_pos[1];
    positions[2] = best_pos[2];
    positions[3] = best_pos[3];
    sum_out[0] = square_saturating_i64(best_c);
    sum_out[1] = best_e;
    return entered;
}

void g729_fcb_build_sparse_code(
    const int8_t positions[4],
    const int16_t signs[G729_SUBFRAME_SAMPLES],
    int16_t code[G729_SUBFRAME_SAMPLES]) {
    int i;
    if (positions == NULL || signs == NULL || code == NULL) {
        return;
    }
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        code[i] = 0;
    }
    for (i = 0; i < 4; ++i) {
        int p = positions[i];
        if (signs[p] > 0) {
            code[p] = G729_FCB_PULSE_AMPLITUDE;
        } else {
            code[p] = G729_FCB_NEGATIVE_PULSE_AMPLITUDE;
        }
    }
}

void g729_fcb_build_code(const int8_t positions[4],
                         const int16_t signs[G729_SUBFRAME_SAMPLES],
                         int16_t int_lag,
                         int16_t prev_gp_q14,
                         int16_t code[G729_SUBFRAME_SAMPLES]) {
    if (positions == NULL || signs == NULL || code == NULL) {
        return;
    }
    g729_fcb_build_sparse_code(positions, signs, code);
    g729_fcb_apply_pitch_enhancement(
        code, int_lag, g729_fcb_clamp_pitch_gain_for_enhancement(prev_gp_q14));
}

void g729_fcb_filter_code(const int16_t code[G729_SUBFRAME_SAMPLES],
                          const int16_t h[G729_SUBFRAME_SAMPLES],
                          int16_t z[G729_SUBFRAME_SAMPLES]) {
    int n;
    if (code == NULL || h == NULL || z == NULL) {
        return;
    }
    for (n = 0; n < G729_SUBFRAME_SAMPLES; ++n) {
        int i;
        int64_t acc = 0;
        for (i = 0; i <= n; ++i) {
            acc += (int64_t)code[i] * (int64_t)h[n - i];
        }
        z[n] = saturate16_from_i64(round_shift64(acc, 13));
    }
}

uint8_t g729_fcb_pack_s(const int8_t positions[4],
                        const int16_t signs[G729_SUBFRAME_SAMPLES]) {
    int i;
    uint8_t s = 0;
    if (positions == NULL || signs == NULL) {
        return 0;
    }
    for (i = 0; i < 4; ++i) {
        if (signs[(int)positions[i]] > 0) {
            s = (uint8_t)(s | (uint8_t)(1u << (unsigned)i));
        }
    }
    return s;
}

uint16_t g729_fcb_pack_c(const int8_t positions[4]) {
    uint16_t i0;
    uint16_t i1;
    uint16_t i2;
    uint16_t i3;
    uint16_t jx = 0;
    uint16_t p3;
    if (positions == NULL) {
        return 0;
    }
    i0 = (uint16_t)positions[0] / 5u;
    i1 = ((uint16_t)positions[1] - 1u) / 5u;
    i2 = ((uint16_t)positions[2] - 2u) / 5u;
    p3 = (uint16_t)positions[3];
    if ((p3 % 5u) == 4u) {
        jx = 1u;
    }
    i3 = (p3 - 3u - jx) / 5u;
    return (uint16_t)(i0 | (i1 << 3) | (i2 << 6) | (jx << 9) |
                      (i3 << 10));
}
