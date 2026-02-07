#include <UpmixRT/SpatialAnalyzer.h>
#include <algorithm>
#include <cmath>

namespace audio_plugin {

void SpatialAnalyzer::prepare(double sampleRate) {
    filterBank_.prepare(sampleRate);
    for (auto& band : bands_)
        band.prepare(sampleRate);
    heightEstimator_.prepare(sampleRate);
}

void SpatialAnalyzer::reset() {
    filterBank_.reset();
    for (auto& band : bands_)
        band.reset();
    heightEstimator_.reset();
}

SpatialParams SpatialAnalyzer::process(float inputL, float inputR) {
    float bandL[kNumBands];
    float bandR[kNumBands];

    filterBank_.process(inputL, inputR, bandL, bandR);

    float totalEnergy = kEpsilon;
    float weightedICC = 0.0f;
    float weightedAzimuth = 0.0f;
    float bandEnergies[kNumBands];

    for (int b = 0; b < kNumBands; ++b) {
        BandAnalysis result = bands_[b].process(bandL[b], bandR[b]);
        bandEnergies[b] = result.energy;
        totalEnergy += result.energy;
        weightedICC += result.energy * result.icc;
        weightedAzimuth += result.energy * result.azimuth;
    }

    float icc = weightedICC / totalEnergy;
    float azimuth = weightedAzimuth / totalEnergy;
    float diffuseness = std::sqrt(1.0f - std::clamp(icc, 0.0f, 1.0f));
    float elevation = heightEstimator_.process(bandEnergies);

    return SpatialParams{icc, azimuth, diffuseness, elevation};
}

}  // namespace audio_plugin
