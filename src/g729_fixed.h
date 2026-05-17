#ifndef G729_FIXED_H
#define G729_FIXED_H

#include <stdint.h>

typedef int16_t g729_word16;
typedef int32_t g729_word32;

#define G729_MAX16 ((g729_word16)32767)
#define G729_MIN16 ((g729_word16)(-32767 - 1))
#define G729_MAX32 ((g729_word32)2147483647)
#define G729_MIN32 ((g729_word32)(-2147483647 - 1))

g729_word16 g729_saturate(g729_word32 x);
g729_word16 g729_add(g729_word16 a, g729_word16 b);
g729_word16 g729_sub(g729_word16 a, g729_word16 b);
g729_word16 g729_negate(g729_word16 a);
g729_word16 g729_abs_s(g729_word16 a);

g729_word32 g729_l_add(g729_word32 a, g729_word32 b);
g729_word32 g729_l_sub(g729_word32 a, g729_word32 b);
g729_word32 g729_l_negate(g729_word32 a);
g729_word32 g729_l_abs(g729_word32 a);

g729_word32 g729_l_mult(g729_word16 a, g729_word16 b);
g729_word32 g729_l_mac(g729_word32 acc, g729_word16 a, g729_word16 b);
g729_word32 g729_l_msu(g729_word32 acc, g729_word16 a, g729_word16 b);
g729_word16 g729_mult(g729_word16 a, g729_word16 b);
g729_word16 g729_mult_r(g729_word16 a, g729_word16 b);

g729_word16 g729_shl(g729_word16 a, g729_word16 n);
g729_word16 g729_shr(g729_word16 a, g729_word16 n);
g729_word16 g729_shr_r(g729_word16 a, g729_word16 n);
g729_word32 g729_l_shl(g729_word32 a, g729_word16 n);
g729_word32 g729_l_shr(g729_word32 a, g729_word16 n);
g729_word32 g729_l_shr_r(g729_word32 a, g729_word16 n);

g729_word16 g729_extract_h(g729_word32 x);
g729_word16 g729_extract_l(g729_word32 x);
g729_word32 g729_l_deposit_h(g729_word16 x);
g729_word32 g729_l_deposit_l(g729_word16 x);
g729_word16 g729_round(g729_word32 x);

g729_word16 g729_norm_s(g729_word16 x);
g729_word16 g729_norm_l(g729_word32 x);
g729_word16 g729_div_s(g729_word16 num, g729_word16 den);

void g729_fixed_clear_overflow(void);
int g729_fixed_overflow(void);

#endif
