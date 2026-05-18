#include "g729.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void usage(FILE *f) {
    fprintf(f, "usage: g729enc [input.s16le|-] [output.g729|-]\n");
}

static int open_input(const char *path, FILE **fp) {
    if (path == NULL || strcmp(path, "-") == 0) {
        *fp = stdin;
        return 0;
    }
    *fp = fopen(path, "rb");
    if (*fp == NULL) {
        fprintf(stderr, "g729enc: open input %s: %s\n", path, strerror(errno));
        return 1;
    }
    return 0;
}

static int open_output(const char *path, FILE **fp) {
    if (path == NULL || strcmp(path, "-") == 0) {
        *fp = stdout;
        return 0;
    }
    *fp = fopen(path, "wb");
    if (*fp == NULL) {
        fprintf(stderr, "g729enc: open output %s: %s\n", path, strerror(errno));
        return 1;
    }
    return 0;
}

static int close_if_file(FILE *fp, const FILE *standard, const char *label) {
    if (fp == NULL || fp == standard) {
        return 0;
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "g729enc: close %s: %s\n", label, strerror(errno));
        return 1;
    }
    return 0;
}

static void bytes_to_pcm16le(const uint8_t buf[G729_FRAME_SAMPLES * 2],
                             int16_t pcm[G729_FRAME_SAMPLES]) {
    int i;
    for (i = 0; i < G729_FRAME_SAMPLES; ++i) {
        uint16_t u = (uint16_t)((uint16_t)buf[2 * i] |
                                (uint16_t)((uint16_t)buf[2 * i + 1] << 8));
        pcm[i] = (int16_t)u;
    }
}

int main(int argc, char **argv) {
    FILE *in = NULL;
    FILE *out = NULL;
    g729_encoder enc;
    int status = 0;

    if (argc > 3) {
        usage(stderr);
        return 2;
    }
    if (open_input(argc >= 2 ? argv[1] : NULL, &in) != 0) {
        return 1;
    }
    if (open_output(argc >= 3 ? argv[2] : NULL, &out) != 0) {
        (void)close_if_file(in, stdin, "input");
        return 1;
    }

    g729_encoder_init(&enc);
    for (;;) {
        uint8_t pcm_bytes[G729_FRAME_SAMPLES * 2];
        int16_t pcm[G729_FRAME_SAMPLES];
        uint8_t bits[G729_FRAME_BYTES];
        size_t n = fread(pcm_bytes, 1, sizeof(pcm_bytes), in);
        int rc;

        if (n == 0) {
            if (ferror(in)) {
                fprintf(stderr, "g729enc: read input: %s\n", strerror(errno));
                status = 1;
            }
            break;
        }
        if (n != sizeof(pcm_bytes)) {
            fprintf(stderr, "g729enc: trailing partial PCM frame (%lu bytes)\n",
                    (unsigned long)n);
            status = 1;
            break;
        }

        bytes_to_pcm16le(pcm_bytes, pcm);
        rc = g729_encode_frame(&enc, pcm, bits);
        if (rc != G729_OK) {
            fprintf(stderr, "g729enc: encode frame: %s\n", g729_strerror(rc));
            status = 1;
            break;
        }
        if (fwrite(bits, 1, sizeof(bits), out) != sizeof(bits)) {
            fprintf(stderr, "g729enc: write output: %s\n", strerror(errno));
            status = 1;
            break;
        }
    }

    if (close_if_file(out, stdout, "output") != 0) {
        status = 1;
    }
    if (close_if_file(in, stdin, "input") != 0) {
        status = 1;
    }
    return status;
}
