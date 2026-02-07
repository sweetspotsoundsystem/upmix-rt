#include <UpmixRT/AnalysisBand.h>
#include <algorithm>
#include <cmath>

namespace audio_plugin {

void AnalysisBand::prepare(double sampleRate) {
    auto computeAlpha = [&](float timeSec) -> float {
        if (timeSec <= 0.0f) return 1.0f;
        return 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * timeSec));
    };

    iccAlpha_ = computeAlpha(kICCSmoothingTimeSec);
    azimuthAlpha_ = computeAlpha(kAzimuthSmoothingTimeSec);
    energyAlpha_ = computeAlpha(kEnergySmoothingTimeSec);
    reset();
}

void AnalysisBand::reset() {
    iccSmooth_ = 0.0f;
    azimuthSmooth_ = 0.0f;
    energySmooth_ = 0.0f;
    smoothLL_ = 0.0f;
    smoothRR_ = 0.0f;
    smoothLR_ = 0.0f;
}

BandAnalysis AnalysisBand::process(float bandL, float bandR) {
    float mid = (bandL + bandR) * 0.5f;
    float side = (bandL - bandR) * 0.5f;

    // Energy
    float energy = bandL * bandL + bandR * bandR;
    energySmooth_ += energyAlpha_ * (energy - energySmooth_);

    // ICC via smoothed correlations
    smoothLL_ += iccAlpha_ * (bandL * bandL - smoothLL_);
    smoothRR_ += iccAlpha_ * (bandR * bandR - smoothRR_);
    smoothLR_ += iccAlpha_ * (bandL * bandR - smoothLR_);

    float denom = std::sqrt(smoothLL_ * smoothRR_);
    float icc = (denom > kEpsilon) ? (smoothLR_ / denom) : 0.0f;
    icc = std::clamp(icc, 0.0f, 1.0f);
    iccSmooth_ += iccAlpha_ * (icc - iccSmooth_);

    // Azimuth: atan2(|R|-|L|, |R|+|L|) * pi/2
    float absL = std::abs(bandL);
    float absR = std::abs(bandR);
    float azSum = absR + absL;
    float azDiff = absR - absL;
    float azimuth = (azSum > kEpsilon)
        ? std::atan2(azDiff, azSum) * 2.0f
        : 0.0f;
    azimuthSmooth_ += azimuthAlpha_ * (azimuth - azimuthSmooth_);

    return BandAnalysis{
        mid,
        side,
        iccSmooth_,
        azimuthSmooth_,
        energySmooth_
    };
}

}  // namespace audio_plugin
