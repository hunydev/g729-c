#include "g729.h"

#include <stdint.h>

int main(void) {
    g729_encoder enc;
    g729_decoder dec;
    int16_t pcm[G729_FRAME_SAMPLES] = {0};
    uint8_t bits[G729_FRAME_BYTES] = {0};

    g729_encoder_init(&enc);
    g729_decoder_init(&dec);

    (void)g729_encode_frame(&enc, pcm, bits);
    (void)g729_decode_frame(&dec, bits, pcm);
    return 0;
}
