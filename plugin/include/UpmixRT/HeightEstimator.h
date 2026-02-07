#pragma once

#include "Constants.h"

namespace audio_plugin {

class HeightEstimator {
public:
    void prepare(double sampleRate);
    void reset();

    // Estimate height/elevation from per-band energies.
    // bandEnergies: array of kNumBands energy values.
    // Returns elevation factor in [0, kHeightMaxElevation].
    float process(const float* bandEnergies);

private:
    float smoothedElevation_ = 0.0f;
    float alpha_ = 0.0f;
};

}  // namespace audio_plugin
