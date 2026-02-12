#pragma once

#include "Constants.h"

namespace audio_plugin {

class OutputWriter {
public:
    void prepare(double sampleRate);
    void reset();

    // Write one sample to the output buffer.
    // speakerOutputs: decoded multichannel (up to kMaxOutputChannels).
    // Contract: decoder fills all kMaxOutputChannels each sample
    // (active layout channels + zero/fade tail for the rest).
    // dryL, dryR: original stereo input (always written to main out 1-2)
    // dryWetTarget: target wet level [0..1] for aux outputs (3+)
    // gainDbTarget: wet aux output gain (dB), smoothed sample-by-sample
    // numOutputChannels: actual output channel count
    // outputPtrs: array of pointers to output channel buffers
    // sampleIndex: sample position in current buffer
    void writeSample(const float* speakerOutputs,
                     float dryL, float dryR,
                     float dryWetTarget,
                     float gainDbTarget,
                     int numOutputChannels,
                     float** outputPtrs,
                     int sampleIndex);

private:
    float smoothedDryWet_ = 1.0f;
    float dryWetAlpha_ = 0.0f;
    float smoothedGainDb_ = 0.0f;
    float gainAlpha_ = 0.0f;
};

}  // namespace audio_plugin
