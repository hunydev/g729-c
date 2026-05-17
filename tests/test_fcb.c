#include "g729_fcb.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/fcb_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int code_equal(const int16_t a[G729_SUBFRAME_SAMPLES],
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
    int16_t code[G729_SUBFRAME_SAMPLES];
    int16_t want[G729_SUBFRAME_SAMPLES];
    int positions[4];
    g729_fcb_indices idx;
    int i;

    g729_fcb_decode_positions(0, positions);
    CHECK(positions[0] == 0);
    CHECK(positions[1] == 1);
    CHECK(positions[2] == 2);
    CHECK(positions[3] == 3);

    g729_fcb_decode_positions(0x053C, positions);
    CHECK(positions[0] == 20);
    CHECK(positions[1] == 36);
    CHECK(positions[2] == 22);
    CHECK(positions[3] == 8);

    memset(code, 0x5A, sizeof(code));
    idx.positions = 0;
    idx.signs = 0x0F;
    g729_fcb_decode(idx, 40, 0, code);
    memset(want, 0, sizeof(want));
    want[0] = G729_FCB_PULSE_AMPLITUDE;
    want[1] = G729_FCB_PULSE_AMPLITUDE;
    want[2] = G729_FCB_PULSE_AMPLITUDE;
    want[3] = G729_FCB_PULSE_AMPLITUDE;
    CHECK(code_equal(code, want));

    idx.positions = 0x053C;
    idx.signs = 0x09;
    g729_fcb_decode(idx, 40, 0, code);
    memset(want, 0, sizeof(want));
    want[20] = G729_FCB_PULSE_AMPLITUDE;
    want[36] = G729_FCB_NEGATIVE_PULSE_AMPLITUDE;
    want[22] = G729_FCB_NEGATIVE_PULSE_AMPLITUDE;
    want[8] = G729_FCB_PULSE_AMPLITUDE;
    CHECK(code_equal(code, want));

    idx.positions = 0;
    idx.signs = 0x01;
    g729_fcb_decode(idx, 20, 8192, code);
    CHECK(code[0] == G729_FCB_PULSE_AMPLITUDE);
    CHECK(code[1] == G729_FCB_NEGATIVE_PULSE_AMPLITUDE);
    CHECK(code[2] == G729_FCB_NEGATIVE_PULSE_AMPLITUDE);
    CHECK(code[3] == G729_FCB_NEGATIVE_PULSE_AMPLITUDE);
    CHECK(code[20] >= 4095 && code[20] <= 4097);
    CHECK(code[21] >= -4097 && code[21] <= -4095);

    idx.positions = 0x1234;
    idx.signs = 0x0A;
    g729_fcb_decode(idx, 25, 10000, code);
    g729_fcb_decode_positions(idx.positions, positions);
    g729_fcb_place_pulses(positions, idx.signs, want);
    g729_fcb_apply_pitch_enhancement(want, 25, 10000);
    CHECK(code_equal(code, want));

    CHECK(g729_fcb_clamp_pitch_gain_for_enhancement(-100) ==
          G729_FCB_INITIAL_PITCH_ENHANCEMENT_Q14);
    CHECK(g729_fcb_clamp_pitch_gain_for_enhancement(3277) == 3277);
    CHECK(g729_fcb_clamp_pitch_gain_for_enhancement(9000) == 9000);
    CHECK(g729_fcb_clamp_pitch_gain_for_enhancement(20000) == 13017);

    memset(code, 0, sizeof(code));
    code[0] = G729_FCB_PULSE_AMPLITUDE;
    g729_fcb_apply_pitch_enhancement(code, 0, 8192);
    for (i = 1; i < G729_SUBFRAME_SAMPLES; ++i) {
        CHECK(code[i] == 0);
    }

    for (i = 0; i < FCB_ORACLE_VECTOR_COUNT; ++i) {
        idx.positions = FCB_ORACLE_VECTORS[i].positions;
        idx.signs = FCB_ORACLE_VECTORS[i].signs;
        g729_fcb_decode(idx,
                        FCB_ORACLE_VECTORS[i].t,
                        FCB_ORACLE_VECTORS[i].beta_q14,
                        code);
        CHECK(code_equal(code, FCB_ORACLE_VECTORS[i].code));
    }

    return 0;
}
