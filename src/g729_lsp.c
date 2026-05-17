#include "g729_lsp.h"

#include <stddef.h>
#include <string.h>

#include "g729_fixed.h"
#include "g729_lsp_tables.h"

enum {
    LSF_MIN_EDGE = 41,
    LSF_MIN_GAP = 321,
    LSF_MAX_EDGE = 25682,
    LSF_REARR_J1 = 10,
    LSF_REARR_J2 = 5,
    LSP_PI_Q13 = 25736,
    LSP_NUM_CELLS = 64,
    LSP_FRAC_BITS = 8,
    LSP_LSF_TO_COS_SCALE_Q15 = 20861,
    LSP_Q13_PI04 = 1029,
    LSP_Q13_PI92 = 23676,
    LSP_Q13_ONE = 8192,
    LSP_Q11_ONE = 2048,
    LSP_Q11_ONE_TWO = 2458
};

static const int16_t initial_prev_lsp[10] = {
    30000, 26000, 21000, 15000, 8000,
    0, -8000, -15000, -21000, -26000,
};

static const int16_t initial_past_residual[10] = {
    2339, 4679, 7018, 9358, 11698,
    14037, 16377, 18717, 21056, 23396,
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

static int16_t sat16_from_i64(int64_t x) {
    if (x > 32767) {
        return 32767;
    }
    if (x < -32768) {
        return -32768;
    }
    return (int16_t)x;
}

static int32_t wrap32_from_i64(int64_t x) {
    uint32_t u = (uint32_t)((uint64_t)x & 0xffffffffu);
    if (u <= 2147483647u) {
        return (int32_t)u;
    }
    return (int32_t)((int64_t)u - 4294967296LL);
}

static int32_t abs_i32(int32_t x) {
    if (x < 0) {
        return -x;
    }
    return x;
}

static void init_decoder(g729_lsp_decoder *dec) {
    int k;
    if (dec->initialized) {
        return;
    }
    for (k = 0; k < 4; ++k) {
        memcpy(dec->past_residuals[k], initial_past_residual,
               sizeof(initial_past_residual));
    }
    memcpy(dec->prev_lsf, initial_past_residual, sizeof(initial_past_residual));
}

void g729_lsp_decoder_reset(g729_lsp_decoder *dec) {
    if (dec == NULL) {
        return;
    }
    memset(dec, 0, sizeof(*dec));
}

void g729_lsp_init_freq_prev(int16_t freq_prev[4][10]) {
    int k;
    if (freq_prev == NULL) {
        return;
    }
    for (k = 0; k < 4; ++k) {
        memcpy(freq_prev[k], initial_past_residual, sizeof(initial_past_residual));
    }
}

static void combine_residual(uint8_t l1, uint8_t l2, uint8_t l3,
                             int16_t out[10]) {
    int i;
    l1 &= 0x7Fu;
    l2 &= 0x1Fu;
    l3 &= 0x1Fu;
    for (i = 0; i < 5; ++i) {
        out[i] = g729_add(g729_lsp_codebook_l1[l1][i],
                          g729_lsp_codebook_l2[l2][i]);
    }
    for (i = 5; i < 10; ++i) {
        out[i] = g729_add(g729_lsp_codebook_l1[l1][i],
                          g729_lsp_codebook_l3[l3][i - 5]);
    }
}

static void insertion_sort_10(int16_t a[10]) {
    int i;
    for (i = 1; i < 10; ++i) {
        int16_t v = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > v) {
            a[j + 1] = a[j];
            --j;
        }
        a[j + 1] = v;
    }
}

static void rearrange_adjacent(int16_t lsf[10], int16_t j_gap) {
    int i;
    for (i = 1; i < 10; ++i) {
        int32_t gap = (int32_t)lsf[i] - (int32_t)lsf[i - 1];
        if (gap < j_gap) {
            int16_t correction = (int16_t)((j_gap - gap) / 2);
            lsf[i - 1] = g729_sub(lsf[i - 1], correction);
            lsf[i] = g729_add(lsf[i], correction);
        }
    }
}

static void enforce_lsf_stability(int16_t lsf[10]) {
    int i;
    insertion_sort_10(lsf);

    if (lsf[0] < LSF_MIN_EDGE) {
        lsf[0] = LSF_MIN_EDGE;
    }
    for (i = 1; i < 10; ++i) {
        int16_t min_next = g729_add(lsf[i - 1], LSF_MIN_GAP);
        if (lsf[i] < min_next) {
            lsf[i] = min_next;
        }
    }
    if (lsf[9] > LSF_MAX_EDGE) {
        lsf[9] = LSF_MAX_EDGE;
        for (i = 9; i > 0; --i) {
            int16_t max_prev = g729_sub(lsf[i], LSF_MIN_GAP);
            if (lsf[i - 1] > max_prev) {
                lsf[i - 1] = max_prev;
            }
        }
    }
}

static int16_t apply_predictor_coordinate(uint8_t selector,
                                          int coord,
                                          int16_t residual,
                                          int16_t past[4][10]) {
    int32_t acc = 0;
    selector &= 1u;
    acc = g729_l_mac(acc, g729_lsp_ma_predictor_inv_sum[selector][coord],
                     residual);
    acc = g729_l_mac(acc, g729_lsp_ma_predictors[selector][0][coord],
                     past[0][coord]);
    acc = g729_l_mac(acc, g729_lsp_ma_predictors[selector][1][coord],
                     past[1][coord]);
    acc = g729_l_mac(acc, g729_lsp_ma_predictors[selector][2][coord],
                     past[2][coord]);
    acc = g729_l_mac(acc, g729_lsp_ma_predictors[selector][3][coord],
                     past[3][coord]);
    return g729_extract_h(acc);
}

static void apply_predictor(g729_lsp_decoder *dec,
                            uint8_t selector,
                            const int16_t residual[10],
                            int16_t out[10]) {
    int i;
    for (i = 0; i < 10; ++i) {
        out[i] = apply_predictor_coordinate(selector, i, residual[i],
                                            dec->past_residuals);
    }
    memcpy(dec->past_residuals[3], dec->past_residuals[2],
           sizeof(dec->past_residuals[3]));
    memcpy(dec->past_residuals[2], dec->past_residuals[1],
           sizeof(dec->past_residuals[2]));
    memcpy(dec->past_residuals[1], dec->past_residuals[0],
           sizeof(dec->past_residuals[1]));
    memcpy(dec->past_residuals[0], residual, sizeof(dec->past_residuals[0]));
}

int16_t g729_lsp_lsf_to_lsp(int16_t omega) {
    int32_t w = omega;
    int32_t q;
    int32_t max_q;
    int32_t idx;
    int32_t frac;
    int32_t interp;
    if (w < 0) {
        w = 0;
    }

    q = (w * LSP_LSF_TO_COS_SCALE_Q15) / 32768;
    max_q = (LSP_NUM_CELLS << LSP_FRAC_BITS) - 1;
    if (q > max_q) {
        q = max_q;
    }

    idx = q >> LSP_FRAC_BITS;
    frac = q & ((1 << LSP_FRAC_BITS) - 1);
    interp = (int32_t)g729_lsp_cos[idx] +
             (int32_t)g729_l_shr((int32_t)g729_lsp_cos_slope[idx] * frac,
                                 12);

    return g729_saturate(interp);
}

int16_t g729_lsp_lsp_to_lsf(int16_t q) {
    int32_t qi = q;
    int32_t lo;
    int32_t hi;
    int32_t best_omega;
    int32_t best_diff;
    int32_t start;
    int32_t end;
    int32_t omega;

    if (qi >= (int32_t)g729_lsp_cos[0]) {
        return 0;
    }
    if (qi <= (int32_t)g729_lsp_cos[64]) {
        return (int16_t)LSP_PI_Q13;
    }

    lo = 0;
    hi = LSP_PI_Q13;
    while (hi - lo > 1) {
        int32_t mid = (lo + hi) >> 1;
        if ((int32_t)g729_lsp_lsf_to_lsp((int16_t)mid) > qi) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    best_omega = lo;
    best_diff = abs_i32(qi - (int32_t)g729_lsp_lsf_to_lsp((int16_t)lo));
    start = lo - 128;
    if (start < 0) {
        start = 0;
    }
    end = hi + 128;
    if (end > LSP_PI_Q13) {
        end = LSP_PI_Q13;
    }
    for (omega = start; omega <= end; ++omega) {
        int32_t diff =
            abs_i32(qi - (int32_t)g729_lsp_lsf_to_lsp((int16_t)omega));
        int tie_break = (qi >= 0 && omega > best_omega) ||
                        (qi < 0 && omega < best_omega);
        if (diff < best_diff || (diff == best_diff && tie_break)) {
            best_diff = diff;
            best_omega = omega;
        }
    }
    return (int16_t)best_omega;
}

void g729_lsp_lsp_vector_to_lsf(const int16_t lsp[10], int16_t lsf[10]) {
    int i;
    if (lsp == NULL || lsf == NULL) {
        return;
    }
    for (i = 0; i < 10; ++i) {
        lsf[i] = g729_lsp_lsp_to_lsf(lsp[i]);
    }
}

static void interpolate_lsp(const int16_t prev[10],
                            const int16_t curr[10],
                            int16_t sf1[10],
                            int16_t sf2[10]) {
    int i;
    for (i = 0; i < 10; ++i) {
        sf1[i] = (int16_t)((int32_t)g729_shr(prev[i], 1) +
                           (int32_t)g729_shr(curr[i], 1));
        sf2[i] = curr[i];
    }
}

static int16_t lp_lsp_grid_value(int k) {
    int32_t omega = ((int32_t)k * (int32_t)LSP_PI_Q13) / 59;
    if (omega > LSP_PI_Q13) {
        omega = LSP_PI_Q13;
    }
    if (k == 0) {
        return 32767;
    }
    if (k == 59) {
        return -32768;
    }
    return g729_lsp_lsf_to_lsp((int16_t)omega);
}

static void compute_f1_f2(const int16_t a[11], int32_t f1[6], int32_t f2[6]) {
    int i;
    f1[0] = 1 << 24;
    f2[0] = 1 << 24;
    for (i = 0; i < 5; ++i) {
        int32_t ai1 = (int32_t)a[i + 1] * 4096;
        int32_t a10i = (int32_t)a[10 - i] * 4096;
        f1[i + 1] = wrap32_from_i64((int64_t)ai1 + (int64_t)a10i - f1[i]);
        f2[i + 1] = wrap32_from_i64((int64_t)ai1 - (int64_t)a10i + f2[i]);
    }
}

static int32_t chebyshev_c(int16_t x, const int32_t f[6]) {
    int k;
    int32_t bk1 = 1 << 24;
    int32_t bk2 = 0;
    int64_t x64 = x;
    for (k = 4; k >= 1; --k) {
        int32_t two_xb =
            wrap32_from_i64(shr64_floor(2 * x64 * (int64_t)bk1, 15));
        int32_t bk = wrap32_from_i64((int64_t)two_xb - bk2 + f[5 - k]);
        bk2 = bk1;
        bk1 = bk;
    }
    {
        int32_t x_b1 = wrap32_from_i64(shr64_floor(x64 * (int64_t)bk1, 15));
        return wrap32_from_i64((int64_t)x_b1 - bk2 +
                               g729_l_shr(f[5], 1));
    }
}

static int signs_differ(int32_t a, int32_t b) {
    return (a < 0) != (b < 0);
}

static int16_t bisect_root(int16_t x_lo,
                           int16_t x_hi,
                           int32_t c_lo,
                           int32_t c_hi,
                           const int32_t f[6]) {
    int i;
    for (i = 0; i < 8; ++i) {
        int16_t mid = (int16_t)g729_l_shr((int32_t)x_lo + (int32_t)x_hi, 1);
        int32_t c_mid = chebyshev_c(mid, f);
        if (signs_differ(c_lo, c_mid)) {
            x_hi = mid;
            c_hi = c_mid;
        } else {
            x_lo = mid;
            c_lo = c_mid;
        }
    }
    (void)c_hi;
    return (int16_t)g729_l_shr((int32_t)x_lo + (int32_t)x_hi, 1);
}

static int find_lsp_roots(const int32_t f1[6],
                          const int32_t f2[6],
                          int16_t q[10]) {
    int16_t roots_f1[5];
    int16_t roots_f2[5];
    int n_f1 = 0;
    int n_f2 = 0;
    int k;
    int16_t x_prev = lp_lsp_grid_value(0);
    int32_t c_prev1 = chebyshev_c(x_prev, f1);
    int32_t c_prev2 = chebyshev_c(x_prev, f2);

    for (k = 1; k < 60; ++k) {
        int16_t x = lp_lsp_grid_value(k);
        int32_t c1 = chebyshev_c(x, f1);
        int32_t c2 = chebyshev_c(x, f2);
        if (n_f1 < 5 && signs_differ(c_prev1, c1)) {
            roots_f1[n_f1] = bisect_root(x_prev, x, c_prev1, c1, f1);
            ++n_f1;
        }
        if (n_f2 < 5 && signs_differ(c_prev2, c2)) {
            roots_f2[n_f2] = bisect_root(x_prev, x, c_prev2, c2, f2);
            ++n_f2;
        }
        x_prev = x;
        c_prev1 = c1;
        c_prev2 = c2;
    }

    if (n_f1 < 5 || n_f2 < 5) {
        return -1;
    }
    for (k = 0; k < 5; ++k) {
        q[2 * k] = roots_f1[k];
        q[2 * k + 1] = roots_f2[k];
    }
    return 0;
}

int g729_lsp_lp_to_lsp(const int16_t a[11], int16_t lsp[10]) {
    int32_t f1[6];
    int32_t f2[6];
    if (a == NULL || lsp == NULL) {
        return -1;
    }
    compute_f1_f2(a, f1, f2);
    return find_lsp_roots(f1, f2, lsp);
}

static int64_t lsp_poly_product_q24(int64_t q, int64_t coeff) {
    return shr64_floor(q * coeff, 16) * 4;
}

static void build_lsp_poly_q24(const int16_t lsp[10], int offset, int64_t f[6]) {
    int step;
    int i;
    for (i = 0; i < 6; ++i) {
        f[i] = 0;
    }
    f[0] = (int64_t)1 << 24;
    f[1] = -lsp_poly_product_q24(lsp[offset], f[0]);

    for (step = 1; step < 5; ++step) {
        int64_t old[6];
        int64_t next[6] = {0, 0, 0, 0, 0, 0};
        int j;
        memcpy(old, f, sizeof(old));
        next[0] = old[0];
        next[1] = old[1] - lsp_poly_product_q24(lsp[2 * step + offset],
                                                old[0]);
        for (j = step; j >= 1; --j) {
            int64_t add = old[j - 1];
            if (j == step) {
                add *= 2;
            }
            next[j + 1] = old[j + 1] -
                          lsp_poly_product_q24(lsp[2 * step + offset],
                                               old[j]) +
                          add;
        }
        memcpy(f, next, sizeof(next));
    }
}

void g729_lsp_lsp_to_lp(const int16_t lsp[10], int16_t a[11]) {
    int64_t f1[6];
    int64_t f2[6];
    int i;
    if (lsp == NULL || a == NULL) {
        return;
    }

    build_lsp_poly_q24(lsp, 0, f1);
    build_lsp_poly_q24(lsp, 1, f2);

    for (i = 5; i >= 1; --i) {
        f1[i] += f1[i - 1];
        f2[i] -= f2[i - 1];
    }

    a[0] = 4096;
    for (i = 1; i <= 5; ++i) {
        a[i] = sat16_from_i64(shr64_floor(((f1[i] + f2[i]) * 8) +
                                              ((int64_t)1 << 15),
                                          16));
        a[11 - i] = sat16_from_i64(shr64_floor(((f1[i] - f2[i]) * 8) +
                                                   ((int64_t)1 << 15),
                                               16));
    }
}

static int16_t weight_from_arg(int16_t arg_q13) {
    int64_t a;
    int64_t sq_q26;
    int64_t d_q26;
    int64_t w_q11;
    if (arg_q13 > 0) {
        return LSP_Q11_ONE;
    }
    a = arg_q13;
    sq_q26 = a * a;
    d_q26 = 10 * sq_q26 + (1LL << 26);
    w_q11 = (1LL << 37) / d_q26;
    if (w_q11 > 32767) {
        return 32767;
    }
    return (int16_t)w_q11;
}

static int16_t boost12(int16_t w_q11) {
    int32_t v = ((int32_t)w_q11 * LSP_Q11_ONE_TWO) >> 11;
    return sat16_from_i64(v);
}

static void weights_lsf(const int16_t omega[10], int16_t weights[10]) {
    int i;
    weights[0] =
        weight_from_arg(g729_sub(g729_sub(omega[1], LSP_Q13_PI04),
                                 LSP_Q13_ONE));
    for (i = 1; i <= 8; ++i) {
        int16_t gap = g729_sub(omega[i + 1], omega[i - 1]);
        weights[i] = weight_from_arg(g729_sub(gap, LSP_Q13_ONE));
    }
    weights[9] =
        weight_from_arg(g729_sub(g729_sub(LSP_Q13_PI92, omega[8]),
                                 LSP_Q13_ONE));
    weights[4] = boost12(weights[4]);
    weights[5] = boost12(weights[5]);
}

static void apply_predictor_with_memory(uint8_t selector,
                                        int16_t mem[4][10],
                                        const int16_t residual[10],
                                        int16_t out[10]) {
    int i;
    selector &= 1u;
    for (i = 0; i < 10; ++i) {
        int32_t acc = 0;
        acc = g729_l_mac(acc, g729_lsp_ma_predictor_inv_sum[selector][i],
                         residual[i]);
        acc = g729_l_mac(acc, g729_lsp_ma_predictors[selector][0][i],
                         mem[0][i]);
        acc = g729_l_mac(acc, g729_lsp_ma_predictors[selector][1][i],
                         mem[1][i]);
        acc = g729_l_mac(acc, g729_lsp_ma_predictors[selector][2][i],
                         mem[2][i]);
        acc = g729_l_mac(acc, g729_lsp_ma_predictors[selector][3][i],
                         mem[3][i]);
        out[i] = g729_extract_h(acc);
    }
}

static void compute_target_lsf(uint8_t selector,
                               int16_t mem[4][10],
                               const int16_t omega[10],
                               int16_t target[10]) {
    int i;
    selector &= 1u;
    for (i = 0; i < 10; ++i) {
        int16_t comp = g729_lsp_ma_predictor_inv_sum[selector][i];
        int32_t acc = g729_l_deposit_h(omega[i]);
        int16_t n_q13;
        int32_t num;
        acc = g729_l_msu(acc, g729_lsp_ma_predictors[selector][0][i],
                         mem[0][i]);
        acc = g729_l_msu(acc, g729_lsp_ma_predictors[selector][1][i],
                         mem[1][i]);
        acc = g729_l_msu(acc, g729_lsp_ma_predictors[selector][2][i],
                         mem[2][i]);
        acc = g729_l_msu(acc, g729_lsp_ma_predictors[selector][3][i],
                         mem[3][i]);
        n_q13 = g729_extract_h(acc);
        num = (int32_t)n_q13 * 32768;
        target[i] = g729_saturate(num / (int32_t)comp);
    }
}

static void commit_predictor_memory(int16_t mem[4][10],
                                    const int16_t residual[10]) {
    memcpy(mem[3], mem[2], sizeof(mem[3]));
    memcpy(mem[2], mem[1], sizeof(mem[2]));
    memcpy(mem[1], mem[0], sizeof(mem[1]));
    memcpy(mem[0], residual, sizeof(mem[0]));
}

static int search_l1(const int16_t target[10]) {
    int row;
    int best_idx = 0;
    int64_t best_mse = -1;
    for (row = 0; row < 128; ++row) {
        int i;
        int64_t sum = 0;
        for (i = 0; i < 10; ++i) {
            int64_t diff = (int64_t)target[i] - g729_lsp_codebook_l1[row][i];
            sum += diff * diff;
        }
        if (best_mse < 0 || sum < best_mse) {
            best_mse = sum;
            best_idx = row;
        }
    }
    return best_idx;
}

static int search_l2(uint8_t l1,
                     uint8_t selector,
                     int16_t mem[4][10],
                     const int16_t omega[10],
                     const int16_t weights[10]) {
    int row;
    int best_idx = 0;
    int64_t best_mse = -1;
    int16_t residual[10] = {0};
    int16_t omega_hat[10];
    l1 &= 0x7fu;
    selector &= 1u;
    for (row = 0; row < 32; ++row) {
        int i;
        int64_t mse = 0;
        for (i = 0; i < 5; ++i) {
            residual[i] = g729_add(g729_lsp_codebook_l1[l1][i],
                                   g729_lsp_codebook_l2[row][i]);
        }
        apply_predictor_with_memory(selector, mem, residual, omega_hat);
        for (i = 1; i < 5; ++i) {
            int32_t gap = (int32_t)omega_hat[i] - (int32_t)omega_hat[i - 1];
            if (gap < LSF_REARR_J1) {
                int32_t sum = (int32_t)omega_hat[i] + (int32_t)omega_hat[i - 1];
                omega_hat[i - 1] = (int16_t)((sum - LSF_REARR_J1) / 2);
                omega_hat[i] = (int16_t)((sum + LSF_REARR_J1) / 2);
            }
        }
        for (i = 0; i < 5; ++i) {
            int64_t d = (int64_t)omega[i] - omega_hat[i];
            mse += (int64_t)weights[i] * d * d;
        }
        if (best_mse < 0 || mse < best_mse) {
            best_mse = mse;
            best_idx = row;
        }
    }
    return best_idx;
}

static int search_l3(uint8_t l1,
                     uint8_t l2,
                     uint8_t selector,
                     int16_t mem[4][10],
                     const int16_t omega[10],
                     const int16_t weights[10]) {
    int row;
    int best_idx = 0;
    int64_t best_mse = -1;
    int16_t residual[10] = {0};
    int16_t omega_hat[10];
    int i;
    l1 &= 0x7fu;
    l2 &= 0x1fu;
    selector &= 1u;
    for (i = 0; i < 5; ++i) {
        residual[i] = g729_add(g729_lsp_codebook_l1[l1][i],
                               g729_lsp_codebook_l2[l2][i]);
    }
    for (row = 0; row < 32; ++row) {
        int64_t mse = 0;
        for (i = 0; i < 5; ++i) {
            residual[5 + i] = g729_add(g729_lsp_codebook_l1[l1][5 + i],
                                       g729_lsp_codebook_l3[row][i]);
        }
        apply_predictor_with_memory(selector, mem, residual, omega_hat);
        rearrange_adjacent(omega_hat, LSF_REARR_J1);
        for (i = 5; i < 10; ++i) {
            int64_t d = (int64_t)omega[i] - omega_hat[i];
            mse += (int64_t)weights[i] * d * d;
        }
        if (best_mse < 0 || mse < best_mse) {
            best_mse = mse;
            best_idx = row;
        }
    }
    return best_idx;
}

static int64_t final_quantized_lsf_cost(uint8_t selector,
                                        uint8_t l1,
                                        uint8_t l2,
                                        uint8_t l3,
                                        int16_t mem[4][10],
                                        const int16_t omega[10],
                                        const int16_t weights[10],
                                        int16_t residual[10]) {
    int i;
    int16_t omega_hat[10];
    selector &= 1u;
    l1 &= 0x7fu;
    l2 &= 0x1fu;
    l3 &= 0x1fu;
    for (i = 0; i < 5; ++i) {
        residual[i] = g729_add(g729_lsp_codebook_l1[l1][i],
                               g729_lsp_codebook_l2[l2][i]);
        residual[5 + i] = g729_add(g729_lsp_codebook_l1[l1][5 + i],
                                   g729_lsp_codebook_l3[l3][i]);
    }
    rearrange_adjacent(residual, LSF_REARR_J1);
    rearrange_adjacent(residual, LSF_REARR_J2);
    apply_predictor_with_memory(selector, mem, residual, omega_hat);
    enforce_lsf_stability(omega_hat);
    {
        int64_t cost = 0;
        for (i = 0; i < 10; ++i) {
            int64_t d = (int64_t)omega[i] - omega_hat[i];
            cost += (int64_t)weights[i] * d * d;
        }
        return cost;
    }
}

g729_lsp_indices g729_lsp_quantize(const int16_t omega[10],
                                   int16_t freq_prev[4][10]) {
    int16_t weights[10];
    int16_t target[10];
    int16_t residual[10];
    int16_t best_residual[10] = {0};
    int64_t best_cost = -1;
    g729_lsp_indices best = {0, 0, 0, 0};
    uint8_t sel;

    if (omega == NULL || freq_prev == NULL) {
        return best;
    }
    weights_lsf(omega, weights);
    for (sel = 0; sel < 2; ++sel) {
        int l1;
        int l2;
        int l3;
        int64_t cost;
        compute_target_lsf(sel, freq_prev, omega, target);
        l1 = search_l1(target);
        l2 = search_l2((uint8_t)l1, sel, freq_prev, omega, weights);
        l3 = search_l3((uint8_t)l1, (uint8_t)l2, sel, freq_prev, omega, weights);
        cost = final_quantized_lsf_cost(sel, (uint8_t)l1, (uint8_t)l2,
                                        (uint8_t)l3, freq_prev, omega,
                                        weights, residual);
        if (best_cost < 0 || cost < best_cost) {
            best_cost = cost;
            best.l0 = sel;
            best.l1 = (uint8_t)l1;
            best.l2 = (uint8_t)l2;
            best.l3 = (uint8_t)l3;
            memcpy(best_residual, residual, sizeof(best_residual));
        }
    }
    commit_predictor_memory(freq_prev, best_residual);
    return best;
}

void g729_lsp_decode(g729_lsp_decoder *dec,
                     g729_lsp_indices idx,
                     int16_t sf1[11],
                     int16_t sf2[11]) {
    int16_t residual[10];
    int16_t lsf[10];
    int16_t lsp[10];
    int16_t lsp_sf1[10];
    int16_t lsp_sf2[10];
    int i;
    if (dec == NULL || sf1 == NULL || sf2 == NULL) {
        return;
    }

    init_decoder(dec);
    combine_residual(idx.l1, idx.l2, idx.l3, residual);
    rearrange_adjacent(residual, LSF_REARR_J1);
    rearrange_adjacent(residual, LSF_REARR_J2);
    apply_predictor(dec, idx.l0, residual, lsf);
    enforce_lsf_stability(lsf);

    for (i = 0; i < 10; ++i) {
        lsp[i] = g729_lsp_lsf_to_lsp(lsf[i]);
    }

    if (!dec->initialized) {
        memcpy(dec->prev_lsp, initial_prev_lsp, sizeof(initial_prev_lsp));
        dec->initialized = 1;
    }

    interpolate_lsp(dec->prev_lsp, lsp, lsp_sf1, lsp_sf2);
    g729_lsp_lsp_to_lp(lsp_sf1, sf1);
    g729_lsp_lsp_to_lp(lsp_sf2, sf2);

    memcpy(dec->prev_lsp, lsp, sizeof(dec->prev_lsp));
    memcpy(dec->prev_lsf, lsf, sizeof(dec->prev_lsf));
    dec->last_selector = (uint8_t)(idx.l0 & 1u);
}
