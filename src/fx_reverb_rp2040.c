/*
 * Q15 Reverb for RP2040 (Raspberry Pi Pico)
 *
 * This implementation follows the classic Schroeder–Moorer architecture,
 * combining parallel combs and series allpasses to achieve a natural-sounding
 * reverb tail with controllable decay time (T60) and diffusion characteristics.
 *
 * Buffers are dynamically allocated at init based on delay length arrays.
 *
 * Category: Freeverb-style (Schroeder–Moorer algorithm)
 * Structure:
 *  - Pre-delay: circular buffer of PREDELAY_SAMPLES to separate dry hit
 *  - Comb filters: NUM_COMB parallel lowpass-feedback delay lines with
 *      • Delay lengths defined by COMB_DLY_L/R (prime numbers for diffusion)
 *      • Feedback coefficients computed for T60 times (comb_T60)
 *      • Damping (one-pole lowpass in feedback path)
 *  - Sum comb outputs to create dense early reflections and tail
 *  - Allpass filters: NUM_AP serial allpass delay lines for added diffusion
 *  - Dry/Wet mix: WET_Q15 / DRY_Q15 coefficients
 *  - Master gain: MASTER_GAIN_Q15 for output level
 *
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fx.h"

// saturate to signed 16-bit
static inline int16_t sat16(int32_t x) {
    if (x > 0x7FFF)
        return 0x7FFF;
    if (x < -0x8000)
        return -0x8000;
    return (int16_t)x;
}

// Sample rate and sizes
#define SR_HZ 48000u
#define NUM_COMB 8
#define NUM_AP 4
#define PREDELAY_SAMPLES 960u

// Q15 macros and gains
#define F32_Q15(x) ((int16_t)((x) * 32767.f + 0.5f))
#define WET_Q15 F32_Q15(0.50f)
#define DRY_Q15 F32_Q15(0.50f)
#define COMB_GAIN_Q15 F32_Q15(0.20f)
#define AP_GAIN_Q15 F32_Q15(0.50f)
#define MASTER_GAIN_Q15 F32_Q15(1.50f)

// Delay lengths for comb and allpass
static const uint32_t COMB_DLY_L[NUM_COMB] = {509, 863, 1481, 2521, 4273, 7253, 10007, 15013};
static const uint32_t COMB_DLY_R[NUM_COMB] = {523, 877, 1489, 2531, 4283, 7283, 10037, 15031};
static const uint32_t AP_DLY_L[NUM_AP] = {142, 396, 1071, 3079};
static const uint32_t AP_DLY_R[NUM_AP] = {145, 399, 1073, 3081};

// Comb-specific parameters
static const float comb_T60[NUM_COMB] = {0.25f, 0.30f, 0.40f, 0.80f, 2.00f, 6.00f, 10.00f, 20.00f};
static const int16_t comb_gain[NUM_COMB] = {F32_Q15(0.62f), F32_Q15(0.60f), F32_Q15(0.58f),
                                            F32_Q15(0.55f), F32_Q15(0.52f), F32_Q15(0.50f),
                                            F32_Q15(0.48f), F32_Q15(0.45f)};

// Allpass gain
#define ALLPASS_GAIN AP_GAIN_Q15

// Pre-delay buffers (static)
static int16_t predL[PREDELAY_SAMPLES];
static int16_t predR[PREDELAY_SAMPLES];

// Filter structures
typedef struct {
    int16_t *buf;
    uint32_t size, idx;
    int16_t fb, filt;
} comb_t;
typedef struct {
    int16_t *buf;
    uint32_t size, idx;
    int16_t g;
} ap_t;

// Filter instances
typedef struct {
    comb_t L[NUM_COMB], R[NUM_COMB];
    ap_t AL[NUM_AP], AR[NUM_AP];
    bool enabled;
    uint32_t pred_idx;
} reverb_t;
static reverb_t rev;

// Helpers
static inline int16_t to_q15(int32_t x) { return sat16(x >> 16); }
static inline int32_t from_q15(int16_t q) { return (int32_t)q << 16; }
static inline int16_t mul_q15(int16_t a, int16_t b) {
    int32_t p = (int32_t)a * b;
    return sat16(p >> 15);
}

// Initialize filters and allocate buffers
static void init_filters(void) {
    // Allocate comb buffers and reset state
    for (uint32_t i = 0; i < NUM_COMB; ++i) {
        rev.L[i].size = COMB_DLY_L[i];
        rev.R[i].size = COMB_DLY_R[i];
        rev.L[i].buf = calloc(rev.L[i].size, sizeof(int16_t));
        rev.R[i].buf = calloc(rev.R[i].size, sizeof(int16_t));
        rev.L[i].idx = rev.R[i].idx = 0;
        rev.L[i].fb = rev.R[i].fb = 0;
        rev.L[i].filt = rev.R[i].filt = 0;
    }
    // Compute comb feedback gains
    const float MAX_FB = 0.98f;
    for (uint32_t i = 0; i < NUM_COMB; ++i) {
        float g = powf(10.f, -3.f * COMB_DLY_L[i] / (float)SR_HZ / comb_T60[i]);
        if (g > MAX_FB)
            g = MAX_FB;
        rev.L[i].fb = rev.R[i].fb = (int16_t)(g * 32767.f + 0.5f);
    }
    // Allocate allpass buffers and set gains
    for (uint32_t i = 0; i < NUM_AP; ++i) {
        rev.AL[i].size = AP_DLY_L[i];
        rev.AR[i].size = AP_DLY_R[i];
        rev.AL[i].buf = calloc(rev.AL[i].size, sizeof(int16_t));
        rev.AR[i].buf = calloc(rev.AR[i].size, sizeof(int16_t));
        rev.AL[i].idx = rev.AR[i].idx = 0;
        rev.AL[i].g = rev.AR[i].g = ALLPASS_GAIN;
    }
    // Initialize pre-delay and enable flag
    rev.pred_idx = 0;
    rev.enabled = true;
}

// Comb processing with damping
static inline int16_t comb_process(comb_t *c, int16_t x) {
    int16_t d = c->buf[c->idx];
    const int16_t DAMP_Q15 = F32_Q15(0.40f);
    int32_t fb_term = (int32_t)c->fb * d;
    int16_t fb_q15 = (int16_t)(fb_term >> 15);
    c->filt = mul_q15(c->filt, sat16(F32_Q15(1.0f) - DAMP_Q15)) + mul_q15(fb_q15, DAMP_Q15);
    int32_t w = (int32_t)x + c->filt;
    c->buf[c->idx] = sat16(w);
    if (++c->idx == c->size)
        c->idx = 0;
    return d;
}

// Allpass processing
static inline int16_t ap_process(ap_t *a, int16_t x) {
    int16_t d = a->buf[a->idx];
    int16_t y = sat16((int32_t)d - mul_q15(x, a->g));
    a->buf[a->idx] = sat16((int32_t)x + mul_q15(y, a->g));
    if (++a->idx == a->size)
        a->idx = 0;
    return y;
}

// API functions
const char *fx_name(void) { return "Pico USB Audio Loopback Reverb"; }
void fx_init(void) { init_filters(); }
void fx_set_format(uint8_t, uint32_t) { /* unused */ }
void fx_set_enable(bool en) { rev.enabled = en; }

// Main processing
void fx_process(int32_t *out, int32_t *in, size_t frames) {
    if (!rev.enabled) {
        if (out != in)
            memcpy(out, in, frames * 8);
        return;
    }
    for (size_t i = 0; i < frames; ++i) {
        int16_t dryL = to_q15(in[2 * i]);
        int16_t dryR = to_q15(in[2 * i + 1]);
        // Pre-delay
        int16_t preL = predL[rev.pred_idx];
        int16_t preR = predR[rev.pred_idx];
        predL[rev.pred_idx] = dryL;
        predR[rev.pred_idx] = dryR;
        if (++rev.pred_idx == PREDELAY_SAMPLES)
            rev.pred_idx = 0;
        // Comb network
        int32_t sumL = 0, sumR = 0;
        for (uint32_t k = 0; k < NUM_COMB; ++k) {
            int16_t cL = mul_q15(comb_process(&rev.L[k], preL), comb_gain[k]);
            int16_t cR = mul_q15(comb_process(&rev.R[k], preR), comb_gain[k]);
            sumL += cL;
            sumR += cR;
        }
        int16_t wetL = sat16(sumL);
        int16_t wetR = sat16(sumR);
        // Allpass network
        for (uint32_t k = 0; k < NUM_AP; ++k) {
            wetL = ap_process(&rev.AL[k], wetL);
            wetR = ap_process(&rev.AR[k], wetR);
        }
        // Mix
        int16_t mixL = sat16(mul_q15(dryL, DRY_Q15) + mul_q15(wetL, WET_Q15));
        int16_t mixR = sat16(mul_q15(dryR, DRY_Q15) + mul_q15(wetR, WET_Q15));
        int32_t outL = from_q15(mul_q15(mixL, MASTER_GAIN_Q15));
        int32_t outR = from_q15(mul_q15(mixR, MASTER_GAIN_Q15));
        out[2 * i] = outL;
        out[2 * i + 1] = outR;
    }
}
