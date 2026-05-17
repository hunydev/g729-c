#include "g729_fixed.h"

#include <stdint.h>

static int fixed_overflow_flag;

static void set_overflow(void) {
    fixed_overflow_flag = 1;
}

void g729_fixed_clear_overflow(void) {
    fixed_overflow_flag = 0;
}

int g729_fixed_overflow(void) {
    return fixed_overflow_flag;
}

static g729_word32 saturate32_from_i64(int64_t x) {
    if (x > (int64_t)G729_MAX32) {
        set_overflow();
        return G729_MAX32;
    }
    if (x < (int64_t)G729_MIN32) {
        set_overflow();
        return G729_MIN32;
    }
    return (g729_word32)x;
}

static g729_word16 arshift16(g729_word16 x, unsigned n) {
    int32_t mag;
    int32_t shifted;
    if (n == 0u) {
        return x;
    }
    if (x >= 0) {
        return (g729_word16)((uint16_t)x >> n);
    }
    mag = -(int32_t)x;
    shifted = -((mag + ((int32_t)1 << n) - 1) / ((int32_t)1 << n));
    return (g729_word16)shifted;
}

static g729_word32 arshift32(g729_word32 x, unsigned n) {
    int64_t mag;
    int64_t denom;
    int64_t shifted;
    if (n == 0u) {
        return x;
    }
    if (x >= 0) {
        return (g729_word32)((uint32_t)x >> n);
    }
    mag = -(int64_t)x;
    denom = (int64_t)1 << n;
    shifted = -((mag + denom - 1) / denom);
    return (g729_word32)shifted;
}

g729_word16 g729_saturate(g729_word32 x) {
    if (x > (g729_word32)G729_MAX16) {
        set_overflow();
        return G729_MAX16;
    }
    if (x < (g729_word32)G729_MIN16) {
        set_overflow();
        return G729_MIN16;
    }
    return (g729_word16)x;
}

g729_word16 g729_add(g729_word16 a, g729_word16 b) {
    return g729_saturate((g729_word32)a + (g729_word32)b);
}

g729_word16 g729_sub(g729_word16 a, g729_word16 b) {
    return g729_saturate((g729_word32)a - (g729_word32)b);
}

g729_word16 g729_negate(g729_word16 a) {
    if (a == G729_MIN16) {
        set_overflow();
        return G729_MAX16;
    }
    return (g729_word16)(-a);
}

g729_word16 g729_abs_s(g729_word16 a) {
    if (a == G729_MIN16) {
        set_overflow();
        return G729_MAX16;
    }
    if (a < 0) {
        return (g729_word16)(-a);
    }
    return a;
}

g729_word32 g729_l_add(g729_word32 a, g729_word32 b) {
    return saturate32_from_i64((int64_t)a + (int64_t)b);
}

g729_word32 g729_l_sub(g729_word32 a, g729_word32 b) {
    return saturate32_from_i64((int64_t)a - (int64_t)b);
}

g729_word32 g729_l_negate(g729_word32 a) {
    if (a == G729_MIN32) {
        set_overflow();
        return G729_MAX32;
    }
    return (g729_word32)(-a);
}

g729_word32 g729_l_abs(g729_word32 a) {
    if (a == G729_MIN32) {
        set_overflow();
        return G729_MAX32;
    }
    if (a < 0) {
        return (g729_word32)(-a);
    }
    return a;
}

g729_word32 g729_l_mult(g729_word16 a, g729_word16 b) {
    if (a == G729_MIN16 && b == G729_MIN16) {
        set_overflow();
        return G729_MAX32;
    }
    return (g729_word32)((int64_t)a * (int64_t)b * 2);
}

g729_word32 g729_l_mac(g729_word32 acc, g729_word16 a, g729_word16 b) {
    return g729_l_add(acc, g729_l_mult(a, b));
}

g729_word32 g729_l_msu(g729_word32 acc, g729_word16 a, g729_word16 b) {
    return g729_l_sub(acc, g729_l_mult(a, b));
}

g729_word16 g729_mult(g729_word16 a, g729_word16 b) {
    g729_word32 prod;
    if (a == G729_MIN16 && b == G729_MIN16) {
        return G729_MAX16;
    }
    prod = (g729_word32)((int32_t)a * (int32_t)b);
    return (g729_word16)arshift32(prod, 15);
}

g729_word16 g729_mult_r(g729_word16 a, g729_word16 b) {
    g729_word32 prod;
    if (a == G729_MIN16 && b == G729_MIN16) {
        return G729_MAX16;
    }
    prod = (g729_word32)((int32_t)a * (int32_t)b + 0x4000);
    return g729_saturate(arshift32(prod, 15));
}

g729_word16 g729_shl(g729_word16 a, g729_word16 n) {
    int64_t scaled;
    if (n <= 0) {
        if (n < -16) {
            n = -16;
        }
        return g729_shr(a, (g729_word16)(-n));
    }
    if (n >= 16) {
        if (a > 0) {
            set_overflow();
            return G729_MAX16;
        }
        if (a < 0) {
            set_overflow();
            return G729_MIN16;
        }
        return 0;
    }
    scaled = (int64_t)a * ((int64_t)1 << (unsigned)n);
    return g729_saturate(saturate32_from_i64(scaled));
}

g729_word16 g729_shr(g729_word16 a, g729_word16 n) {
    if (n < 0) {
        if (n < -16) {
            n = -16;
        }
        return g729_shl(a, (g729_word16)(-n));
    }
    if (n >= 15) {
        if (a < 0) {
            return -1;
        }
        return 0;
    }
    return arshift16(a, (unsigned)n);
}

g729_word16 g729_shr_r(g729_word16 a, g729_word16 n) {
    g729_word16 out;
    if (n <= 0) {
        return g729_shl(a, (g729_word16)(-n));
    }
    if (n > 15) {
        return 0;
    }
    out = g729_shr(a, n);
    if ((((uint16_t)a) & (uint16_t)(1u << (unsigned)(n - 1))) != 0u) {
        out = g729_add(out, 1);
    }
    return out;
}

g729_word32 g729_l_shl(g729_word32 a, g729_word16 n) {
    int64_t scaled;
    if (n <= 0) {
        if (n < -32) {
            n = -32;
        }
        return g729_l_shr(a, (g729_word16)(-n));
    }
    if (n >= 32) {
        if (a > 0) {
            set_overflow();
            return G729_MAX32;
        }
        if (a < 0) {
            set_overflow();
            return G729_MIN32;
        }
        return 0;
    }
    scaled = (int64_t)a * ((int64_t)1 << (unsigned)n);
    return saturate32_from_i64(scaled);
}

g729_word32 g729_l_shr(g729_word32 a, g729_word16 n) {
    if (n < 0) {
        if (n < -32) {
            n = -32;
        }
        return g729_l_shl(a, (g729_word16)(-n));
    }
    if (n >= 31) {
        if (a < 0) {
            return -1;
        }
        return 0;
    }
    return arshift32(a, (unsigned)n);
}

g729_word32 g729_l_shr_r(g729_word32 a, g729_word16 n) {
    g729_word32 out;
    if (n <= 0) {
        return g729_l_shl(a, (g729_word16)(-n));
    }
    if (n > 31) {
        return 0;
    }
    out = g729_l_shr(a, n);
    if ((((uint32_t)a) & (uint32_t)(1u << (unsigned)(n - 1))) != 0u) {
        out = g729_l_add(out, 1);
    }
    return out;
}

g729_word16 g729_extract_h(g729_word32 x) {
    return (g729_word16)arshift32(x, 16);
}

g729_word16 g729_extract_l(g729_word32 x) {
    uint32_t low = (uint32_t)x & 0xFFFFu;
    if (low <= 32767u) {
        return (g729_word16)low;
    }
    return (g729_word16)((int32_t)low - 65536);
}

g729_word32 g729_l_deposit_h(g729_word16 x) {
    return (g729_word32)((int64_t)x * 65536);
}

g729_word32 g729_l_deposit_l(g729_word16 x) {
    return (g729_word32)x;
}

g729_word16 g729_round(g729_word32 x) {
    return g729_extract_h(g729_l_add(x, 0x00008000));
}

g729_word16 g729_norm_s(g729_word16 x) {
    uint16_t ux;
    g729_word16 n = 0;
    if (x == 0) {
        return 0;
    }
    if (x == -1) {
        return 15;
    }
    ux = (uint16_t)x;
    if (x < 0) {
        ux = (uint16_t)(~ux);
    }
    while (ux < 0x4000u) {
        ux = (uint16_t)(ux << 1);
        ++n;
    }
    return n;
}

g729_word16 g729_norm_l(g729_word32 x) {
    uint32_t ux;
    g729_word16 n = 0;
    if (x == 0) {
        return 0;
    }
    if (x == -1) {
        return 31;
    }
    ux = (uint32_t)x;
    if (x < 0) {
        ux = ~ux;
    }
    while (ux < 0x40000000u) {
        ux <<= 1;
        ++n;
    }
    return n;
}

g729_word16 g729_div_s(g729_word16 num, g729_word16 den) {
    int i;
    g729_word16 q = 0;
    g729_word32 n;
    g729_word32 d;
    if (num < 0 || den <= 0 || num > den) {
        return G729_MAX16;
    }
    if (num == 0) {
        return 0;
    }
    if (num == den) {
        return G729_MAX16;
    }

    n = (g729_word32)num * 32768;
    d = (g729_word32)den * 32768;
    for (i = 0; i < 15; ++i) {
        q = (g729_word16)(q * 2);
        n *= 2;
        if (n >= d) {
            n -= d;
            q = (g729_word16)(q | 1);
        }
    }
    return q;
}
