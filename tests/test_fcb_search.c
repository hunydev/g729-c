#include "g729_fcb_search.h"

#include <stdint.h>
#include <stdio.h>

#include "g729_fcb.h"
#include "fixtures/fcb_search_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int vec16_equal(const int16_t *a, const int16_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int vec32_equal(const int32_t *a, const int32_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int vec8_equal(const int8_t *a, const int8_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int vec64_equal(const int64_t *a, const int64_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int phi_equal(const int32_t *a, const int32_t *b) {
    int i;
    for (i = 0; i < 1600; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    int i;

    {
        int8_t pos[4] = {0, 1, 2, 3};
        int16_t signs[40] = {0};
        int16_t code[40];
        signs[0] = 1;
        signs[1] = -1;
        signs[2] = 1;
        signs[3] = -1;
        g729_fcb_build_sparse_code(pos, signs, code);
        CHECK(code[0] == G729_FCB_PULSE_AMPLITUDE);
        CHECK(code[1] == G729_FCB_NEGATIVE_PULSE_AMPLITUDE);
        CHECK(g729_fcb_pack_s(pos, signs) == 0x05);
        CHECK(g729_fcb_pack_c(pos) == 0);
        pos[3] = 4;
        CHECK(g729_fcb_pack_c(pos) == 512);
    }

    for (i = 0; i < FCB_SEARCH_ORACLE_VECTOR_COUNT; ++i) {
        const fcb_search_oracle_vector *tc = &FCB_SEARCH_ORACLE_VECTORS[i];
        int16_t x_prime[40];
        int32_t d[40];
        int16_t signs[40];
        int32_t d_abs[40];
        int32_t phi[40][40];
        int8_t positions[4];
        int64_t sum_out[2];
        int entered;
        int16_t code[40];
        int16_t z[40];

        g729_fcb_adjusted_target(tc->x, tc->y, tc->gp, x_prime);
        CHECK(vec16_equal(x_prime, tc->x_prime, 40));

        g729_fcb_correlation_d(x_prime, tc->h_search, d);
        CHECK(vec32_equal(d, tc->d, 40));

        g729_fcb_signs_from_d(d, signs, d_abs);
        CHECK(vec16_equal(signs, tc->signs, 40));
        CHECK(vec32_equal(d_abs, tc->d_abs, 40));

        g729_fcb_phi_prime(tc->h_search, signs, phi);
        CHECK(phi_equal(&phi[0][0], &tc->phi[0][0]));

        g729_fcb_search_depth_first(d_abs, phi, positions, sum_out);
        CHECK(vec8_equal(positions, tc->full_positions, 4));
        CHECK(vec64_equal(sum_out, tc->full_sum, 2));

        entered = g729_fcb_search_depth_first_threshold_scan(
            d_abs, phi, positions, sum_out, tc->limit);
        CHECK(entered == tc->threshold_entered);
        CHECK(vec8_equal(positions, tc->threshold_positions, 4));
        CHECK(vec64_equal(sum_out, tc->threshold_sum, 2));

        g729_fcb_build_code(positions, signs, tc->int_lag, tc->prev_gp, code);
        CHECK(vec16_equal(code, tc->code, 40));

        g729_fcb_filter_code(code, tc->h, z);
        CHECK(vec16_equal(z, tc->z, 40));
        CHECK(g729_fcb_pack_s(positions, signs) == tc->s);
        CHECK(g729_fcb_pack_c(positions) == tc->c);
    }

    return 0;
}
