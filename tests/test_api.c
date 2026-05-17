#include "g729.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
        return 1; \
    } \
} while (0)

int main(void) {
    g729_encoder enc;
    g729_decoder dec;
    int16_t pcm[G729_FRAME_SAMPLES] = {0};
    uint8_t bits[G729_FRAME_BYTES] = {0};

    CHECK(G729_SAMPLE_RATE == 8000);
    CHECK(G729_FRAME_SAMPLES == 80);
    CHECK(G729_FRAME_BYTES == 10);

    memset(&enc, 0xA5, sizeof(enc));
    memset(&dec, 0x5A, sizeof(dec));
    g729_encoder_init(&enc);
    g729_decoder_init(&dec);

    CHECK(g729_encode_frame(NULL, pcm, bits) == G729_ERR_NULL);
    CHECK(g729_encode_frame(&enc, NULL, bits) == G729_ERR_NULL);
    CHECK(g729_encode_frame(&enc, pcm, NULL) == G729_ERR_NULL);
    CHECK(g729_decode_frame(NULL, bits, pcm) == G729_ERR_NULL);
    CHECK(g729_decode_frame(&dec, NULL, pcm) == G729_ERR_NULL);
    CHECK(g729_decode_frame(&dec, bits, NULL) == G729_ERR_NULL);

    CHECK(g729_encode_frame(&enc, pcm, bits) == G729_OK);
    CHECK(g729_decode_frame(&dec, bits, pcm) == G729_OK);
    CHECK(strcmp(g729_strerror(G729_OK), "ok") == 0);

    g729_encoder_reset(&enc);
    g729_decoder_reset(&dec);
    CHECK(g729_encode_frame(&enc, pcm, bits) == G729_OK);
    CHECK(g729_decode_frame(&dec, bits, pcm) == G729_OK);

    return 0;
}
