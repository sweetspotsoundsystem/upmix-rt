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
    // Smooth dry/wet parameter
    smoothedDryWet_ += dryWetAlpha_ * (dryWetTarget - smoothedDryWet_);
    float wet = smoothedDryWet_;
    float dry = 1.0f - wet;

    // Smooth gain parameter (in dB domain) and convert to linear
    smoothedGainDb_ += gainAlpha_ * (gainDbTarget - smoothedGainDb_);
    float gainLinear = std::pow(10.0f, smoothedGainDb_ / 20.0f);

    for (int ch = 0; ch < numOutputChannels; ++ch) {
        float wetSample = speakerOutputs[ch];

        // Front L/R get dry signal blended in
        float sample;
        if (ch == 0) {
            sample = wet * wetSample + dry * dryL;
        } else if (ch == 1) {
            sample = wet * wetSample + dry * dryR;
        } else {
            sample = wet * wetSample;
        }

        outputPtrs[ch][sampleIndex] = sample * gainLinear;
    }
}

}  // namespace audio_plugin
