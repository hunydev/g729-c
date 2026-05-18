#define _POSIX_C_SOURCE 200809L

#include "g729.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_FRAMES 20000
#define RING_FRAMES 64

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

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static long parse_frames(int argc, char **argv) {
    char *end = NULL;
    long frames;
    if (argc < 2) {
        return DEFAULT_FRAMES;
    }
    frames = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || frames <= 0) {
        return -1;
    }
    return frames;
}

int main(int argc, char **argv) {
    long frames = parse_frames(argc, argv);
    uint8_t ring[RING_FRAMES][G729_FRAME_BYTES];
    int16_t pcm[G729_FRAME_SAMPLES];
    uint8_t bits[G729_FRAME_BYTES];
    int16_t out[G729_FRAME_SAMPLES];
    uint32_t noise = 0x729a5eedu;
    uint64_t encode_hash = 1469598103934665603ull;
    uint64_t decode_hash = 1099511628211ull;
    g729_encoder enc;
    g729_decoder dec;
    double encode_start;
    double encode_seconds;
    double decode_start;
    double decode_seconds;
    double audio_seconds;
    long frame;
    int i;

    if (frames <= 0) {
        fprintf(stderr, "usage: %s [positive-frame-count]\n", argv[0]);
        return 2;
    }

    g729_encoder_init(&enc);
    for (frame = 0; frame < RING_FRAMES; ++frame) {
        fill_pcm((int)frame, &noise, pcm);
        if (g729_encode_frame(&enc, pcm, ring[frame]) != G729_OK) {
            fprintf(stderr, "benchmark setup encode failed\n");
            return 1;
        }
    }

    noise = 0x729a5eedu;
    g729_encoder_init(&enc);
    encode_start = now_seconds();
    for (frame = 0; frame < frames; ++frame) {
        fill_pcm((int)frame, &noise, pcm);
        if (g729_encode_frame(&enc, pcm, bits) != G729_OK) {
            fprintf(stderr, "encode failed at frame %ld\n", frame);
            return 1;
        }
        for (i = 0; i < G729_FRAME_BYTES; ++i) {
            encode_hash = mix_byte(encode_hash, bits[i]);
        }
    }
    encode_seconds = now_seconds() - encode_start;

    g729_decoder_init(&dec);
    decode_start = now_seconds();
    for (frame = 0; frame < frames; ++frame) {
        if (g729_decode_frame(&dec, ring[frame % RING_FRAMES], out) != G729_OK) {
            fprintf(stderr, "decode failed at frame %ld\n", frame);
            return 1;
        }
        for (i = 0; i < G729_FRAME_SAMPLES; ++i) {
            decode_hash = mix_i16(decode_hash, out[i]);
        }
    }
    decode_seconds = now_seconds() - decode_start;
    audio_seconds = (double)frames * 0.01;

    if (encode_seconds <= 0.0 || decode_seconds <= 0.0) {
        fprintf(stderr, "timer returned non-positive duration\n");
        return 1;
    }

    printf("frames: %ld\n", frames);
    printf("audio_seconds: %.2f\n", audio_seconds);
    printf("encode_seconds: %.6f\n", encode_seconds);
    printf("decode_seconds: %.6f\n", decode_seconds);
    printf("encode_fps: %.2f\n", (double)frames / encode_seconds);
    printf("decode_fps: %.2f\n", (double)frames / decode_seconds);
    printf("encode_realtime: %.2fx\n", audio_seconds / encode_seconds);
    printf("decode_realtime: %.2fx\n", audio_seconds / decode_seconds);
    printf("encode_checksum: %016llx\n", (unsigned long long)encode_hash);
    printf("decode_checksum: %016llx\n", (unsigned long long)decode_hash);

    return 0;
}
