#include <UpmixRT/OutputWriter.h>
#include <cmath>

namespace audio_plugin {

void OutputWriter::prepare(double sampleRate) {
    dryWetAlpha_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * kDryWetSmoothTimeSec));
    smoothedDryWet_ = 1.0f;
    gainAlpha_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * kGainSmoothTimeSec));
    smoothedGainDb_ = 0.0f;
}

void OutputWriter::reset() {
    smoothedDryWet_ = 1.0f;
    smoothedGainDb_ = 0.0f;
}

void OutputWriter::writeSample(const float* speakerOutputs,
                                float dryL, float dryR,
                                float dryWetTarget,
                                float gainDbTarget,
                                int numOutputChannels,
                                float** outputPtrs,
                                int sampleIndex) {
    // Smooth dry/wet parameter for aux wet outputs
    smoothedDryWet_ += dryWetAlpha_ * (dryWetTarget - smoothedDryWet_);
    float wet = smoothedDryWet_;

    // Smooth gain parameter (in dB domain) and convert to linear
    smoothedGainDb_ += gainAlpha_ * (gainDbTarget - smoothedGainDb_);
    float gainLinear = std::pow(10.0f, smoothedGainDb_ / 20.0f);

    // Main stereo out (1-2) is always dry passthrough and never gain-scaled.
    if (numOutputChannels > 0) {
        outputPtrs[0][sampleIndex] = dryL;
    }
    if (numOutputChannels > 1) {
        outputPtrs[1][sampleIndex] = dryR;
    }

    // Upmix appears only on multi-out aux channels (3+), with dry/wet and gain.
    for (int ch = 2; ch < numOutputChannels; ++ch) {
        int wetChannel = ch - 2;
        outputPtrs[ch][sampleIndex] = wet * speakerOutputs[wetChannel] * gainLinear;
    }
}

}  // namespace audio_plugin
