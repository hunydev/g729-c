#include "g729_lpc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/lpc_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int vec11_equal(const int16_t a[G729_LPC_ORDER + 1],
                       const int16_t b[G729_LPC_ORDER + 1]) {
    int i;
    for (i = 0; i <= G729_LPC_ORDER; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    int v;

    for (v = 0; v < LPC_ORACLE_VECTOR_COUNT; ++v) {
        int16_t a[G729_LPC_ORDER + 1];
        g729_lpc_analyze(LPC_ORACLE_VECTORS[v].speech, a);
        if (!vec11_equal(a, LPC_ORACLE_VECTORS[v].a)) {
            fprintf(stderr, "lpc analyzer mismatch: vector=%s\n",
                    LPC_ORACLE_VECTORS[v].name);
            return 1;
        }
    }

    {
        int16_t windowed[G729_LPC_WINDOW_SAMPLES];
        int32_t r[G729_LPC_ORDER + 1];
        int scale;
        int k;
        for (k = 0; k < G729_LPC_WINDOW_SAMPLES; ++k) {
            windowed[k] = 1024;
        }
        scale = g729_lpc_autocorrelate(windowed, r);
        CHECK(scale == 0);
        for (k = 0; k <= G729_LPC_ORDER; ++k) {
            CHECK(r[k] == (int32_t)(240 - k) * 1024 * 1024);
        }
    }

    {
        int16_t windowed[G729_LPC_WINDOW_SAMPLES];
        int32_t r[G729_LPC_ORDER + 1];
        int scale;
        int i;
        for (i = 0; i < G729_LPC_WINDOW_SAMPLES; ++i) {
            windowed[i] = 30000;
        }
        scale = g729_lpc_autocorrelate(windowed, r);
        CHECK(scale > 0);
        CHECK(r[0] > 0);
    }

    {
        int32_t r[G729_LPC_ORDER + 1];
        int16_t a[G729_LPC_ORDER + 1];
        int i;
        memset(r, 0, sizeof(r));
        r[0] = 1;
        g729_lpc_levinson_durbin(r, a);
        CHECK(a[0] == 4096);
        for (i = 1; i <= G729_LPC_ORDER; ++i) {
            CHECK(a[i] == 0);
        }
    }

    return 0;
}
