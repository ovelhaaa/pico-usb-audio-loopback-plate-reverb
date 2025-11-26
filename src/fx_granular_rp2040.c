/*
 * Q15 Granular Delay with Freeze for RP2040
 *
 * This effect continuously records audio into a circular buffer and plays back
 * short, overlapping "grains" of audio. The `BOOTSEL` button can be used to
 * "freeze" the audio in the buffer, creating a sustained textural drone.
 *
 * - Granular Engine: A set of "grains" are scheduled to play back from the
 *   circular buffer. Each grain has its own position, length, and envelope.
 * - Freeze Effect: When enabled, the buffer stops recording new audio, and
 *   the grains are looped from the captured audio segment.
 * - Dry/Wet Mix: The original (dry) signal is mixed with the granular (wet)
 *   signal.
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

// Q15 macros
#define F32_Q15(x) ((int16_t)((x) * 32767.f + 0.5f))
#define FX_MUL(a, b) ((int16_t)(((int32_t)(a) * (b)) >> 15))

// Buffer and Grain settings
#define BUFFER_SIZE (16384)
#define NUM_GRAINS 8
static int16_t grain_length = 2048;
static int16_t grain_density = 8;
#define GRAIN_FADE 512

// Parameters
static int16_t wet_mix = F32_Q15(0.5f);
static int16_t dry_mix = F32_Q15(0.5f);
static bool freeze_enabled = false;

// Buffer
static int16_t buffer[BUFFER_SIZE];
static uint32_t write_pos = 0;

// Grains
typedef struct {
    uint32_t pos;
    uint32_t counter;
    bool active;
} grain_t;

static grain_t grains[NUM_GRAINS];

// Initialize the effect
void fx_init(void) {
    memset(buffer, 0, sizeof(buffer));
    for (int i = 0; i < NUM_GRAINS; i++) {
        grains[i].active = false;
    }
}

const char *fx_name(void) { return "Granular Freeze"; }

void fx_set_format(uint8_t ch, uint32_t sr) {
    // No-op
}

void fx_set_enable(bool en) {
    freeze_enabled = en;
}

void fx_set_param(uint8_t id, int16_t val) {
    switch (id) {
        case FX_PARAM_WET_MIX:
            wet_mix = val;
            break;
        case FX_PARAM_DRY_MIX:
            dry_mix = val;
            break;
        case FX_PARAM_GRAIN_LENGTH:
            grain_length = 256 + (val >> 1); // Scale to a reasonable range
            break;
        case FX_PARAM_GRAIN_DENSITY:
            grain_density = 1 + (val >> 11); // Scale to a reasonable range (1-8)
            break;
        default:
            break;
    }
}

void fx_process(int32_t *out, int32_t *in, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        int16_t in_l = in[2 * i] >> 16;
        int16_t in_r = in[2 * i + 1] >> 16;

        // Write to buffer (if not frozen)
        if (!freeze_enabled) {
            buffer[write_pos] = (in_l + in_r) >> 1; // Mono-ize
            write_pos = (write_pos + 1) % BUFFER_SIZE;
        }

        // Process grains
        int32_t wet_sample = 0;
        for (int j = 0; j < grain_density; j++) {
            if (grains[j].active) {
                // Envelope
                int16_t envelope = F32_Q15(1.0f);
                if (grains[j].counter < GRAIN_FADE) {
                    envelope = F32_Q15((float)grains[j].counter / GRAIN_FADE);
                } else if (grains[j].counter > grain_length - GRAIN_FADE) {
                    envelope = F32_Q15((float)(grain_length - grains[j].counter) / GRAIN_FADE);
                }

                // Read from buffer
                uint32_t read_pos = (grains[j].pos + grains[j].counter) % BUFFER_SIZE;
                wet_sample += FX_MUL(buffer[read_pos], envelope);

                // Increment counter and deactivate if finished
                grains[j].counter++;
                if (grains[j].counter >= grain_length) {
                    grains[j].active = false;
                }
            } else {
                // Activate a new grain
                grains[j].active = true;
                grains[j].counter = 0;
                grains[j].pos = (write_pos + (rand() % (BUFFER_SIZE - grain_length))) % BUFFER_SIZE;
            }
        }

        int16_t wet_out = sat16(wet_sample);

        // Mix
        int16_t out_l = FX_MUL(in_l, dry_mix) + FX_MUL(wet_out, wet_mix);
        int16_t out_r = FX_MUL(in_r, dry_mix) + FX_MUL(wet_out, wet_mix);

        out[2 * i] = (int32_t)out_l << 16;
        out[2 * i + 1] = (int32_t)out_r << 16;
    }
}
