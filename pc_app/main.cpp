#include <iostream>
#include <vector>
#include "AudioFile.h"
#include "reverb.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.wav> <output.wav>" << std::endl;
        return 1;
    }

    const char* inputFilePath = argv[1];
    const char* outputFilePath = argv[2];

    AudioFile<float> audioFile;
    if (!audioFile.load(inputFilePath)) {
        std::cerr << "Error: Could not load input file " << inputFilePath << std::endl;
        return 1;
    }

    int sampleRate = audioFile.getSampleRate();
    int numChannels = audioFile.getNumChannels();

    std::cout << "Input file loaded successfully." << std::endl;
    std::cout << "Sample Rate: " << sampleRate << std::endl;
    std::cout << "Channels: " << numChannels << std::endl;

    if (numChannels < 1 || numChannels > 2) {
        std::cerr << "Error: This application currently only supports mono or stereo WAV files." << std::endl;
        return 1;
    }

    // If mono, duplicate the channel
    if (numChannels == 1) {
        audioFile.samples.push_back(audioFile.samples[0]);
        audioFile.setNumChannels(2);
        std::cout << "Mono file detected. Duplicating channel to create stereo." << std::endl;
    }

    Reverb reverb(static_cast<float>(sampleRate));

    // Get the audio buffers
    std::vector<float>& leftChannel = audioFile.samples[0];
    std::vector<float>& rightChannel = audioFile.samples[1];

    std::cout << "Applying reverb..." << std::endl;
    reverb.process(leftChannel.data(), rightChannel.data(), audioFile.getNumSamplesPerChannel());

    if (!audioFile.save(outputFilePath)) {
        std::cerr << "Error: Could not save output file " << outputFilePath << std::endl;
        return 1;
    }

    std::cout << "Reverb applied and output file saved successfully to " << outputFilePath << std::endl;

    return 0;
}
