#include "g729_lpc.h"

#include <stdint.h>
#include <string.h>

#include "g729_fixed.h"

static const int16_t lpc_analysis_window[G729_LPC_WINDOW_SAMPLES] = {
    2621, 2623, 2629, 2638, 2651, 2668, 2689, 2713,
    2741, 2773, 2808, 2847, 2890, 2936, 2986, 3040,
    3097, 3158, 3223, 3291, 3363, 3438, 3517, 3599,
    3685, 3775, 3867, 3963, 4063, 4166, 4272, 4382,
    4495, 4611, 4731, 4854, 4979, 5108, 5241, 5376,
    5514, 5655, 5800, 5947, 6097, 6250, 6406, 6565,
    6726, 6890, 7057, 7227, 7399, 7573, 7751, 7930,
    8112, 8297, 8483, 8672, 8864, 9057, 9253, 9450,
    9650, 9852, 10056, 10261, 10468, 10678, 10889, 11101,
    11316, 11531, 11749, 11968, 12188, 12409, 12632, 12857,
    13082, 13309, 13536, 13765, 13994, 14225, 14456, 14689,
    14922, 15155, 15390, 15624, 15860, 16096, 16332, 16568,
    16805, 17042, 17279, 17517, 17754, 17991, 18229, 18466,
    18703, 18939, 19176, 19412, 19647, 19883, 20117, 20351,
    20584, 20817, 21049, 21280, 21510, 21739, 21967, 22194,
    22420, 22645, 22869, 23091, 23312, 23531, 23750, 23966,
    24181, 24395, 24606, 24817, 25025, 25231, 25436, 25639,
    25839, 26038, 26235, 26429, 26622, 26812, 27000, 27185,
    27368, 27549, 27728, 27904, 28077, 28248, 28416, 28581,
    28744, 28904, 29062, 29216, 29368, 29516, 29662, 29805,
    29945, 30082, 30215, 30346, 30473, 30598, 30719, 30837,
    30951, 31063, 31171, 31275, 31377, 31474, 31569, 31660,
    31748, 31832, 31912, 31989, 32063, 32133, 32199, 32262,
    32321, 32377, 32429, 32477, 32522, 32562, 32600, 32633,
    32663, 32689, 32712, 32730, 32745, 32756, 32764, 32767,
    32767, 32742, 32666, 32538, 32359, 32130, 31851, 31522,
    31144, 30717, 30243, 29721, 29152, 28538, 27880, 27178,
    26434, 25648, 24822, 23958, 23056, 22118, 21145, 20140,
    19103, 18036, 16941, 15820, 14674, 13505, 12315, 11106,
    9879, 8637, 7382, 6115, 4838, 3554, 2264, 971,
};

static const int16_t lag_window[G729_LPC_ORDER] = {
    32732, 32623, 32442, 32191, 31871,
    31484, 31033, 30520, 29950, 29324,
};

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

static int16_t saturate16_i64(int64_t x) {
    if (x > 32767) {
        return 32767;
    }
    if (x < -32768) {
        return -32768;
    }
    return (int16_t)x;
}

static int64_t q24_to_q12_round(int64_t v) {
    if (v >= 0) {
        return (v + (1LL << 11)) >> 12;
    }
    return -(((-v) + (1LL << 11)) >> 12);
}

void g729_lpc_window_speech(const int16_t speech[G729_LPC_WINDOW_SAMPLES],
                            int16_t windowed[G729_LPC_WINDOW_SAMPLES]) {
    int n;
    if (speech == NULL || windowed == NULL) {
        return;
    }
    for (n = 0; n < G729_LPC_WINDOW_SAMPLES; ++n) {
        windowed[n] = g729_mult(speech[n], lpc_analysis_window[n]);
    }
}

int g729_lpc_autocorrelate(const int16_t windowed[G729_LPC_WINDOW_SAMPLES],
                           int32_t r[G729_LPC_ORDER + 1]) {
    int n;
    int k;
    int scale = 0;
    int64_t sum_sq = 0;
    int32_t shifted[G729_LPC_WINDOW_SAMPLES];

    if (windowed == NULL || r == NULL) {
        return 0;
    }

    for (n = 0; n < G729_LPC_WINDOW_SAMPLES; ++n) {
        int64_t v = windowed[n];
        sum_sq += v * v;
    }
    while (sum_sq > (int64_t)G729_MAX32) {
        sum_sq >>= 2;
        ++scale;
    }

    if (scale == 0) {
        for (k = 0; k <= G729_LPC_ORDER; ++k) {
            int64_t acc = 0;
            for (n = k; n < G729_LPC_WINDOW_SAMPLES; ++n) {
                acc += (int64_t)windowed[n] * (int64_t)windowed[n - k];
            }
            r[k] = (int32_t)acc;
        }
        return 0;
    }

    for (n = 0; n < G729_LPC_WINDOW_SAMPLES; ++n) {
        shifted[n] = g729_l_shr(windowed[n], (int16_t)scale);
    }
    for (k = 0; k <= G729_LPC_ORDER; ++k) {
        int64_t acc = 0;
        for (n = k; n < G729_LPC_WINDOW_SAMPLES; ++n) {
            acc += (int64_t)shifted[n] * (int64_t)shifted[n - k];
        }
        r[k] = (int32_t)acc;
    }
    return scale;
}

void g729_lpc_apply_lag_window(int32_t r[G729_LPC_ORDER + 1]) {
    int k;
    if (r == NULL) {
        return;
    }
    r[0] = g729_l_add(r[0], g729_l_shr(r[0], 13));
    for (k = 1; k <= G729_LPC_ORDER; ++k) {
        r[k] = (int32_t)shr64_floor((int64_t)r[k] * (int64_t)lag_window[k - 1],
                                    15);
    }
}

void g729_lpc_levinson_durbin(const int32_t r[G729_LPC_ORDER + 1],
                              int16_t a[G729_LPC_ORDER + 1]) {
    int i;
    int j;
    int64_t a_work[G729_LPC_ORDER + 1] = {0};
    int64_t a_prev[G729_LPC_ORDER + 1] = {0};
    int64_t e;

    if (r == NULL || a == NULL) {
        return;
    }

    a_work[0] = 1LL << 24;
    e = r[0];

    for (i = 1; i <= G729_LPC_ORDER; ++i) {
        int64_t sum = q24_to_q12_round(a_work[0]) * (int64_t)r[i];
        int32_t k_q15 = 0;
        int64_t k_sq;
        int64_t one_minus_k_sq;

        for (j = 1; j < i; ++j) {
            sum += q24_to_q12_round(a_work[j]) * (int64_t)r[i - j];
        }
        if (e > 0) {
            int64_t num = -(sum * 8);
            int64_t q = num / e;
            if (q > 32767) {
                k_q15 = 32767;
            } else if (q < -32768) {
                k_q15 = -32768;
            } else {
                k_q15 = (int32_t)q;
            }
        }

        memcpy(a_prev, a_work, (size_t)i * sizeof(a_work[0]));
        for (j = 1; j < i; ++j) {
            a_work[j] =
                a_prev[j] +
                shr64_floor((int64_t)k_q15 * a_prev[i - j], 15);
        }
        a_work[i] = (int64_t)k_q15 * (1LL << 9);

        k_sq = (int64_t)k_q15 * (int64_t)k_q15;
        if (k_sq > (1LL << 30)) {
            k_sq = 1LL << 30;
        }
        one_minus_k_sq = (1LL << 30) - k_sq;
        e = shr64_floor(e * one_minus_k_sq, 30);
    }

    a[0] = 4096;
    for (j = 1; j <= G729_LPC_ORDER; ++j) {
        a[j] = saturate16_i64(q24_to_q12_round(a_work[j]));
    }
}

void g729_lpc_analyze(const int16_t speech[G729_LPC_WINDOW_SAMPLES],
                      int16_t a[G729_LPC_ORDER + 1]) {
    int16_t windowed[G729_LPC_WINDOW_SAMPLES];
    int32_t r[G729_LPC_ORDER + 1];
    if (speech == NULL || a == NULL) {
        return;
    }
    g729_lpc_window_speech(speech, windowed);
    (void)g729_lpc_autocorrelate(windowed, r);
    g729_lpc_apply_lag_window(r);
    g729_lpc_levinson_durbin(r, a);
}
