#include <iostream>
#include <vector>
#include <cassert>
#include "reverb.h"

int main() {
    const int sampleRate = 48000;
    const int numSamples = sampleRate; // 1 second of audio
    Reverb reverb(sampleRate);

    std::vector<float> left(numSamples, 0.0f);
    std::vector<float> right(numSamples, 0.0f);

    // Create an impulse
    left[0] = 1.0f;
    right[0] = 1.0f;

    std::vector<float> left_original = left;
    std::vector<float> right_original = right;

    reverb.process(left.data(), right.data(), numSamples);

    bool output_is_different = false;
    for(int i = 1; i < numSamples; ++i) {
        if (left[i] != left_original[i] || right[i] != right_original[i]) {
            output_is_different = true;
            break;
        }
    }

    assert(output_is_different);

    std::cout << "Test passed: Reverb processed the audio." << std::endl;

    return 0;
}
