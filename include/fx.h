#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "usb_descriptors.h"
#include "fx_param.h"

const char *fx_name(void);
void fx_init(void);
void fx_set_format(uint8_t bit_rate, uint32_t sampling_rate);
void fx_set_enable(bool enable);
void fx_process(int32_t *output, int32_t *input, size_t frame_length);
void fx_set_param(uint8_t id, int16_t val);
