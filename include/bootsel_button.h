/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once
#include <stdbool.h>

#define BOOTSEL_BUTTON_GPIO_PIN 23

void bb_init(void);
bool bb_get_bootsel_button(void);
