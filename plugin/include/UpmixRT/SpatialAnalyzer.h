#pragma once

#include "Constants.h"
#include "FilterBank.h"
#include "AnalysisBand.h"
#include "HeightEstimator.h"

namespace audio_plugin {

class SpatialAnalyzer {
public:
    void prepare(double sampleRate);
    void reset();

    // Process one stereo sample pair, return aggregated spatial parameters.
    SpatialParams process(float inputL, float inputR);

private:
    FilterBank filterBank_;
    AnalysisBand bands_[kNumBands];
    HeightEstimator heightEstimator_;
};

}  // namespace audio_plugin
