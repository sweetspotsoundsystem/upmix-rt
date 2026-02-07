#include <UpmixRT/Decorrelator.h>
#include <cmath>

namespace audio_plugin {

void Decorrelator::prepare(double sampleRate, const int* delays, int numStages) {
    numStages_ = numStages;
    float ratio = static_cast<float>(sampleRate) / kDecorrRefSampleRate;

    for (int i = 0; i < numStages_; ++i) {
        int scaledDelay = std::max(1, static_cast<int>(std::round(
            static_cast<float>(delays[i]) * ratio)));
        stages_[static_cast<size_t>(i)].delaySamples = scaledDelay;
        stages_[static_cast<size_t>(i)].buffer.resize(static_cast<size_t>(scaledDelay), 0.0f);
        stages_[static_cast<size_t>(i)].writePos = 0;
    }
}

void Decorrelator::reset() {
    for (int i = 0; i < numStages_; ++i) {
        auto& stage = stages_[static_cast<size_t>(i)];
        std::fill(stage.buffer.begin(), stage.buffer.end(), 0.0f);
        stage.writePos = 0;
    }
}

float Decorrelator::process(float input) {
    float signal = input;

    for (int i = 0; i < numStages_; ++i) {
        auto& stage = stages_[static_cast<size_t>(i)];

        // Read delayed sample
        float delayed = stage.buffer[static_cast<size_t>(stage.writePos)];

        // Allpass: y[n] = -coeff * x[n] + delayed + coeff * delayed_prev
        // Standard form: y[n] = delayed + coeff * (x[n] - delayed)
        // Wait, standard allpass is: y = -g*x + x_delayed + g*y_delayed
        // Using the canonical form with the delay buffer storing the feedback:
        float output = -kAllpassCoeff * signal + delayed;
        float toWrite = signal + kAllpassCoeff * output;

        stage.buffer[static_cast<size_t>(stage.writePos)] = toWrite;
        stage.writePos = (stage.writePos + 1) % stage.delaySamples;

        signal = output;
    }

    return signal;
}

}  // namespace audio_plugin
