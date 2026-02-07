#pragma once

#include <array>
#include <vector>
#include "Constants.h"

namespace audio_plugin {

class Decorrelator {
public:
    // delays: pointer to array of delay values (in samples @ kDecorrRefSampleRate)
    // numStages: number of allpass stages
    void prepare(double sampleRate, const int* delays, int numStages);
    void reset();

    float process(float input);

private:
    struct AllpassStage {
        std::vector<float> buffer;
        int writePos = 0;
        int delaySamples = 0;
    };

    std::array<AllpassStage, 2> stages_;
    int numStages_ = 0;
};

}  // namespace audio_plugin
