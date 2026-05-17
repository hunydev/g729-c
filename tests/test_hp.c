#include "g729_hp.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/hp_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int vec40_equal(const int16_t a[G729_SUBFRAME_SAMPLES],
                       const int16_t b[G729_SUBFRAME_SAMPLES]) {
    int i;
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    int i;

    for (i = 0; i < HP_ORACLE_VECTOR_COUNT; ++i) {
        g729_hp_filter_state state;
        int16_t pre[G729_SUBFRAME_SAMPLES];
        int16_t final[G729_SUBFRAME_SAMPLES];

        state.x[0] = HP_ORACLE_VECTORS[i].x_in[0];
        state.x[1] = HP_ORACLE_VECTORS[i].x_in[1];
        state.y[0] = HP_ORACLE_VECTORS[i].y_in[0];
        state.y[1] = HP_ORACLE_VECTORS[i].y_in[1];

        memset(pre, 0x7f, sizeof(pre));
        memset(final, 0x7f, sizeof(final));
        g729_hp_filter(&state, HP_ORACLE_VECTORS[i].in, pre, final);
        CHECK(vec40_equal(pre, HP_ORACLE_VECTORS[i].pre));
        CHECK(vec40_equal(final, HP_ORACLE_VECTORS[i].final));
        CHECK(state.x[0] == HP_ORACLE_VECTORS[i].x_out[0]);
        CHECK(state.x[1] == HP_ORACLE_VECTORS[i].x_out[1]);
        CHECK(state.y[0] == HP_ORACLE_VECTORS[i].y_out[0]);
        CHECK(state.y[1] == HP_ORACLE_VECTORS[i].y_out[1]);
    }

    {
        g729_hp_filter_state state;
        int16_t final[G729_SUBFRAME_SAMPLES];
        state.x[0] = HP_ORACLE_VECTORS[1].x_in[0];
        state.x[1] = HP_ORACLE_VECTORS[1].x_in[1];
        state.y[0] = HP_ORACLE_VECTORS[1].y_in[0];
        state.y[1] = HP_ORACLE_VECTORS[1].y_in[1];
        g729_hp_filter_final(&state, HP_ORACLE_VECTORS[1].in, final);
        CHECK(vec40_equal(final, HP_ORACLE_VECTORS[1].final));
    }

    {
        g729_hp_filter_state state;
        int16_t pre[G729_SUBFRAME_SAMPLES];
        state.x[0] = HP_ORACLE_VECTORS[2].x_in[0];
        state.x[1] = HP_ORACLE_VECTORS[2].x_in[1];
        state.y[0] = HP_ORACLE_VECTORS[2].y_in[0];
        state.y[1] = HP_ORACLE_VECTORS[2].y_in[1];
        g729_hp_filter_pre(&state, HP_ORACLE_VECTORS[2].in, pre);
        CHECK(vec40_equal(pre, HP_ORACLE_VECTORS[2].pre));
    }

    {
        g729_hp_filter_state state;
        memset(&state, 0x6d, sizeof(state));
        g729_hp_reset(&state);
        CHECK(state.x[0] == 0);
        CHECK(state.x[1] == 0);
        CHECK(state.y[0] == 0);
        CHECK(state.y[1] == 0);
    }

    return 0;
}
