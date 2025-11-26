#include "reverb.h"
#include <cmath>
#include <vector>

// Constants adapted from the original C file
namespace {
    const int NUM_COMB = 8;
    const int NUM_AP = 4;
    const float WET = 0.50f;
    const float DRY = 0.50f;
    const float DAMP = 0.40f;
    const float ALLPASS_GAIN = 0.50f;
    const float MASTER_GAIN = 1.50f;
    const float DESIGN_SAMPLE_RATE = 48000.0f;

    // Delay lengths for 48kHz sample rate
    const int COMB_DLY_L_48k[] = {509, 863, 1481, 2521, 4273, 7253, 10007, 15013};
    const int COMB_DLY_R_48k[] = {523, 877, 1489, 2531, 4283, 7283, 10037, 15031};
    const int AP_DLY_L_48k[] = {142, 396, 1071, 3079};
    const int AP_DLY_R_48k[] = {145, 399, 1073, 3081};
    const float COMB_T60[] = {0.25f, 0.30f, 0.40f, 0.80f, 2.00f, 6.00f, 10.00f, 20.00f};
    const float COMB_GAIN[] = {0.62f, 0.60f, 0.58f, 0.55f, 0.52f, 0.50f, 0.48f, 0.45f};
}

Reverb::Reverb(float sample_rate) : sample_rate_(sample_rate), enabled_(true), predelay_index_(0) {
    init_filters();
}

Reverb::~Reverb() {
    // std::vector will handle memory deallocation
}

void Reverb::init_filters() {
    float sr_ratio = sample_rate_ / DESIGN_SAMPLE_RATE;

    // Pre-delay buffers
    int predelay_samples = static_cast<int>(960 * sr_ratio);
    predelay_buffer_left_.assign(predelay_samples, 0.0f);
    predelay_buffer_right_.assign(predelay_samples, 0.0f);

    // Comb filters
    combs_left_.resize(NUM_COMB);
    combs_right_.resize(NUM_COMB);
    for (int i = 0; i < NUM_COMB; ++i) {
        int comb_delay_l = static_cast<int>(COMB_DLY_L_48k[i] * sr_ratio);
        int comb_delay_r = static_cast<int>(COMB_DLY_R_48k[i] * sr_ratio);
        combs_left_[i].buffer.assign(comb_delay_l, 0.0f);
        combs_left_[i].index = 0;
        combs_left_[i].damp = DAMP;
        combs_left_[i].last = 0.0f;

        combs_right_[i].buffer.assign(comb_delay_r, 0.0f);
        combs_right_[i].index = 0;
        combs_right_[i].damp = DAMP;
        combs_right_[i].last = 0.0f;

        // Calculate feedback
        float g = powf(10.f, -3.f * comb_delay_l / sample_rate_ / COMB_T60[i]);
        combs_left_[i].feedback = (g > 0.98f) ? 0.98f : g;
        g = powf(10.f, -3.f * comb_delay_r / sample_rate_ / COMB_T60[i]);
        combs_right_[i].feedback = (g > 0.98f) ? 0.98f : g;
    }

    // All-pass filters
    allpasses_left_.resize(NUM_AP);
    allpasses_right_.resize(NUM_AP);
    for (int i = 0; i < NUM_AP; ++i) {
        int ap_delay_l = static_cast<int>(AP_DLY_L_48k[i] * sr_ratio);
        int ap_delay_r = static_cast<int>(AP_DLY_R_48k[i] * sr_ratio);
        allpasses_left_[i].buffer.assign(ap_delay_l, 0.0f);
        allpasses_left_[i].index = 0;
        allpasses_right_[i].buffer.assign(ap_delay_r, 0.0f);
        allpasses_right_[i].index = 0;
    }
}

void Reverb::set_enabled(bool enabled) {
    enabled_ = enabled;
}

void Reverb::process(float* in_left, float* in_right, int num_samples) {
    if (!enabled_) {
        return;
    }

    for (int i = 0; i < num_samples; ++i) {
        float dry_left = in_left[i];
        float dry_right = in_right[i];

        // Pre-delay
        float pre_left = predelay_buffer_left_[predelay_index_];
        float pre_right = predelay_buffer_right_[predelay_index_];
        predelay_buffer_left_[predelay_index_] = dry_left;
        predelay_buffer_right_[predelay_index_] = dry_right;
        if (++predelay_index_ >= predelay_buffer_left_.size()) {
            predelay_index_ = 0;
        }

        // Comb filters
        float wet_left = 0, wet_right = 0;
        for (int j = 0; j < NUM_COMB; ++j) {
            // Left channel
            float delayed_l = combs_left_[j].buffer[combs_left_[j].index];
            float feedback_l = delayed_l * combs_left_[j].feedback;
            combs_left_[j].last = combs_left_[j].last * (1.0f - combs_left_[j].damp) + feedback_l * combs_left_[j].damp;
            combs_left_[j].buffer[combs_left_[j].index] = pre_left + combs_left_[j].last;
            if (++combs_left_[j].index >= combs_left_[j].buffer.size()) combs_left_[j].index = 0;
            wet_left += delayed_l * COMB_GAIN[j];

            // Right channel
            float delayed_r = combs_right_[j].buffer[combs_right_[j].index];
            float feedback_r = delayed_r * combs_right_[j].feedback;
            combs_right_[j].last = combs_right_[j].last * (1.0f - combs_right_[j].damp) + feedback_r * combs_right_[j].damp;
            combs_right_[j].buffer[combs_right_[j].index] = pre_right + combs_right_[j].last;
            if (++combs_right_[j].index >= combs_right_[j].buffer.size()) combs_right_[j].index = 0;
            wet_right += delayed_r * COMB_GAIN[j];
        }

        // All-pass filters
        for (int j = 0; j < NUM_AP; ++j) {
            // Left channel
            float delayed_l = allpasses_left_[j].buffer[allpasses_left_[j].index];
            float output_l = delayed_l - wet_left * ALLPASS_GAIN;
            allpasses_left_[j].buffer[allpasses_left_[j].index] = wet_left + output_l * ALLPASS_GAIN;
            if (++allpasses_left_[j].index >= allpasses_left_[j].buffer.size()) allpasses_left_[j].index = 0;
            wet_left = output_l;

            // Right channel
            float delayed_r = allpasses_right_[j].buffer[allpasses_right_[j].index];
            float output_r = delayed_r - wet_right * ALLPASS_GAIN;
            allpasses_right_[j].buffer[allpasses_right_[j].index] = wet_right + output_r * ALLPASS_GAIN;
            if (++allpasses_right_[j].index >= allpasses_right_[j].buffer.size()) allpasses_right_[j].index = 0;
            wet_right = output_r;
        }

        // Mix and output
        in_left[i] = (dry_left * DRY + wet_left * WET) * MASTER_GAIN;
        in_right[i] = (dry_right * DRY + wet_right * WET) * MASTER_GAIN;
    }
}
