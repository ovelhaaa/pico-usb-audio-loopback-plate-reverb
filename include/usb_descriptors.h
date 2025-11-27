#pragma once

#include <stddef.h>
#include <stdint.h>

#define UAC2_ENTITY_CLOCK               0x04
#define UAC2_ENTITY_SPK_INPUT_TERMINAL  0x01
#define UAC2_ENTITY_SPK_OUTPUT_TERMINAL 0x03
#define UAC2_ENTITY_MIC_INPUT_TERMINAL  0x11
#define UAC2_ENTITY_MIC_OUTPUT_TERMINAL 0x13

enum
{
  ITF_NUM_AUDIO_CONTROL = 0,
  ITF_NUM_AUDIO_STREAMING_SPK,
  ITF_NUM_AUDIO_STREAMING_MIC,
  ITF_NUM_MIDI,
  ITF_NUM_MIDI_STREAMING,
  ITF_NUM_TOTAL
};

#define EPNUM_MIDI_OUT 0x02
#define EPNUM_MIDI_IN 0x02

void usb_audio_get_config(uint32_t *sampling_rate, uint8_t *bit_rate, uint8_t *channels);
size_t usb_audio_frame_size(void);
size_t usb_audio_frame_num(size_t packet_size);
