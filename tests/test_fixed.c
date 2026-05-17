#include "g729_fixed.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int abs32(int x) {
    return x < 0 ? -x : x;
}

int main(void) {
    CHECK(g729_saturate(0) == 0);
    g729_fixed_clear_overflow();
    CHECK(!g729_fixed_overflow());
    CHECK(g729_saturate(32768) == G729_MAX16);
    CHECK(g729_fixed_overflow());
    g729_fixed_clear_overflow();
    CHECK(g729_saturate(-32769) == G729_MIN16);
    CHECK(g729_fixed_overflow());
    g729_fixed_clear_overflow();

    CHECK(g729_add(100, 200) == 300);
    CHECK(g729_add(30000, 30000) == G729_MAX16);
    CHECK(g729_add(-30000, -30000) == G729_MIN16);
    CHECK(g729_sub(200, 50) == 150);
    CHECK(g729_sub(0, G729_MIN16) == G729_MAX16);
    CHECK(g729_negate(G729_MIN16) == G729_MAX16);
    CHECK(g729_abs_s(G729_MIN16) == G729_MAX16);

    CHECK(g729_l_add(100, 200) == 300);
    CHECK(g729_l_add(2000000000, 2000000000) == G729_MAX32);
    CHECK(g729_l_add(-2000000000, -2000000000) == G729_MIN32);
    CHECK(g729_l_sub(300, 100) == 200);
    CHECK(g729_l_sub(0, G729_MIN32) == G729_MAX32);
    CHECK(g729_l_negate(G729_MIN32) == G729_MAX32);
    CHECK(g729_l_abs(G729_MIN32) == G729_MAX32);

    CHECK(g729_l_mult(100, 200) == 40000);
    CHECK(g729_l_mult(G729_MIN16, G729_MIN16) == G729_MAX32);
    CHECK(g729_l_mac(1000, 100, 200) == 41000);
    CHECK(g729_l_mac(G729_MAX32 - 10, 100, 100) == G729_MAX32);
    CHECK(g729_l_msu(100000, 100, 200) == 60000);
    CHECK(g729_l_msu(G729_MIN32 + 10, 100, 100) == G729_MIN32);
    CHECK(g729_mult(16384, 16384) == 8192);
    CHECK(g729_mult(G729_MAX16, G729_MAX16) == 32766);
    CHECK(g729_mult(G729_MIN16, G729_MIN16) == G729_MAX16);
    CHECK(g729_mult(16384, -16384) == -8192);
    CHECK(g729_mult_r(32767, 2) == 2);

    CHECK(g729_shl(100, 3) == 800);
    CHECK(g729_shl(20000, 2) == G729_MAX16);
    CHECK(g729_shl(-20000, 2) == G729_MIN16);
    CHECK(g729_shl(100, -1) == 50);
    CHECK(g729_shr(800, 3) == 100);
    CHECK(g729_shr(-100, 1) == -50);
    CHECK(g729_shr(-2, 1) == -1);
    CHECK(g729_shr(100, -1) == 200);
    CHECK(g729_shr(G729_MAX16, 15) == 0);
    CHECK(g729_shr(G729_MIN16, 15) == -1);
    CHECK(g729_shr_r(3, 1) == 2);
    CHECK(g729_shr_r(4, 2) == 1);
    CHECK(g729_shr_r(6, 2) == 2);
    CHECK(g729_shr_r(-3, 1) == -1);
    CHECK(g729_shr_r(G729_MAX16, 15) == 1);
    CHECK(g729_shr_r(G729_MIN16, 15) == -1);

    CHECK(g729_l_shl(1000, 10) == 1024000);
    CHECK(g729_l_shl(1000000000, 2) == G729_MAX32);
    CHECK(g729_l_shl(-1000000000, 2) == G729_MIN32);
    CHECK(g729_l_shr(1024000, 10) == 1000);
    CHECK(g729_l_shr(-2, 1) == -1);
    CHECK(g729_l_shr(1000, -1) == 2000);
    CHECK(g729_l_shr(1000000, 31) == 0);
    CHECK(g729_l_shr(-1000000, 31) == -1);
    CHECK(g729_l_shr_r(3, 1) == 2);
    CHECK(g729_l_shr_r(4, 2) == 1);
    CHECK(g729_l_shr_r(6, 2) == 2);
    CHECK(g729_l_shr_r(-3, 1) == -1);
    CHECK(g729_l_shr_r(G729_MAX32, 31) == 1);
    CHECK(g729_l_shr_r(G729_MIN32, 31) == -1);

    CHECK(g729_extract_h(0x00010000) == 1);
    CHECK(g729_extract_h(-0x00010000) == -1);
    CHECK(g729_extract_h(G729_MIN32) == G729_MIN16);
    CHECK(g729_extract_l(0x00008000) == G729_MIN16);
    CHECK(g729_extract_l(0x12345678) == 0x5678);
    CHECK(g729_l_deposit_h(1) == 0x00010000);
    CHECK(g729_l_deposit_h(-1) == -0x00010000);
    CHECK(g729_l_deposit_h(G729_MIN16) == G729_MIN32);
    CHECK(g729_l_deposit_l(G729_MIN16) == -32768);
    CHECK(g729_round(0x00008000) == 1);
    CHECK(g729_round(-0x00008000) == 0);
    CHECK(g729_round(G729_MAX32) == G729_MAX16);
    CHECK(g729_round(G729_MIN32) == G729_MIN16);

    CHECK(g729_norm_s(0) == 0);
    CHECK(g729_norm_s(1) == 14);
    CHECK(g729_norm_s(G729_MAX16) == 0);
    CHECK(g729_norm_s(G729_MIN16) == 0);
    CHECK(g729_norm_s(-1) == 15);
    CHECK(g729_norm_l(0) == 0);
    CHECK(g729_norm_l(1) == 30);
    CHECK(g729_norm_l(G729_MAX32) == 0);
    CHECK(g729_norm_l(G729_MIN32) == 0);
    CHECK(g729_norm_l(-1) == 31);

    CHECK(g729_div_s(0, 1) == 0);
    CHECK(g729_div_s(1000, 1000) == G729_MAX16);
    CHECK(abs32(g729_div_s(16384, 32767) - 16384) <= 1);
    CHECK(abs32(g729_div_s(8192, 32767) - 8192) <= 1);
    CHECK(g729_div_s(100, 50) == G729_MAX16);
    CHECK(g729_div_s(-1, 100) == G729_MAX16);
    CHECK(g729_div_s(100, 0) == G729_MAX16);

    return 0;
}
