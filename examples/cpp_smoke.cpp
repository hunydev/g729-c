#include "g729.h"

#include <cstdint>

int main() {
    g729_encoder enc;
    g729_decoder dec;
    int16_t pcm[G729_FRAME_SAMPLES] = {};
    uint8_t bits[G729_FRAME_BYTES] = {};

    g729_encoder_init(&enc);
    g729_decoder_init(&dec);

    if (g729_encode_frame(&enc, pcm, bits) != G729_OK) {
        return 1;
    }
    if (g729_decode_frame(&dec, bits, pcm) != G729_OK) {
        return 1;
    }
    return 0;
}
