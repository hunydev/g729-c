#ifndef G729_H
#define G729_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define G729_SAMPLE_RATE 8000
#define G729_FRAME_SAMPLES 80
#define G729_FRAME_BYTES 10

typedef enum g729_result {
    G729_OK = 0,
    G729_ERR_NULL = -1,
    G729_ERR_SHORT_PCM = -2,
    G729_ERR_SHORT_BITSTREAM = -3,
    G729_ERR_SHORT_OUTPUT = -4,
    G729_ERR_UNSUPPORTED_ANNEXB = -5
} g729_result;

typedef struct g729_encoder {
    uint32_t magic;
    uint32_t reserved0;
    uint64_t reserved[255];
} g729_encoder;

typedef struct g729_decoder {
    uint32_t magic;
    uint32_t reserved0;
    uint64_t reserved[255];
} g729_decoder;

void g729_encoder_init(g729_encoder *enc);
void g729_encoder_reset(g729_encoder *enc);
void g729_decoder_init(g729_decoder *dec);
void g729_decoder_reset(g729_decoder *dec);

int g729_encode_frame(g729_encoder *enc,
                      const int16_t pcm[G729_FRAME_SAMPLES],
                      uint8_t bits[G729_FRAME_BYTES]);

int g729_decode_frame(g729_decoder *dec,
                      const uint8_t bits[G729_FRAME_BYTES],
                      int16_t pcm[G729_FRAME_SAMPLES]);

const char *g729_strerror(int code);

#ifdef __cplusplus
}
#endif

#endif
