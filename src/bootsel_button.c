/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "bootsel_button.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

void bb_init(void) {
    gpio_init(BOOTSEL_BUTTON_GPIO_PIN);
    gpio_set_dir(BOOTSEL_BUTTON_GPIO_PIN, GPIO_IN);
    gpio_pull_up(BOOTSEL_BUTTON_GPIO_PIN);
}

bool bb_get_bootsel_button(void) {
    return !gpio_get(BOOTSEL_BUTTON_GPIO_PIN);
}
