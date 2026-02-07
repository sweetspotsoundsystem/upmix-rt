#pragma once

#include "Constants.h"

namespace audio_plugin {

class AnalysisBand {
public:
    void prepare(double sampleRate);
    void reset();

    // Process one sample pair for this band, update smoothed parameters.
    BandAnalysis process(float bandL, float bandR);

private:
    float iccSmooth_ = 0.0f;
    float azimuthSmooth_ = 0.0f;
    float energySmooth_ = 0.0f;

    // EMA coefficients (computed from sample rate)
    float iccAlpha_ = 0.0f;
    float azimuthAlpha_ = 0.0f;
    float energyAlpha_ = 0.0f;

    // Running accumulators for ICC computation
    float smoothLL_ = 0.0f;
    float smoothRR_ = 0.0f;
    float smoothLR_ = 0.0f;
};

}  // namespace audio_plugin
