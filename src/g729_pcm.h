#ifndef G729_PCM_H
#define G729_PCM_H

#include <stddef.h>
#include <stdint.h>

#include "g729.h"

typedef struct g729_preprocessor {
    int32_t x1;
    int32_t x2;
    int32_t y1;
    int32_t y2;
} g729_preprocessor;

void g729_preprocessor_reset(g729_preprocessor *pre);
void g729_preprocessor_process(g729_preprocessor *pre,
                               const int16_t *in,
                               int16_t *out,
                               size_t len);
void g729_preprocessor_process_frame(g729_preprocessor *pre,
                                     const int16_t in[G729_FRAME_SAMPLES],
                                     int16_t out[G729_FRAME_SAMPLES]);

#endif
