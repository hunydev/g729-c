#include "g729.h"

#include <stdint.h>
#include <stdio.h>

#define STRESS_FRAMES 1200

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int16_t clamp16(int32_t v) {
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return (int16_t)v;
}

static uint32_t next_lcg(uint32_t *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int32_t triangle(int phase, int amplitude) {
    int p = phase & 255;
    int v = p < 128 ? p : 255 - p;
    return ((v * 2 - 127) * amplitude) / 127;
}

static void fill_pcm(int frame, uint32_t *noise, int16_t pcm[G729_FRAME_SAMPLES]) {
    int i;
    for (i = 0; i < G729_FRAME_SAMPLES; ++i) {
        int phase = frame * 11 + i * 3;
        int32_t shaped = triangle(phase, 9000) + triangle(phase * 5 + 17, 2300);
        int32_t n = (int32_t)((next_lcg(noise) >> 16) & 0x1fffu) - 4096;
        int32_t envelope = 4096 + (int32_t)((frame * 97 + i * 13) & 0x0fffu);
        int32_t sample = ((shaped + n) * envelope) >> 13;
        if ((frame & 31) == 0) {
            sample = 0;
        } else if ((frame & 63) == 17 && i == 0) {
            sample = 30000;
        }
        pcm[i] = clamp16(sample);
    }
}

static uint64_t mix_byte(uint64_t h, uint8_t v) {
    h ^= (uint64_t)v;
    return h * 1099511628211ull;
}

static uint64_t mix_i16(uint64_t h, int16_t v) {
    uint16_t u = (uint16_t)v;
    h = mix_byte(h, (uint8_t)(u & 0xffu));
    return mix_byte(h, (uint8_t)(u >> 8));
}

static int run_stream(uint64_t *bits_hash, uint64_t *pcm_hash) {
    g729_encoder enc;
    g729_decoder dec;
    uint32_t noise = 0x729a5eedu;
    uint64_t bh = 1469598103934665603ull;
    uint64_t ph = 1099511628211ull;
    int frame;

    g729_encoder_init(&enc);
    g729_decoder_init(&dec);

    for (frame = 0; frame < STRESS_FRAMES; ++frame) {
        int16_t in[G729_FRAME_SAMPLES];
        uint8_t bits[G729_FRAME_BYTES];
        int16_t out[G729_FRAME_SAMPLES];
        int i;

        fill_pcm(frame, &noise, in);
        if (g729_encode_frame(&enc, in, bits) != G729_OK) {
            return 1;
        }
        if (g729_decode_frame(&dec, bits, out) != G729_OK) {
            return 1;
        }

        for (i = 0; i < G729_FRAME_BYTES; ++i) {
            bh = mix_byte(bh, bits[i]);
        }
        for (i = 0; i < G729_FRAME_SAMPLES; ++i) {
            ph = mix_i16(ph, out[i]);
        }
    }

    *bits_hash = bh;
    *pcm_hash = ph;
    return 0;
}

int main(void) {
    uint64_t bits_a;
    uint64_t bits_b;
    uint64_t pcm_a;
    uint64_t pcm_b;

    CHECK(run_stream(&bits_a, &pcm_a) == 0);
    CHECK(run_stream(&bits_b, &pcm_b) == 0);
    CHECK(bits_a == bits_b);
    CHECK(pcm_a == pcm_b);
    CHECK(bits_a != 1469598103934665603ull);
    CHECK(pcm_a != 1099511628211ull);

    return 0;
}
