#include "g729.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fixtures/decode_oracle_vectors.h"

#ifndef G729DEC_BIN
#define G729DEC_BIN "build/tools/g729dec"
#endif

#ifndef G729_CLI_TEST_DIR
#define G729_CLI_TEST_DIR "build/tests"
#endif

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

static int write_bits_file(const char *path) {
    FILE *f = fopen(path, "wb");
    int frame;
    if (f == NULL) {
        return 1;
    }
    for (frame = 0; frame < DECODE_ORACLE_FRAMES_PER_VECTOR; ++frame) {
        if (fwrite(DECODE_ORACLE_VECTORS[3].frames[frame].bits,
                   1,
                   G729_FRAME_BYTES,
                   f) != G729_FRAME_BYTES) {
            fclose(f);
            return 1;
        }
    }
    return fclose(f) != 0;
}

static int expected_sample(int idx) {
    int frame = idx / G729_FRAME_SAMPLES;
    int sample = idx % G729_FRAME_SAMPLES;
    return DECODE_ORACLE_VECTORS[3].frames[frame].pcm[sample];
}

static int verify_pcm_file(const char *path) {
    FILE *f = fopen(path, "rb");
    int idx;
    if (f == NULL) {
        return 0;
    }
    for (idx = 0; idx < DECODE_ORACLE_FRAMES_PER_VECTOR * G729_FRAME_SAMPLES;
         ++idx) {
        int lo = fgetc(f);
        int hi = fgetc(f);
        uint16_t u;
        int16_t sample;
        if (lo == EOF || hi == EOF) {
            fclose(f);
            return 0;
        }
        u = (uint16_t)((uint16_t)(uint8_t)lo |
                       (uint16_t)((uint16_t)(uint8_t)hi << 8));
        sample = (int16_t)u;
        if (sample != expected_sample(idx)) {
            fclose(f);
            return 0;
        }
    }
    if (fgetc(f) != EOF) {
        fclose(f);
        return 0;
    }
    return fclose(f) == 0;
}

int main(void) {
    const char *in_path = G729_CLI_TEST_DIR "/cli_decode_input.g729";
    const char *out_path = G729_CLI_TEST_DIR "/cli_decode_output.s16le";
    const char *stdout_path = G729_CLI_TEST_DIR "/cli_decode_stdout.s16le";
    const char *partial_path = G729_CLI_TEST_DIR "/cli_decode_partial.g729";
    char cmd[1024];
    FILE *partial;

    CHECK(write_bits_file(in_path) == 0);

    snprintf(cmd, sizeof(cmd), "%s %s %s", G729DEC_BIN, in_path, out_path);
    CHECK(system(cmd) == 0);
    CHECK(verify_pcm_file(out_path));

    snprintf(cmd, sizeof(cmd), "%s < %s > %s", G729DEC_BIN, in_path,
             stdout_path);
    CHECK(system(cmd) == 0);
    CHECK(verify_pcm_file(stdout_path));

    partial = fopen(partial_path, "wb");
    CHECK(partial != NULL);
    CHECK(fwrite(DECODE_ORACLE_VECTORS[0].frames[0].bits, 1, 3, partial) == 3);
    CHECK(fclose(partial) == 0);
    snprintf(cmd, sizeof(cmd), "%s %s %s.partial >/dev/null 2>/dev/null",
             G729DEC_BIN, partial_path, out_path);
    CHECK(system(cmd) != 0);

    return 0;
}
