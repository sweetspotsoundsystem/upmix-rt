#include <UpmixRT/FilterBank.h>

namespace audio_plugin {

void FilterBank::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    for (int i = 0; i < kNumCrossovers; ++i) {
        auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, kCrossoverFreqs[i], 0.5f);
        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, kCrossoverFreqs[i], 0.5f);

        stages_[i].lpL.coefficients = lpCoeffs;
        stages_[i].hpL.coefficients = hpCoeffs;
        stages_[i].lpR.coefficients = lpCoeffs;
        stages_[i].hpR.coefficients = hpCoeffs;
    }
    reset();
}

void FilterBank::reset() {
    for (auto& stage : stages_)  {
        stage.lpL.reset();
        stage.hpL.reset();
        stage.lpR.reset();
        stage.hpR.reset();
    }
}

void FilterBank::process(float inputL, float inputR,
                         float* bandL, float* bandR) {
    // Cascade: at each crossover, LP output goes to current band,
    // HP output continues to the next stage.
    float remL = inputL;
    float remR = inputR;

    for (int i = 0; i < kNumCrossovers; ++i) {
        bandL[i] = stages_[i].lpL.processSample(remL);
        bandR[i] = stages_[i].lpR.processSample(remR);
        remL = stages_[i].hpL.processSample(remL);
        remR = stages_[i].hpR.processSample(remR);
    }

    // Last band gets the remainder (everything above the highest crossover)
    bandL[kNumBands - 1] = remL;
    bandR[kNumBands - 1] = remR;
}

}  // namespace audio_plugin
