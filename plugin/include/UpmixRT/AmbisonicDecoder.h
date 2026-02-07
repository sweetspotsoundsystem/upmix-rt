#pragma once

#include <array>
#include <juce_dsp/juce_dsp.h>
#include "Constants.h"
#include "SpeakerLayout.h"

namespace audio_plugin {

class AmbisonicDecoder {
public:
    void prepare(double sampleRate, SpeakerLayout layout);
    void reset();

    // Decode B-format to speaker feeds.
    // bFormat[4] = {W, X, Y, Z}
    // speakerOutputs: at least kMaxOutputChannels floats
    void decode(const float* bFormat, SpeakerLayout layout,
                float* speakerOutputs);

private:
    void updateLayout(SpeakerLayout layout);

    SpeakerLayout currentLayout_ = SpeakerLayout::Surround51;
    SpeakerLayout prevLayout_ = SpeakerLayout::Surround51;
    float crossfadeProgress_ = 1.0f;  // 1.0 = fully transitioned
    float crossfadeStep_ = 0.0f;

    // Current and previous decoder matrices (flat, row-major)
    std::array<float, kMaxOutputChannels * kNumAmbiChannels> currentMatrix_{};
    std::array<float, kMaxOutputChannels * kNumAmbiChannels> prevMatrix_{};

    // LFE lowpass filter (2nd-order Butterworth)
    juce::dsp::IIR::Filter<float> lfeFilter_;
    int lfeChannelIndex_ = -1;
    double sampleRate_ = 48000.0;
};

}  // namespace audio_plugin
