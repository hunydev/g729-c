#ifndef G729_LPC_H
#define G729_LPC_H

#include <stdint.h>

#define G729_LPC_ORDER 10
#define G729_LPC_WINDOW_SAMPLES 240

void g729_lpc_window_speech(const int16_t speech[G729_LPC_WINDOW_SAMPLES],
                            int16_t windowed[G729_LPC_WINDOW_SAMPLES]);
int g729_lpc_autocorrelate(const int16_t windowed[G729_LPC_WINDOW_SAMPLES],
                           int32_t r[G729_LPC_ORDER + 1]);
void g729_lpc_apply_lag_window(int32_t r[G729_LPC_ORDER + 1]);
void g729_lpc_levinson_durbin(const int32_t r[G729_LPC_ORDER + 1],
                              int16_t a[G729_LPC_ORDER + 1]);
void g729_lpc_analyze(const int16_t speech[G729_LPC_WINDOW_SAMPLES],
                      int16_t a[G729_LPC_ORDER + 1]);

#endif
