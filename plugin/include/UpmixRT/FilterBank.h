#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Constants.h"

namespace audio_plugin {

class FilterBank {
public:
    void prepare(double sampleRate);
    void reset();

    // Splits a stereo sample into kNumBands frequency bands (analysis only).
    // bandL[b] and bandR[b] receive the per-band L/R samples.
    void process(float inputL, float inputR,
                 float* bandL, float* bandR);

private:
    struct CrossoverStage {
        juce::dsp::IIR::Filter<float> lpL, hpL, lpR, hpR;
    };

    CrossoverStage stages_[kNumCrossovers];
    double sampleRate_ = 48000.0;
};

}  // namespace audio_plugin
