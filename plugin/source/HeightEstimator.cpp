#include <UpmixRT/HeightEstimator.h>
#include <cmath>

namespace audio_plugin {

void HeightEstimator::prepare(double sampleRate) {
    // Use a smoothing time similar to energy smoothing
    float timeSec = 0.010f;
    alpha_ = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * timeSec));
    reset();
}

void HeightEstimator::reset() {
    smoothedElevation_ = 0.0f;
}

float HeightEstimator::process(const float* bandEnergies) {
    float totalEnergy = kEpsilon;
    float hfEnergy = 0.0f;

    for (int b = 0; b < kNumBands; ++b) {
        totalEnergy += bandEnergies[b];
        if (b >= kHeightHFBandStart) {
            hfEnergy += bandEnergies[b];
        }
    }

    float hfRatio = hfEnergy / totalEnergy;
    float elevation = hfRatio * kHeightMaxElevation;

    smoothedElevation_ += alpha_ * (elevation - smoothedElevation_);
    return smoothedElevation_;
}

}  // namespace audio_plugin
