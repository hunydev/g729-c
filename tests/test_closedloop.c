#include "g729_closedloop.h"

#include <stdint.h>
#include <stdio.h>

#include "g729_pitch.h"
#include "fixtures/closedloop_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int vec_equal(const int16_t *a, const int16_t *b, int n) {
    int i;
    for (i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    int i;

    {
        int16_t tmin;
        int16_t tmax;
        g729_closedloop_subframe1_window(20, &tmin, &tmax);
        CHECK(tmin == 20);
        CHECK(tmax == 26);
        g729_closedloop_subframe1_window(143, &tmin, &tmax);
        CHECK(tmin == 137);
        CHECK(tmax == 143);
        g729_closedloop_subframe2_window(20, &tmin, &tmax);
        CHECK(tmin == 20);
        CHECK(tmax == 29);
        g729_closedloop_subframe2_window(143, &tmin, &tmax);
        CHECK(tmin == 134);
        CHECK(tmax == 143);
    }

    CHECK(g729_closedloop_encode_p1(19, 1) == 0);
    CHECK(g729_closedloop_encode_p1(20, -1) == 1);
    CHECK(g729_closedloop_encode_p1(85, -1) == 196);
    CHECK(g729_closedloop_encode_p1(85, 0) == 197);
    CHECK(g729_closedloop_encode_p1(86, 0) == 198);
    CHECK(g729_closedloop_encode_p2(20, -1, 20) == 1);
    CHECK(g729_closedloop_encode_p2(20, 0, 20) == 2);
    CHECK(g729_closedloop_encode_p0(0) == g729_pitch_parity(0));

    for (i = 0; i < CLOSEDLOOP_ORACLE_VECTOR_COUNT; ++i) {
        const closedloop_oracle_vector *tc = &CLOSEDLOOP_ORACLE_VECTORS[i];
        int16_t residual[G729_CLOSEDLOOP_SUBFRAME_LEN];
        int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN];
        int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN];
        int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN];
        int16_t v[G729_CLOSEDLOOP_SUBFRAME_LEN];
        int16_t y[G729_CLOSEDLOOP_SUBFRAME_LEN];
        int16_t tmin = 0;
        int16_t tmax = 0;
        int16_t refined_lag = 0;
        int8_t refined_frac = 0;
        int16_t gp;
        g729_closedloop_pitch_result search;

        g729_closedloop_lp_residual_subframe(tc->speech, tc->a_hat,
                                             tc->residual_mem, residual);
        CHECK(vec_equal(residual, tc->residual,
                        G729_CLOSEDLOOP_SUBFRAME_LEN));

        g729_closedloop_target_signal(tc->a_hat, residual, tc->sw_mem, x);
        CHECK(vec_equal(x, tc->x, G729_CLOSEDLOOP_SUBFRAME_LEN));

        g729_closedloop_impulse_response(tc->a_hat, h);
        CHECK(vec_equal(h, tc->h, G729_CLOSEDLOOP_SUBFRAME_LEN));

        g729_closedloop_backward_filter(x, h, xb);
        CHECK(vec_equal(xb, tc->xb, G729_CLOSEDLOOP_SUBFRAME_LEN));

        if (tc->subframe == 0) {
            g729_closedloop_subframe1_window(tc->centre, &tmin, &tmax);
        } else {
            g729_closedloop_subframe2_window(tc->centre, &tmin, &tmax);
        }
        CHECK(tmin == tc->tmin);
        CHECK(tmax == tc->tmax);

        search = g729_closedloop_search_integer(xb, tc->exc, tc->centre,
                                                tc->subframe);
        CHECK(search.int_lag == tc->search_lag);
        CHECK(search.rn == tc->search_rn);

        if (tc->subframe == 0) {
            g729_closedloop_refine_fraction_subframe1(
                xb, tc->exc, search.int_lag, &refined_lag, &refined_frac);
            CHECK(g729_closedloop_encode_p1(refined_lag, refined_frac) ==
                  tc->p1);
            CHECK(g729_closedloop_encode_p0(tc->p1) == tc->p0);
        } else {
            g729_closedloop_refine_fraction_subframe2(
                xb, tc->exc, search.int_lag, tc->int_t1, &refined_lag,
                &refined_frac);
            CHECK(g729_closedloop_encode_p2(refined_lag, refined_frac,
                                            tc->tmin) == tc->p2);
        }
        CHECK(refined_lag == tc->refined_lag);
        CHECK(refined_frac == tc->refined_frac);

        g729_closedloop_adaptive_vector(tc->exc, refined_lag, refined_frac, v);
        CHECK(vec_equal(v, tc->v, G729_CLOSEDLOOP_SUBFRAME_LEN));

        gp = g729_closedloop_gp_and_y(x, v, h, y);
        CHECK(gp == tc->gp);
        CHECK(vec_equal(y, tc->y, G729_CLOSEDLOOP_SUBFRAME_LEN));
    }

    return 0;
}
