#include "g729.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "g729_bitstream.h"
#include "g729_closedloop.h"
#include "g729_fcb.h"
#include "g729_fcb_search.h"
#include "g729_fixed.h"
#include "g729_gain.h"
#include "g729_gain_quant.h"
#include "g729_hp.h"
#include "g729_lpc.h"
#include "g729_lsp.h"
#include "g729_openloop.h"
#include "g729_pitch.h"
#include "g729_pcm.h"
#include "g729_postfilter.h"
#include "g729_synth.h"

enum {
    G729_ENCODER_MAGIC = 0x47373239u,
    G729_DECODER_MAGIC = 0x47374439u,
    G729_PAST_EXC_LEN = G729_POSTFILTER_PITCH_MAX + 10,
    G729_INITIAL_ERASURE_PITCH_DELAY = 60,
    G729_ENCODER_OLD_SPEECH_LEN = 240,
    G729_ENCODER_OLD_EXC_LEN = G729_GAIN_QUANT_EXC_MEM_SAMPLES,
    G729_ENCODER_CLOSEDLOOP_HISTORY = G729_CLOSEDLOOP_SEARCH_HISTORY,
    G729_ENCODER_FCB_FRAME_LIMIT = 180,
    G729_ENCODER_FCB_SUBFRAME0_LIMIT = 90
};

static const int16_t encoder_initial_lsp_old[10] = {
    30000, 26000, 21000, 15000, 8000,
    0, -8000, -15000, -21000, -26000,
};

typedef struct g729_encoder_private {
    g729_preprocessor pre;
    int16_t old_speech[G729_ENCODER_OLD_SPEECH_LEN];
    int16_t old_wspeech[G729_OPENLOOP_OLD_WSPEECH_LEN];
    int16_t old_exc[G729_ENCODER_OLD_EXC_LEN];
    int16_t lsp_old[10];
    int16_t past_qua_en[4];
    int16_t freq_prev[4][10];
    int16_t a_q12_latest[G729_LPC_ORDER + 1];
    int16_t lp_residual_mem[G729_LPC_ORDER];
    int16_t sw_mem[G729_LPC_ORDER];
    int16_t t_op;
    g729_lsp_decoder lsp_dec;
    int16_t a_hat_sf1[G729_LPC_ORDER + 1];
    int16_t a_hat_sf2[G729_LPC_ORDER + 1];
    int16_t sw_mem_err[G729_LPC_ORDER];
    int16_t lp_residual_mem_q[G729_LPC_ORDER];
    int16_t int_t1;
    int16_t int_t2;
    int8_t frac1;
    int8_t frac2;
    uint8_t p1;
    uint8_t p0;
    uint8_t p2;
    int16_t prev_gp_q14;
    int prev_taming;
    uint8_t s1;
    uint8_t s2;
    uint16_t c1;
    uint16_t c2;
    uint8_t ga1;
    uint8_t gb1;
    uint8_t ga2;
    uint8_t gb2;
    uint16_t l0;
    uint16_t l1;
    uint16_t l2;
    uint16_t l3;
    int core_fcb_entries_remaining;
} g729_encoder_private;

typedef struct g729_decoder_private {
    g729_lsp_decoder lsp;
    g729_gain_decoder gain;
    g729_synthesizer synth;
    g729_postfilter postfilter;
    g729_hp_filter_state hp;
    int16_t past_exc[G729_PAST_EXC_LEN];
    int16_t prev_gp_q14;
    int have_prev_gp_q14;
    int64_t prev_fixed_gain_q14;
    int prev_pitch_delay;
    int have_prev_pitch_delay;
} g729_decoder_private;

typedef char g729_encoder_private_fits[
    sizeof(g729_encoder_private) <= sizeof(((g729_encoder *)0)->reserved) ? 1 : -1];
typedef char g729_decoder_private_fits[
    sizeof(g729_decoder_private) <= sizeof(((g729_decoder *)0)->reserved) ? 1 : -1];

static g729_encoder_private *encoder_private(g729_encoder *enc) {
    return (g729_encoder_private *)(void *)enc->reserved;
}

static g729_decoder_private *decoder_private(g729_decoder *dec) {
    return (g729_decoder_private *)(void *)dec->reserved;
}

static void encoder_state_init(g729_encoder_private *st) {
    int i;
    if (st == NULL) {
        return;
    }
    g729_lsp_init_freq_prev(st->freq_prev);
    memcpy(st->lsp_old, encoder_initial_lsp_old, sizeof(st->lsp_old));
    for (i = 0; i < 4; ++i) {
        st->past_qua_en[i] = G729_GAIN_PAST_ERROR_DEFAULT;
    }
}

static int32_t encoder_apply_gain_q14_to_q0(int16_t gain_q14,
                                            int16_t sample_q0) {
    int32_t prod = g729_l_mult(gain_q14, sample_q0);
    return g729_round(g729_l_shl(prod, 1));
}

static int32_t encoder_apply_gc_to_q12(int16_t gc_mant_q14,
                                       int8_t gc_exp,
                                       int16_t sample_q12) {
    int32_t prod;
    int32_t scaled;
    int shift_r;
    if (gc_mant_q14 == 0 || sample_q12 == 0) {
        return 0;
    }
    prod = g729_l_mult(gc_mant_q14, sample_q12);
    shift_r = 12 - (int)gc_exp;
    if (shift_r >= 0) {
        scaled = g729_l_shr(prod, (int16_t)shift_r);
    } else {
        scaled = g729_l_shl(prod, (int16_t)(-shift_r));
    }
    return g729_round(g729_l_shl(scaled, 1));
}

static int encoder_lpc_step(g729_encoder_private *st,
                            const int16_t pcm[G729_FRAME_SAMPLES]) {
    int16_t processed[G729_FRAME_SAMPLES];
    int16_t q_q15[10];
    int16_t omega[10];
    g729_lsp_indices idx;
    if (st == NULL || pcm == NULL) {
        return G729_ERR_NULL;
    }

    st->core_fcb_entries_remaining = G729_ENCODER_FCB_FRAME_LIMIT;

    g729_preprocessor_process_frame(&st->pre, pcm, processed);
    memmove(st->old_speech, &st->old_speech[G729_FRAME_SAMPLES],
            (G729_ENCODER_OLD_SPEECH_LEN - G729_FRAME_SAMPLES) *
                sizeof(st->old_speech[0]));
    memcpy(&st->old_speech[G729_ENCODER_OLD_SPEECH_LEN - G729_FRAME_SAMPLES],
           processed, sizeof(processed));

    g729_lpc_analyze(st->old_speech, st->a_q12_latest);
    if (g729_lsp_lp_to_lsp(st->a_q12_latest, q_q15) != 0) {
        memcpy(q_q15, st->lsp_old, sizeof(q_q15));
    } else {
        memcpy(st->lsp_old, q_q15, sizeof(st->lsp_old));
    }
    g729_lsp_lsp_vector_to_lsf(q_q15, omega);
    idx = g729_lsp_quantize(omega, st->freq_prev);
    st->l0 = idx.l0;
    st->l1 = idx.l1;
    st->l2 = idx.l2;
    st->l3 = idx.l3;
    g729_lsp_decode(&st->lsp_dec, idx, st->a_hat_sf1, st->a_hat_sf2);
    return G729_OK;
}

static void encoder_openloop_step(g729_encoder_private *st) {
    g729_openloop_search_result result;
    if (st == NULL) {
        return;
    }
    result = g729_openloop_step_split_search(st->a_hat_sf1, st->a_hat_sf2,
                                             &st->old_speech[120],
                                             st->lp_residual_mem, st->sw_mem,
                                             st->old_wspeech);
    st->t_op = result.top;
}

static void encoder_closedloop_excitation_search(
    const g729_encoder_private *st,
    const int16_t residual[G729_CLOSEDLOOP_SUBFRAME_LEN],
    int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN]) {
    memcpy(exc, &st->old_exc[G729_ENCODER_OLD_EXC_LEN -
                             G729_ENCODER_CLOSEDLOOP_HISTORY],
           G729_ENCODER_CLOSEDLOOP_HISTORY * sizeof(exc[0]));
    memcpy(&exc[G729_ENCODER_CLOSEDLOOP_HISTORY], residual,
           G729_CLOSEDLOOP_SUBFRAME_LEN * sizeof(exc[0]));
}

static int encoder_fcb_subframe_limit(const g729_encoder_private *st, int sub) {
    int limit = st->core_fcb_entries_remaining;
    if (sub == 0 && limit > G729_ENCODER_FCB_SUBFRAME0_LIMIT) {
        limit = G729_ENCODER_FCB_SUBFRAME0_LIMIT;
    }
    return limit;
}

static void encoder_record_fcb_entries(g729_encoder_private *st, int entered) {
    st->core_fcb_entries_remaining -= entered;
    if (st->core_fcb_entries_remaining < 0) {
        st->core_fcb_entries_remaining = 0;
    }
}

static void encoder_fcb_step(g729_encoder_private *st,
                             int sub,
                             const int16_t x[G729_SUBFRAME_SAMPLES],
                             const int16_t y[G729_SUBFRAME_SAMPLES],
                             const int16_t h[G729_SUBFRAME_SAMPLES],
                             const int16_t v[G729_SUBFRAME_SAMPLES],
                             int16_t gp_unq_q14) {
    int16_t x_prime[G729_SUBFRAME_SAMPLES];
    int16_t h_search[G729_SUBFRAME_SAMPLES];
    int32_t d[G729_SUBFRAME_SAMPLES];
    int16_t signs[G729_SUBFRAME_SAMPLES];
    int32_t d_abs[G729_SUBFRAME_SAMPLES];
    int32_t phi[G729_SUBFRAME_SAMPLES][G729_SUBFRAME_SAMPLES];
    int8_t positions[4];
    int64_t sum_out[2];
    int16_t code[G729_SUBFRAME_SAMPLES];
    int16_t z[G729_SUBFRAME_SAMPLES];
    int16_t int_lag = sub == 0 ? st->int_t1 : st->int_t2;
    int limit;
    int entered;
    int n;
    int32_t gpc_pred_q12;
    g729_gain_quant_search_result search;
    g729_gain_quant_reconstruct_result recon;
    int16_t gp_tamed;
    int16_t u_hat[G729_SUBFRAME_SAMPLES];

    g729_fcb_adjusted_target(x, y, gp_unq_q14, x_prime);
    memcpy(h_search, h, sizeof(h_search));
    g729_fcb_apply_pitch_enhancement(
        h_search, int_lag,
        g729_fcb_clamp_pitch_gain_for_enhancement(st->prev_gp_q14));
    g729_fcb_correlation_d(x_prime, h_search, d);
    g729_fcb_signs_from_d(d, signs, d_abs);
    g729_fcb_phi_prime(h_search, signs, phi);

    limit = encoder_fcb_subframe_limit(st, sub);
    entered = g729_fcb_search_depth_first_threshold_scan(d_abs, phi, positions,
                                                         sum_out, limit);
    encoder_record_fcb_entries(st, entered);

    g729_fcb_build_code(positions, signs, int_lag, st->prev_gp_q14, code);
    g729_fcb_filter_code(code, h, z);

    gpc_pred_q12 =
        g729_gain_quant_predicted_gc_q12_wide(st->past_qua_en, code);
    search = g729_gain_quant_search_conjugate_float_center(x, y, z,
                                                           gpc_pred_q12);
    gp_tamed = g729_gain_quant_tame(search.gp_q14, st->old_exc);
    st->prev_taming = gp_tamed != search.gp_q14;
    search.gp_q14 = gp_tamed;

    if (sub == 0) {
        st->s1 = g729_fcb_pack_s(positions, signs);
        st->c1 = g729_fcb_pack_c(positions);
        st->ga1 = search.ga_bits;
        st->gb1 = search.gb_bits;
    } else {
        st->s2 = g729_fcb_pack_s(positions, signs);
        st->c2 = g729_fcb_pack_c(positions);
        st->ga2 = search.ga_bits;
        st->gb2 = search.gb_bits;
    }

    recon = g729_gain_quant_reconstruct_wide(st->past_qua_en, code,
                                             search.ga, search.gb);
    for (n = 30; n < G729_SUBFRAME_SAMPLES; ++n) {
        int32_t gp_y = encoder_apply_gain_q14_to_q0(search.gp_q14, y[n]);
        int32_t gc_z = encoder_apply_gc_to_q12(recon.gc_mant_q14,
                                               recon.gc_exp, z[n]);
        st->sw_mem_err[n - 30] =
            g729_saturate((int32_t)x[n] - gp_y - gc_z);
    }

    memmove(st->old_exc, &st->old_exc[G729_SUBFRAME_SAMPLES],
            (G729_ENCODER_OLD_EXC_LEN - G729_SUBFRAME_SAMPLES) *
                sizeof(st->old_exc[0]));
    g729_synth_build_excitation(search.gp_q14, recon.gc_mant_q14,
                                recon.gc_exp, v, code, u_hat);
    memcpy(&st->old_exc[G729_ENCODER_OLD_EXC_LEN - G729_SUBFRAME_SAMPLES],
           u_hat, sizeof(u_hat));

    g729_gain_quant_update_past_qua_en(st->past_qua_en, search.gamma_c_q13);
    st->prev_gp_q14 = search.gp_q14;
}

static void encoder_commit_closedloop_pitch(
    g729_encoder_private *st,
    int sub,
    const int16_t a_hat[G729_LPC_ORDER + 1],
    const int16_t speech[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN],
    const int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN],
    int16_t int_lag,
    int8_t frac) {
    int16_t v[G729_CLOSEDLOOP_SUBFRAME_LEN];
    int16_t y[G729_CLOSEDLOOP_SUBFRAME_LEN];
    int16_t gp;
    (void)a_hat;
    (void)exc;

    g729_pitch_adaptive_codebook(int_lag, frac, st->old_exc,
                                 G729_ENCODER_OLD_EXC_LEN, v);
    gp = g729_closedloop_gp_and_y(x, v, h, y);

    if (sub == 0) {
        st->int_t1 = int_lag;
        st->frac1 = frac;
        st->p1 = g729_closedloop_encode_p1(int_lag, frac);
        st->p0 = g729_closedloop_encode_p0(st->p1);
    } else {
        int16_t tmin;
        int16_t tmax;
        g729_closedloop_subframe2_window(st->int_t1, &tmin, &tmax);
        (void)tmax;
        st->int_t2 = int_lag;
        st->frac2 = frac;
        st->p2 = g729_closedloop_encode_p2(int_lag, frac, tmin);
    }

    encoder_fcb_step(st, sub, x, y, h, v, gp);
    memcpy(st->lp_residual_mem_q, &speech[30],
           G729_LPC_ORDER * sizeof(st->lp_residual_mem_q[0]));
}

static void encoder_closedloop_step(g729_encoder_private *st, int sub) {
    const int16_t *a_hat = sub == 0 ? st->a_hat_sf1 : st->a_hat_sf2;
    const int16_t *speech = &st->old_speech[120 + 40 * sub];
    int16_t residual[G729_CLOSEDLOOP_SUBFRAME_LEN];
    int16_t x[G729_CLOSEDLOOP_SUBFRAME_LEN];
    int16_t h[G729_CLOSEDLOOP_SUBFRAME_LEN];
    int16_t xb[G729_CLOSEDLOOP_SUBFRAME_LEN];
    int16_t exc[G729_CLOSEDLOOP_SEARCH_LEN];
    int16_t centre = sub == 0 ? st->t_op : st->int_t1;
    g729_closedloop_pitch_result pitch;
    int16_t int_lag;
    int8_t frac;

    g729_closedloop_lp_residual_subframe(speech, a_hat,
                                         st->lp_residual_mem_q, residual);
    g729_closedloop_target_signal(a_hat, residual, st->sw_mem_err, x);
    g729_closedloop_impulse_response(a_hat, h);
    g729_closedloop_backward_filter(x, h, xb);
    encoder_closedloop_excitation_search(st, residual, exc);

    pitch = g729_closedloop_search_integer(xb, exc, centre, sub);
    if (sub == 0) {
        g729_closedloop_refine_fraction_subframe1(xb, exc, pitch.int_lag,
                                                  &int_lag, &frac);
    } else {
        g729_closedloop_refine_fraction_subframe2(xb, exc, pitch.int_lag,
                                                  st->int_t1, &int_lag,
                                                  &frac);
    }

    encoder_commit_closedloop_pitch(st, sub, a_hat, speech, x, h, exc,
                                    int_lag, frac);
}

static int encode_frame(g729_encoder_private *st,
                        const int16_t pcm[G729_FRAME_SAMPLES],
                        uint8_t bits[G729_FRAME_BYTES]) {
    g729_bitstream_frame frame;
    int rc = encoder_lpc_step(st, pcm);
    if (rc != G729_OK) {
        return rc;
    }
    encoder_openloop_step(st);
    encoder_closedloop_step(st, 0);
    encoder_closedloop_step(st, 1);

    frame.l0 = st->l0;
    frame.l1 = st->l1;
    frame.l2 = st->l2;
    frame.l3 = st->l3;
    frame.p1 = st->p1;
    frame.p0 = st->p0;
    frame.c1 = st->c1;
    frame.s1 = st->s1;
    frame.ga1 = st->ga1;
    frame.gb1 = st->gb1;
    frame.p2 = st->p2;
    frame.c2 = st->c2;
    frame.s2 = st->s2;
    frame.ga2 = st->ga2;
    frame.gb2 = st->gb2;
    return g729_bitstream_pack(&frame, bits);
}

static int16_t pitch_enhancement_beta_q14(const g729_decoder_private *st) {
    if (!st->have_prev_gp_q14) {
        return G729_FCB_INITIAL_PITCH_ENHANCEMENT_Q14;
    }
    return g729_fcb_clamp_pitch_gain_for_enhancement(st->prev_gp_q14);
}

static int concealed_pitch_delay(const g729_decoder_private *st) {
    if (!st->have_prev_pitch_delay) {
        return G729_INITIAL_ERASURE_PITCH_DELAY;
    }
    return st->prev_pitch_delay;
}

static void remember_pitch_gain(g729_decoder_private *st, int16_t gp_q14) {
    st->prev_gp_q14 = gp_q14;
    st->have_prev_gp_q14 = 1;
}

static void remember_pitch_delay(g729_decoder_private *st, int t_int) {
    st->prev_pitch_delay = t_int;
    st->have_prev_pitch_delay = 1;
}

static int64_t decoder_gain_q14_from_mant_exp(int16_t mant, int8_t exp) {
    int64_t v;
    if (mant == 0) {
        return 0;
    }
    v = mant;
    if (exp >= 0) {
        unsigned shift = (unsigned)exp;
        if (shift >= 62u) {
            return INT64_MAX;
        }
        return v << shift;
    }
    {
        unsigned shift = (unsigned)(-(int)exp);
        if (shift >= 63u) {
            return 0;
        }
        return v >> shift;
    }
}

static void scale_excitation_for_history(int16_t u[G729_SUBFRAME_SAMPLES],
                                         unsigned shift) {
    int i;
    for (i = 0; i < G729_SUBFRAME_SAMPLES; ++i) {
        u[i] = g729_shr(u[i], (int16_t)shift);
    }
}

static void scale_past_excitation_history(int16_t past[G729_PAST_EXC_LEN],
                                          unsigned shift) {
    int i;
    for (i = 0; i < G729_PAST_EXC_LEN; ++i) {
        past[i] = g729_shr(past[i], (int16_t)shift);
    }
}

static void decode_subframe(g729_decoder_private *st,
                            const int16_t a[11],
                            int t_int,
                            int t_frac,
                            uint16_t c_idx,
                            uint8_t s_idx,
                            uint8_t ga,
                            uint8_t gb,
                            int16_t out[G729_SUBFRAME_SAMPLES]) {
    int16_t beta_q14 = pitch_enhancement_beta_q14(st);
    int16_t v[G729_SUBFRAME_SAMPLES];
    int16_t c[G729_SUBFRAME_SAMPLES];
    int16_t u[G729_SUBFRAME_SAMPLES];
    int16_t commit_u[G729_SUBFRAME_SAMPLES];
    int16_t synth[G729_SUBFRAME_SAMPLES];
    int16_t post[G729_SUBFRAME_SAMPLES];
    g729_gain_result gains;
    g729_fcb_indices fcb_idx;
    g729_gain_indices gain_idx;

    g729_pitch_adaptive_codebook(t_int, t_frac, st->past_exc,
                                 G729_PAST_EXC_LEN, v);
    fcb_idx.positions = c_idx;
    fcb_idx.signs = s_idx;
    g729_fcb_decode(fcb_idx, t_int, beta_q14, c);

    gain_idx.ga = ga;
    gain_idx.gb = gb;
    gains = g729_gain_decode(&st->gain, gain_idx, c);
    st->prev_fixed_gain_q14 =
        decoder_gain_q14_from_mant_exp(gains.gc_mant_q14, gains.gc_exp);

    g729_synth_build_excitation(gains.gp_q14, gains.gc_mant_q14, gains.gc_exp,
                                v, c, u);
    g729_synth_filter(&st->synth, a, u, synth);
    memcpy(commit_u, u, sizeof(commit_u));
    if (st->synth.last_excitation_scale_shift > 0u) {
        scale_past_excitation_history(st->past_exc,
                                      st->synth.last_excitation_scale_shift);
        scale_excitation_for_history(commit_u,
                                     st->synth.last_excitation_scale_shift);
    }

    g729_postfilter_filter(&st->postfilter, a, t_int, synth, post);
    g729_hp_filter_final(&st->hp, post, out);

    memmove(st->past_exc, &st->past_exc[G729_SUBFRAME_SAMPLES],
            (G729_PAST_EXC_LEN - G729_SUBFRAME_SAMPLES) *
                sizeof(st->past_exc[0]));
    memcpy(&st->past_exc[G729_PAST_EXC_LEN - G729_SUBFRAME_SAMPLES], commit_u,
           sizeof(commit_u));

    remember_pitch_gain(st, gains.gp_q14);
    remember_pitch_delay(st, t_int);
}

static int decode_frame(g729_decoder_private *st,
                        const uint8_t bits[G729_FRAME_BYTES],
                        int16_t pcm[G729_FRAME_SAMPLES]) {
    g729_bitstream_frame frame;
    g729_lsp_indices lsp_idx;
    int16_t sf1_a[11];
    int16_t sf2_a[11];
    g729_pitch_delay p1;
    g729_pitch_delay p2;
    int rc = g729_bitstream_unpack(bits, &frame);
    if (rc != G729_OK) {
        return rc;
    }

    lsp_idx.l0 = (uint8_t)frame.l0;
    lsp_idx.l1 = (uint8_t)frame.l1;
    lsp_idx.l2 = (uint8_t)frame.l2;
    lsp_idx.l3 = (uint8_t)frame.l3;
    g729_lsp_decode(&st->lsp, lsp_idx, sf1_a, sf2_a);

    p1 = g729_pitch_decode_subframe1((uint8_t)frame.p1);
    if (!g729_pitch_check_parity((uint8_t)frame.p1, (uint8_t)frame.p0)) {
        p1.t_int = concealed_pitch_delay(st);
        p1.t_frac = 0;
    }
    p2 = g729_pitch_decode_subframe2((uint8_t)frame.p2, p1.t_int);

    decode_subframe(st, sf1_a, p1.t_int, p1.t_frac, frame.c1,
                    (uint8_t)frame.s1, (uint8_t)frame.ga1,
                    (uint8_t)frame.gb1, pcm);
    decode_subframe(st, sf2_a, p2.t_int, p2.t_frac, frame.c2,
                    (uint8_t)frame.s2, (uint8_t)frame.ga2,
                    (uint8_t)frame.gb2, &pcm[G729_SUBFRAME_SAMPLES]);

    return G729_OK;
}

void g729_encoder_init(g729_encoder *enc) {
    if (enc == NULL) {
        return;
    }
    memset(enc, 0, sizeof(*enc));
    enc->magic = G729_ENCODER_MAGIC;
    encoder_state_init(encoder_private(enc));
}

void g729_encoder_reset(g729_encoder *enc) {
    g729_encoder_init(enc);
}

void g729_decoder_init(g729_decoder *dec) {
    if (dec == NULL) {
        return;
    }
    memset(dec, 0, sizeof(*dec));
    dec->magic = G729_DECODER_MAGIC;
}

void g729_decoder_reset(g729_decoder *dec) {
    g729_decoder_init(dec);
}

const char *g729_version_string(void) {
    return G729_VERSION_STRING;
}

int g729_encode_frame(g729_encoder *enc,
                      const int16_t pcm[G729_FRAME_SAMPLES],
                      uint8_t bits[G729_FRAME_BYTES]) {
    g729_encoder_private *st;
    if (enc == NULL || pcm == NULL || bits == NULL) {
        return G729_ERR_NULL;
    }
    if (enc->magic != G729_ENCODER_MAGIC) {
        g729_encoder_init(enc);
    }
    st = encoder_private(enc);
    return encode_frame(st, pcm, bits);
}

int g729_decode_frame(g729_decoder *dec,
                      const uint8_t bits[G729_FRAME_BYTES],
                      int16_t pcm[G729_FRAME_SAMPLES]) {
    g729_decoder_private *st;
    if (dec == NULL || bits == NULL || pcm == NULL) {
        return G729_ERR_NULL;
    }
    if (dec->magic != G729_DECODER_MAGIC) {
        g729_decoder_init(dec);
    }
    st = decoder_private(dec);
    return decode_frame(st, bits, pcm);
}

const char *g729_strerror(int code) {
    switch (code) {
    case G729_OK:
        return "ok";
    case G729_ERR_NULL:
        return "null pointer";
    case G729_ERR_SHORT_PCM:
        return "input PCM length is not one 80-sample frame";
    case G729_ERR_SHORT_BITSTREAM:
        return "bitstream length is not one 10-byte frame";
    case G729_ERR_SHORT_OUTPUT:
        return "output buffer too small";
    case G729_ERR_UNSUPPORTED_ANNEXB:
        return "Annex B SID/CNG/DTX is not supported";
    default:
        return "unknown g729 error";
    }
}
