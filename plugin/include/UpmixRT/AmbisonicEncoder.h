#pragma once

#include "Constants.h"
#include "Decorrelator.h"

namespace audio_plugin {

class AmbisonicEncoder {
public:
    void prepare(double sampleRate);
    void reset();

    // Encode a stereo sample pair into first-order B-format.
    // bFormat[4] = {W, X, Y, Z}
    void encode(float inputL, float inputR,
                const SpatialParams& params,
                float* bFormat);

private:
    Decorrelator decorrX_;
    Decorrelator decorrZ_;
};

}  // namespace audio_plugin
