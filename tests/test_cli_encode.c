#include "g729.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fixtures/encode_oracle_vectors.h"

#ifndef G729ENC_BIN
#define G729ENC_BIN "build/tools/g729enc"
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

static int write_pcm_file(const char *path) {
    FILE *f = fopen(path, "wb");
    int frame;
    int sample;
    if (f == NULL) {
        return 1;
    }
    for (frame = 0; frame < ENCODE_ORACLE_FRAMES_PER_VECTOR; ++frame) {
        for (sample = 0; sample < G729_FRAME_SAMPLES; ++sample) {
            uint16_t u =
                (uint16_t)ENCODE_ORACLE_VECTORS[3].frames[frame].pcm[sample];
            if (fputc((int)(u & 0xffu), f) == EOF ||
                fputc((int)(u >> 8), f) == EOF) {
                fclose(f);
                return 1;
            }
        }
    }
    return fclose(f) != 0;
}

static int verify_bits_file(const char *path) {
    FILE *f = fopen(path, "rb");
    int frame;
    int byte;
    if (f == NULL) {
        return 0;
    }
    for (frame = 0; frame < ENCODE_ORACLE_FRAMES_PER_VECTOR; ++frame) {
        for (byte = 0; byte < G729_FRAME_BYTES; ++byte) {
            int got = fgetc(f);
            if (got == EOF ||
                (uint8_t)got !=
                    ENCODE_ORACLE_VECTORS[3].frames[frame].bits[byte]) {
                fclose(f);
                return 0;
            }
        }
    }
    if (fgetc(f) != EOF) {
        fclose(f);
        return 0;
    }
    return fclose(f) == 0;
}

int main(void) {
    const char *in_path = G729_CLI_TEST_DIR "/cli_encode_input.s16le";
    const char *out_path = G729_CLI_TEST_DIR "/cli_encode_output.g729";
    const char *stdout_path = G729_CLI_TEST_DIR "/cli_encode_stdout.g729";
    const char *partial_path = G729_CLI_TEST_DIR "/cli_encode_partial.s16le";
    char cmd[1024];
    FILE *partial;

    CHECK(write_pcm_file(in_path) == 0);

    snprintf(cmd, sizeof(cmd), "%s %s %s", G729ENC_BIN, in_path, out_path);
    CHECK(system(cmd) == 0);
    CHECK(verify_bits_file(out_path));

    snprintf(cmd, sizeof(cmd), "%s < %s > %s", G729ENC_BIN, in_path,
             stdout_path);
    CHECK(system(cmd) == 0);
    CHECK(verify_bits_file(stdout_path));

    partial = fopen(partial_path, "wb");
    CHECK(partial != NULL);
    CHECK(fwrite(&ENCODE_ORACLE_VECTORS[0].frames[0].pcm[0], 1, 3,
                 partial) == 3);
    CHECK(fclose(partial) == 0);
    snprintf(cmd, sizeof(cmd), "%s %s %s.partial >/dev/null 2>/dev/null",
             G729ENC_BIN, partial_path, out_path);
    CHECK(system(cmd) != 0);

    return 0;
}
