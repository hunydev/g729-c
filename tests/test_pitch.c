#include "g729_pitch.h"

#include <stdint.h>
#include <stdio.h>

#include "fixtures/pitch_oracle_vectors.h"

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static uint8_t expected_parity(uint8_t p1) {
    uint8_t bits = (uint8_t)((p1 >> 2) & 0x3Fu);
    uint8_t x = (uint8_t)(bits ^ (bits >> 4));
    x = (uint8_t)(x ^ (x >> 2));
    x = (uint8_t)(x ^ (x >> 1));
    return (uint8_t)((x & 1u) ^ 1u);
}

static int vector_equal(const int16_t a[G729_SUBFRAME_SAMPLES],
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
    static const struct {
        uint8_t p1;
        int want_int;
        int want_frac;
    } sf1_cases[] = {
        {0, 19, 1},
        {1, 20, -1},
        {2, 20, 0},
        {3, 20, 1},
        {4, 21, -1},
        {197, 85, 0},
        {198, 86, 0},
        {199, 87, 0},
        {255, 143, 0},
    };
    int i;

    for (i = 0; i < (int)(sizeof(sf1_cases) / sizeof(sf1_cases[0])); ++i) {
        g729_pitch_delay got = g729_pitch_decode_subframe1(sf1_cases[i].p1);
        CHECK(got.t_int == sf1_cases[i].want_int);
        CHECK(got.t_frac == sf1_cases[i].want_frac);
    }

    for (i = 0; i < 256; ++i) {
        g729_pitch_delay got = g729_pitch_decode_subframe1((uint8_t)i);
        CHECK(got.t_int >= 19);
        CHECK(got.t_int <= 143);
        CHECK(got.t_frac >= -1);
        CHECK(got.t_frac <= 1);
    }

    {
        g729_pitch_delay got = g729_pitch_decode_subframe2(16, 50);
        CHECK(got.t_int == 50);
        CHECK(got.t_frac == -1);
        got = g729_pitch_decode_subframe2(0, 60);
        CHECK(got.t_int == 54);
        CHECK(got.t_frac == 1);
        got = g729_pitch_decode_subframe2(31, 60);
        CHECK(got.t_int == 65);
        CHECK(got.t_frac == -1);
        got = g729_pitch_decode_subframe2(0, 20);
        CHECK(got.t_int == 19);
        CHECK(got.t_frac == 1);
        got = g729_pitch_decode_subframe2(31, 140);
        CHECK(got.t_int == 144);
        CHECK(got.t_frac == -1);
    }

    for (i = 19; i <= 143; ++i) {
        int p2;
        for (p2 = 0; p2 < 32; ++p2) {
            g729_pitch_delay got = g729_pitch_decode_subframe2((uint8_t)p2, i);
            CHECK(got.t_int >= 19);
            CHECK(got.t_int <= 144);
            CHECK(got.t_frac >= -1);
            CHECK(got.t_frac <= 1);
        }
    }

    {
        int match_count = 0;
        for (i = 0; i < 256; ++i) {
            uint8_t expected = expected_parity((uint8_t)i);
            uint8_t p0;
            CHECK(g729_pitch_parity((uint8_t)i) == expected);
            for (p0 = 0; p0 <= 1; ++p0) {
                int got = g729_pitch_check_parity((uint8_t)i, p0);
                int want = (p0 == expected);
                CHECK(got == want);
                if (got) {
                    ++match_count;
                }
            }
        }
        CHECK(match_count == 256);
    }

    {
        int16_t past[250];
        int16_t v[G729_SUBFRAME_SAMPLES];
        for (i = 0; i < 250; ++i) {
            past[i] = (int16_t)i;
        }
        g729_pitch_adaptive_codebook(60, 0, past, 250, v);
        for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
            CHECK(v[i] == (int16_t)(190 + i));
        }
    }

    for (i = 0; i < PITCH_ORACLE_VECTOR_COUNT; ++i) {
        int16_t v[G729_SUBFRAME_SAMPLES];
        g729_pitch_adaptive_codebook(PITCH_ORACLE_VECTORS[i].t_int,
                                     PITCH_ORACLE_VECTORS[i].t_frac,
                                     PITCH_ORACLE_VECTORS[i].past_exc,
                                     PITCH_ORACLE_PAST_EXC_LEN,
                                     v);
        CHECK(vector_equal(v, PITCH_ORACLE_VECTORS[i].v));
    }

    return 0;
}
