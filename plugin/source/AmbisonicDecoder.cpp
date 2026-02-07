#include <UpmixRT/AmbisonicDecoder.h>
#include <cstring>
#include <cmath>

namespace audio_plugin {

void AmbisonicDecoder::prepare(double sampleRate, SpeakerLayout layout) {
    sampleRate_ = sampleRate;

    // LFE filter: 2nd-order Butterworth LP at 120Hz
    auto lfeCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        sampleRate, kLFECutoffHz);
    lfeFilter_.coefficients = lfeCoeffs;

    crossfadeProgress_ = 1.0f;
    crossfadeStep_ = 1.0f / (static_cast<float>(sampleRate) * kLayoutCrossfadeTimeSec);

    currentLayout_ = layout;
    prevLayout_ = layout;
    updateLayout(layout);
}

void AmbisonicDecoder::reset() {
    lfeFilter_.reset();
    crossfadeProgress_ = 1.0f;
}

void AmbisonicDecoder::updateLayout(SpeakerLayout layout) {
    const auto& info = getLayoutInfo(layout);
    lfeChannelIndex_ = info.lfeChannelIndex;

    // Copy decoder matrix into working buffer
    std::memset(currentMatrix_.data(), 0, currentMatrix_.size() * sizeof(float));
    int matrixSize = info.numChannels * kNumAmbiChannels;
    std::memcpy(currentMatrix_.data(), info.decoderMatrix,
                static_cast<size_t>(matrixSize) * sizeof(float));
}

void AmbisonicDecoder::decode(const float* bFormat, SpeakerLayout layout,
                               float* speakerOutputs) {
    // Detect layout change
    if (layout != currentLayout_) {
        // Save current matrix as previous for crossfade
        prevMatrix_ = currentMatrix_;
        prevLayout_ = currentLayout_;
        currentLayout_ = layout;
        updateLayout(layout);
        crossfadeProgress_ = 0.0f;
    }

    const auto& info = getLayoutInfo(layout);
    int numCh = info.numChannels;

    // Decode with current matrix
    for (int spk = 0; spk < numCh; ++spk) {
        float sum = 0.0f;
        for (int ch = 0; ch < kNumAmbiChannels; ++ch) {
            sum += currentMatrix_[static_cast<size_t>(spk * kNumAmbiChannels + ch)]
                   * bFormat[ch];
        }

        // If crossfading, blend with previous decoder output
        if (crossfadeProgress_ < 1.0f) {
            float prevSum = 0.0f;
            const auto& prevInfo = getLayoutInfo(prevLayout_);
            if (spk < prevInfo.numChannels) {
                for (int ch = 0; ch < kNumAmbiChannels; ++ch) {
                    prevSum += prevMatrix_[static_cast<size_t>(spk * kNumAmbiChannels + ch)]
                               * bFormat[ch];
                }
            }
            sum = prevSum + crossfadeProgress_ * (sum - prevSum);
        }

        speakerOutputs[spk] = sum;
    }

    // Handle remaining channels (beyond current layout)
    if (crossfadeProgress_ < 1.0f) {
        const auto& prevInfo = getLayoutInfo(prevLayout_);
        int prevNumCh = prevInfo.numChannels;
        for (int spk = numCh; spk < kMaxOutputChannels; ++spk) {
            if (spk < prevNumCh) {
                // Fade out channel that existed in previous layout
                float prevSum = 0.0f;
                for (int ch = 0; ch < kNumAmbiChannels; ++ch) {
                    prevSum += prevMatrix_[static_cast<size_t>(spk * kNumAmbiChannels + ch)]
                               * bFormat[ch];
                }
                speakerOutputs[spk] = prevSum * (1.0f - crossfadeProgress_);
            } else {
                speakerOutputs[spk] = 0.0f;
            }
        }
    } else {
        for (int spk = numCh; spk < kMaxOutputChannels; ++spk) {
            speakerOutputs[spk] = 0.0f;
        }
    }

    // LFE: lowpass W channel with crossfade support
    float lfeSignal = lfeFilter_.processSample(bFormat[BFormat::W]) * kLFEGainLinear;
    bool currentHasLfe = (lfeChannelIndex_ >= 0 && lfeChannelIndex_ < numCh);

    if (crossfadeProgress_ < 1.0f) {
        const auto& prevInfo = getLayoutInfo(prevLayout_);
        bool prevHasLfe = (prevInfo.lfeChannelIndex >= 0
                           && prevInfo.lfeChannelIndex < prevInfo.numChannels);

        if (currentHasLfe && prevHasLfe) {
            speakerOutputs[lfeChannelIndex_] = lfeSignal;
        } else if (currentHasLfe) {
            // Fade in LFE from matrix decode
            float matrixVal = speakerOutputs[lfeChannelIndex_];
            speakerOutputs[lfeChannelIndex_] = matrixVal
                + crossfadeProgress_ * (lfeSignal - matrixVal);
        } else if (prevHasLfe) {
            // Fade out LFE to matrix decode (or zero)
            int idx = prevInfo.lfeChannelIndex;
            float matrixVal = speakerOutputs[idx];
            speakerOutputs[idx] = matrixVal
                + (1.0f - crossfadeProgress_) * (lfeSignal - matrixVal);
        }
    } else if (currentHasLfe) {
        speakerOutputs[lfeChannelIndex_] = lfeSignal;
    }

    // Advance crossfade
    if (crossfadeProgress_ < 1.0f) {
        crossfadeProgress_ = std::min(1.0f, crossfadeProgress_ + crossfadeStep_);
    }
}

// ===== Layout info lookup =====

const LayoutInfo& getLayoutInfo(SpeakerLayout layout) {
    static const LayoutInfo layouts[] = {
        { SpeakerLayout::Stereo, 2, "Stereo",
          kDecoderStereo, kItuCoeffsStereoL, kItuCoeffsStereoR, -1 },
        { SpeakerLayout::Surround51, 6, "5.1",
          kDecoder51, kItuCoeffs51L, kItuCoeffs51R, 3 },
        { SpeakerLayout::Surround714, 12, "7.1.4",
          kDecoder714, kItuCoeffs714L, kItuCoeffs714R, 3 },
        { SpeakerLayout::Surround916, 16, "9.1.6",
          kDecoder916, kItuCoeffs916L, kItuCoeffs916R, 3 },
        { SpeakerLayout::Surround222, 24, "22.2",
          kDecoder222, kItuCoeffs222L, kItuCoeffs222R, 3 },
        { SpeakerLayout::AmbiX, 4, "AmbiX",
          kDecoderAmbiX, kItuCoeffsAmbiXL, kItuCoeffsAmbiXR, -1 },
    };

    int idx = static_cast<int>(layout);
    if (idx < 0 || idx >= static_cast<int>(SpeakerLayout::kNumLayouts))
        idx = 1;  // fallback to 5.1
    return layouts[idx];
}

}  // namespace audio_plugin
