#include <UpmixRT/AmbisonicEncoder.h>
#include <algorithm>
#include <cmath>

namespace audio_plugin {

void AmbisonicEncoder::prepare(double sampleRate) {
    decorrX_.prepare(sampleRate, kDecorrDelaysX, 2);
    decorrZ_.prepare(sampleRate, kDecorrDelaysZ, 2);
}

void AmbisonicEncoder::reset() {
    decorrX_.reset();
    decorrZ_.reset();
}

void AmbisonicEncoder::encode(float inputL, float inputR,
                               const SpatialParams& params,
                               float* bFormat) {
    float mid = (inputL + inputR) * 0.5f;
    float side = (inputL - inputR) * 0.5f;

    // Phaseless W and Y (exact reconstruction)
    bFormat[BFormat::W] = (inputL + inputR) * kInvSqrt2;
    bFormat[BFormat::Y] = (inputL - inputR) * kInvSqrt2;

    // Front-back enrichment (X)
    float iccSafe = std::clamp(params.icc, 0.0f, 1.0f);
    float iccSqrt = std::sqrt(iccSafe);

    float xDirect = mid * iccSqrt * std::cos(params.azimuth) * 0.5f;
    float zDirect = mid * params.elevation * iccSqrt;

    // Diffuse component (decorrelated, added to X and Z only)
    float diffuseSignal = side * params.diffuseness;
    float diffuseSpread = 0.5f;
    float xDiffuse = decorrX_.process(diffuseSignal) * diffuseSpread;
    float zDiffuse = decorrZ_.process(diffuseSignal) * diffuseSpread;

    bFormat[BFormat::X] = xDirect + xDiffuse;
    bFormat[BFormat::Z] = zDirect + zDiffuse;
}

}  // namespace audio_plugin
