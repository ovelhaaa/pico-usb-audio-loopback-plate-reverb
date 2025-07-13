/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bootsel_button.h"
#include "bsp/board_api.h"
#include "fx.h"
#include "hardware/clocks.h"
#include "led.h"
#include "pico/stdlib.h"
#include "ringbuffer.h"
#include "tusb.h"
#include "usb_descriptors.h"

static ringbuffer_t rx_buffer = {0};
static ringbuffer_t tx_buffer = {0};

static int32_t scratch_in[64 * sizeof(int32_t) * 2];
static int32_t scratch_out[64 * sizeof(int32_t) * 2];
static uint64_t frac_acc = 0;

static int8_t silence_buf[AUDIO_FRAME_BYTES] = {0};
static uint16_t current_sampling_rate = 48000;
static const size_t FRAME_LENGTH = 48;

#define FX_TIME_LOG_COUNT 1000
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

static uint32_t fx_time_samples[FX_TIME_LOG_COUNT];
static int fx_time_index = 0;
static bool fx_log_ready = false;
static char scratch[1024];

void audio_task(void) {
    uint32_t sampling_rate;
    uint8_t bit_rate;
    uint8_t channels;
    usb_audio_get_config(&sampling_rate, &bit_rate, &channels);
    if (sampling_rate != current_sampling_rate) {
        current_sampling_rate = sampling_rate;
        return;
    }

    fx_set_enable(bb_get_bootsel_button());

    const size_t rx_samples = FRAME_LENGTH * 2;
    const size_t frame_bytes = FRAME_LENGTH * sizeof(int32_t) * 2;
    if (ringbuffer_size(&rx_buffer) >= rx_samples) {
        ringbuffer_pop(&rx_buffer, scratch_in, rx_samples);

        fx_process(scratch_out, scratch_in, FRAME_LENGTH);

        ringbuffer_push(&tx_buffer, scratch_out, rx_samples);
    }
}

void led_task(void) { led_update(); }

bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id,
                                   uint8_t ep_out, uint8_t cur_alt_setting) {
    if (ringbuffer_capacity(&rx_buffer) == 0)
        return true;
    uint16_t rx_size = tud_audio_read(scratch_in, n_bytes_received);
    if (rx_size != n_bytes_received)
        return true;
    ringbuffer_push(&rx_buffer, scratch_in, rx_size / sizeof(int32_t));
    return true;
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in,
                                   uint8_t cur_alt_setting) {
    (void)rhport;
    (void)itf;
    (void)ep_in;
    (void)cur_alt_setting;

    uint32_t sampling_rate;
    uint8_t bit_rate;
    uint8_t channels;
    usb_audio_get_config(&sampling_rate, &bit_rate, &channels);

    const uint32_t USB_SOF_HZ = 1000;
    frac_acc += (uint64_t)current_sampling_rate;
    uint32_t frames = frac_acc / USB_SOF_HZ;
    frac_acc %= USB_SOF_HZ;
    const size_t samples_needed = frames * channels;
    if (samples_needed == 0)
        return true;

    size_t have = ringbuffer_size(&tx_buffer);
    size_t to_copy = (have < samples_needed) ? have : samples_needed;
    if (to_copy > 0) {
        ringbuffer_pop(&tx_buffer, scratch_out, to_copy);
    }
    if (to_copy < samples_needed) {
        if (to_copy >= channels && to_copy > 0) {
            size_t last_frame_start = to_copy - channels;
            for (size_t i = to_copy; i < samples_needed; i += channels) {
                for (uint8_t ch = 0; ch < channels; ch++) {
                    if (i + ch < samples_needed) {
                        scratch_out[i + ch] = scratch_out[last_frame_start + ch];
                    }
                }
            }
        } else {
            memset(scratch_out + to_copy, 0, (samples_needed - to_copy) * sizeof(int32_t));
        }
    }

    tud_audio_write(scratch_out, (uint16_t)(samples_needed * sizeof(int32_t)));

    return true;
}

int main(void) {
    set_sys_clock_khz(240000, true);

    board_init();
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
    board_init_after_tusb();

    fx_init();

    while (1) {
        tud_task();
        audio_task();
        led_task();
    }
}
