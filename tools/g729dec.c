#include "g729.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void usage(FILE *f) {
    fprintf(f, "usage: g729dec [input.g729|-] [output.s16le|-]\n");
}

static int open_input(const char *path, FILE **fp) {
    if (path == NULL || strcmp(path, "-") == 0) {
        *fp = stdin;
        return 0;
    }
    *fp = fopen(path, "rb");
    if (*fp == NULL) {
        fprintf(stderr, "g729dec: open input %s: %s\n", path, strerror(errno));
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
        fprintf(stderr, "g729dec: open output %s: %s\n", path, strerror(errno));
        return 1;
    }
    return 0;
}

static int close_if_file(FILE *fp, FILE *standard, const char *label) {
    if (fp == NULL || fp == standard) {
        return 0;
    }
    if (fclose(fp) != 0) {
        fprintf(stderr, "g729dec: close %s: %s\n", label, strerror(errno));
        return 1;
    }
    return 0;
}

static int write_pcm16le(FILE *out, const int16_t pcm[G729_FRAME_SAMPLES]) {
    uint8_t buf[G729_FRAME_SAMPLES * 2];
    int i;
    for (i = 0; i < G729_FRAME_SAMPLES; ++i) {
        uint16_t v = (uint16_t)pcm[i];
        buf[2 * i] = (uint8_t)(v & 0xffu);
        buf[2 * i + 1] = (uint8_t)(v >> 8);
    }
    if (fwrite(buf, 1, sizeof(buf), out) != sizeof(buf)) {
        fprintf(stderr, "g729dec: write output: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    FILE *in = NULL;
    FILE *out = NULL;
    g729_decoder dec;
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

    g729_decoder_init(&dec);
    for (;;) {
        uint8_t bits[G729_FRAME_BYTES];
        int16_t pcm[G729_FRAME_SAMPLES];
        size_t n = fread(bits, 1, sizeof(bits), in);
        int rc;

        if (n == 0) {
            if (ferror(in)) {
                fprintf(stderr, "g729dec: read input: %s\n", strerror(errno));
                status = 1;
            }
            break;
        }
        if (n != sizeof(bits)) {
            fprintf(stderr, "g729dec: trailing partial G.729 frame (%lu bytes)\n",
                    (unsigned long)n);
            status = 1;
            break;
        }
        rc = g729_decode_frame(&dec, bits, pcm);
        if (rc != G729_OK) {
            fprintf(stderr, "g729dec: decode frame: %s\n", g729_strerror(rc));
            status = 1;
            break;
        }
        if (write_pcm16le(out, pcm) != 0) {
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
