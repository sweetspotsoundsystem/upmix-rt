#include <gtest/gtest.h>
#include <UpmixRT/PluginProcessor.h>
#include <UpmixRT/Constants.h>
#include <UpmixRT/SpeakerLayout.h>
#include <UpmixRT/AmbisonicEncoder.h>
#include <UpmixRT/AmbisonicDecoder.h>
#include <UpmixRT/SpatialAnalyzer.h>
#include <UpmixRT/Decorrelator.h>
#include <UpmixRT/FilterBank.h>
#include <UpmixRT/AnalysisBand.h>
#include <UpmixRT/HeightEstimator.h>
#include <UpmixRT/OutputWriter.h>
#include <cmath>
#include <array>

using namespace audio_plugin;

// ===== Phaseless encoding tests =====

TEST(AmbisonicEncoderTest, WChannelIsPhaseless) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
    float bFormat[kNumAmbiChannels];

    // Test with various input values
    float testL[] = {0.5f, -0.3f, 1.0f, 0.0f, -1.0f};
    float testR[] = {0.3f, 0.7f, 1.0f, 0.0f, 0.5f};

    for (int i = 0; i < 5; ++i) {
        encoder.encode(testL[i], testR[i], params, bFormat);
        float expected = (testL[i] + testR[i]) * kInvSqrt2;
        EXPECT_FLOAT_EQ(bFormat[BFormat::W], expected)
            << "W channel not phaseless at sample " << i;
    }
}

TEST(AmbisonicEncoderTest, YChannelIsPhaseless) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
    float bFormat[kNumAmbiChannels];

    float testL[] = {0.5f, -0.3f, 1.0f, 0.0f, -1.0f};
    float testR[] = {0.3f, 0.7f, 1.0f, 0.0f, 0.5f};

    for (int i = 0; i < 5; ++i) {
        encoder.encode(testL[i], testR[i], params, bFormat);
        float expected = (testL[i] - testR[i]) * kInvSqrt2;
        EXPECT_FLOAT_EQ(bFormat[BFormat::Y], expected)
            << "Y channel not phaseless at sample " << i;
    }
}

// ===== ITU Downmix reconstruction test =====

TEST(ITUDownmixTest, StereoLayoutReconstructsInput) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;
    SpatialAnalyzer analyzer;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Stereo);
    analyzer.prepare(48000.0);

    // Warm up filters
    for (int i = 0; i < 1000; ++i) {
        float bf[kNumAmbiChannels];
        float spk[kMaxOutputChannels];
        SpatialParams p = analyzer.process(0.0f, 0.0f);
        encoder.encode(0.0f, 0.0f, p, bf);
        decoder.decode(bf, SpeakerLayout::Stereo, spk);
    }

    // Test reconstruction
    float testL = 0.7f;
    float testR = -0.3f;

    SpatialParams params = analyzer.process(testL, testR);
    float bFormat[kNumAmbiChannels];
    encoder.encode(testL, testR, params, bFormat);

    float speakerOutputs[kMaxOutputChannels];
    decoder.decode(bFormat, SpeakerLayout::Stereo, speakerOutputs);

    // For stereo, ITU downmix is identity: L_down = spk[0], R_down = spk[1]
    const auto& info = getLayoutInfo(SpeakerLayout::Stereo);
    float downL = 0.0f, downR = 0.0f;
    for (int s = 0; s < info.numChannels; ++s) {
        downL += info.ituCoeffsL[s] * speakerOutputs[s];
        downR += info.ituCoeffsR[s] * speakerOutputs[s];
    }

    // W and Y should reconstruct L and R exactly
    // downL = ituL[0]*spk[0] + ituL[1]*spk[1]
    //       = 1.0 * (W*inv2 + Y*inv2) + 0.0 * (W*inv2 - Y*inv2)
    //       = W*inv2 + Y*inv2 = L
    EXPECT_NEAR(downL, testL, 1e-5f) << "Stereo ITU downmix L failed";
    EXPECT_NEAR(downR, testR, 1e-5f) << "Stereo ITU downmix R failed";
}

// ===== Decorrelator tests =====

TEST(DecorrelatorTest, AllpassPreservesMagnitude) {
    Decorrelator decorr;
    decorr.prepare(48000.0, kDecorrDelaysX, 2);

    // Feed a sustained signal and check that output magnitude converges to input
    float inputMag = 0.0f;
    float outputMag = 0.0f;

    for (int i = 0; i < 10000; ++i) {
        float input = std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        float output = decorr.process(input);

        if (i > 5000) {
            inputMag += input * input;
            outputMag += output * output;
        }
    }

    float ratio = outputMag / (inputMag + kEpsilon);
    EXPECT_NEAR(ratio, 1.0f, 0.05f) << "Allpass decorrelator changed magnitude";
}

// ===== FilterBank test =====

TEST(FilterBankTest, BandsSumToOriginal) {
    FilterBank filterBank;
    filterBank.prepare(48000.0);

    // LR2 crossovers should sum to flat magnitude in steady state
    // This is approximate due to transient response
    float bandL[kNumBands];
    float bandR[kNumBands];

    // Warm up with DC
    for (int i = 0; i < 10000; ++i) {
        filterBank.process(1.0f, 0.5f, bandL, bandR);
    }

    // After warmup, bands should sum to input
    float sumL = 0.0f;
    float sumR = 0.0f;
    for (int b = 0; b < kNumBands; ++b) {
        sumL += bandL[b];
        sumR += bandR[b];
    }

    EXPECT_NEAR(sumL, 1.0f, 0.1f) << "Filter bank L bands don't sum to input";
    EXPECT_NEAR(sumR, 0.5f, 0.1f) << "Filter bank R bands don't sum to input";
}

// ===== Silence in/out test =====

TEST(IntegrationTest, SilenceInSilenceOut) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;
    SpatialAnalyzer analyzer;
    OutputWriter writer;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    analyzer.prepare(48000.0);
    writer.prepare(48000.0);

    std::array<float, 6> outputChannels{};
    float* ptrs[6];
    for (int i = 0; i < 6; ++i) ptrs[i] = &outputChannels[static_cast<size_t>(i)];

    for (int s = 0; s < 1000; ++s) {
        SpatialParams params = analyzer.process(0.0f, 0.0f);
        float bFormat[kNumAmbiChannels];
        encoder.encode(0.0f, 0.0f, params, bFormat);
        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);
        writer.writeSample(speakers, 0.0f, 0.0f, 1.0f, 0.0f, 6, ptrs, 0);
    }

    for (int ch = 0; ch < 6; ++ch) {
        EXPECT_NEAR(outputChannels[static_cast<size_t>(ch)], 0.0f, 1e-6f)
            << "Non-silence output on channel " << ch;
    }
}

// ===== Phaseless encoding edge case tests =====

TEST(AmbisonicEncoderTest, WAndYPhaselessWithSilence) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
    float bFormat[kNumAmbiChannels];

    encoder.encode(0.0f, 0.0f, params, bFormat);
    EXPECT_FLOAT_EQ(bFormat[BFormat::W], 0.0f);
    EXPECT_FLOAT_EQ(bFormat[BFormat::Y], 0.0f);
}

TEST(AmbisonicEncoderTest, WAndYPhaselessWithFullScale) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
    float bFormat[kNumAmbiChannels];

    // Full scale positive
    encoder.encode(1.0f, 1.0f, params, bFormat);
    EXPECT_FLOAT_EQ(bFormat[BFormat::W], 2.0f * kInvSqrt2);
    EXPECT_FLOAT_EQ(bFormat[BFormat::Y], 0.0f);

    // Full scale negative
    encoder.encode(-1.0f, -1.0f, params, bFormat);
    EXPECT_FLOAT_EQ(bFormat[BFormat::W], -2.0f * kInvSqrt2);
    EXPECT_FLOAT_EQ(bFormat[BFormat::Y], 0.0f);

    // Full scale opposite polarity
    encoder.encode(1.0f, -1.0f, params, bFormat);
    EXPECT_FLOAT_EQ(bFormat[BFormat::W], 0.0f);
    EXPECT_FLOAT_EQ(bFormat[BFormat::Y], 2.0f * kInvSqrt2);
}

TEST(AmbisonicEncoderTest, WAndYPhaselessWithDC) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    SpatialParams params{1.0f, 0.0f, 0.0f, 0.0f};
    float bFormat[kNumAmbiChannels];

    // Process repeated DC values - W and Y must remain exact every sample
    for (int i = 0; i < 100; ++i) {
        encoder.encode(0.75f, 0.25f, params, bFormat);
        EXPECT_FLOAT_EQ(bFormat[BFormat::W], (0.75f + 0.25f) * kInvSqrt2)
            << "W not exact at sample " << i;
        EXPECT_FLOAT_EQ(bFormat[BFormat::Y], (0.75f - 0.25f) * kInvSqrt2)
            << "Y not exact at sample " << i;
    }
}

TEST(AmbisonicEncoderTest, WAndYIndependentOfSpatialParams) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    float bFormat[kNumAmbiChannels];
    float inputL = 0.6f, inputR = -0.4f;
    float expectedW = (inputL + inputR) * kInvSqrt2;
    float expectedY = (inputL - inputR) * kInvSqrt2;

    // Different spatial params should not affect W and Y
    SpatialParams paramSets[] = {
        {0.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.5f, 1.0f, 0.5f, 0.5f},
        {0.0f, -1.0f, 1.0f, 0.0f},
    };

    for (int i = 0; i < 4; ++i) {
        encoder.encode(inputL, inputR, paramSets[i], bFormat);
        EXPECT_FLOAT_EQ(bFormat[BFormat::W], expectedW)
            << "W depends on spatial params at set " << i;
        EXPECT_FLOAT_EQ(bFormat[BFormat::Y], expectedY)
            << "Y depends on spatial params at set " << i;
    }
}

// ===== Mono center and hard-pan tests =====

TEST(AmbisonicEncoderTest, MonoCenterInputYIsZero) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    // icc=1 => fully correlated, azimuth=0 => center front, no diffuseness
    SpatialParams params{1.0f, 0.0f, 0.0f, 0.0f};
    float bFormat[kNumAmbiChannels];

    float monoLevel = 0.8f;
    encoder.encode(monoLevel, monoLevel, params, bFormat);

    // Y = (L-R)/sqrt(2) = 0 for mono
    EXPECT_FLOAT_EQ(bFormat[BFormat::Y], 0.0f)
        << "Y should be zero for mono center input";
    // W = (L+R)/sqrt(2) = 2*monoLevel/sqrt(2) = monoLevel*sqrt(2)
    EXPECT_FLOAT_EQ(bFormat[BFormat::W], 2.0f * monoLevel * kInvSqrt2);
    // X for mono center with icc=1, azimuth=0: xDirect = mid * sqrt(1) * cos(0) * 0.5
    // mid = (L+R)/2 = monoLevel, so xDirect = monoLevel * 1.0 * 1.0 * 0.5
    // diffuse = 0 (diffuseness=0, side=0)
    EXPECT_GT(bFormat[BFormat::X], 0.0f)
        << "X should be positive for center-front mono input";
}

TEST(AmbisonicEncoderTest, HardPannedLeftVerifyWAndY) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
    float bFormat[kNumAmbiChannels];

    encoder.encode(1.0f, 0.0f, params, bFormat);

    // W = (1+0)/sqrt(2) = 1/sqrt(2)
    EXPECT_FLOAT_EQ(bFormat[BFormat::W], kInvSqrt2);
    // Y = (1-0)/sqrt(2) = 1/sqrt(2)
    EXPECT_FLOAT_EQ(bFormat[BFormat::Y], kInvSqrt2);
}

TEST(AmbisonicEncoderTest, HardPannedRightVerifyWAndY) {
    AmbisonicEncoder encoder;
    encoder.prepare(48000.0);

    SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
    float bFormat[kNumAmbiChannels];

    encoder.encode(0.0f, 1.0f, params, bFormat);

    // W = (0+1)/sqrt(2) = 1/sqrt(2)
    EXPECT_FLOAT_EQ(bFormat[BFormat::W], kInvSqrt2);
    // Y = (0-1)/sqrt(2) = -1/sqrt(2)
    EXPECT_FLOAT_EQ(bFormat[BFormat::Y], -kInvSqrt2);
}

// ===== Additional Decorrelator tests =====

TEST(DecorrelatorTest, AllpassPreservesMagnitudeMultipleFreqs) {
    float testFreqs[] = {100.0f, 500.0f, 1000.0f, 4000.0f, 10000.0f};

    for (float freq : testFreqs) {
        Decorrelator decorr;
        decorr.prepare(48000.0, kDecorrDelaysX, 2);

        float inputMag = 0.0f;
        float outputMag = 0.0f;

        for (int i = 0; i < 20000; ++i) {
            float input = std::sin(2.0f * kPi * freq * static_cast<float>(i) / 48000.0f);
            float output = decorr.process(input);

            // Only measure after warmup (allpass needs time to reach steady state)
            if (i > 10000) {
                inputMag += input * input;
                outputMag += output * output;
            }
        }

        float ratio = outputMag / (inputMag + kEpsilon);
        EXPECT_NEAR(ratio, 1.0f, 0.05f)
            << "Allpass magnitude not preserved at " << freq << " Hz";
    }
}

TEST(DecorrelatorTest, AllpassPreservesMagnitudeZDelays) {
    Decorrelator decorr;
    decorr.prepare(48000.0, kDecorrDelaysZ, 2);

    float inputMag = 0.0f;
    float outputMag = 0.0f;

    for (int i = 0; i < 20000; ++i) {
        float input = std::sin(2.0f * kPi * 2000.0f * static_cast<float>(i) / 48000.0f);
        float output = decorr.process(input);

        if (i > 10000) {
            inputMag += input * input;
            outputMag += output * output;
        }
    }

    float ratio = outputMag / (inputMag + kEpsilon);
    EXPECT_NEAR(ratio, 1.0f, 0.05f)
        << "Allpass Z-decorrelator changed magnitude";
}

TEST(DecorrelatorTest, OutputIsDecorrelatedFromInput) {
    Decorrelator decorr;
    decorr.prepare(48000.0, kDecorrDelaysX, 2);

    // Use a broadband signal (white noise approximation via simple LCG)
    unsigned int seed = 12345;
    float crossCorr = 0.0f;
    float inputPower = 0.0f;
    float outputPower = 0.0f;

    for (int i = 0; i < 50000; ++i) {
        // Simple pseudo-random generator
        seed = seed * 1103515245 + 12345;
        float input = (static_cast<float>(seed & 0x7FFF) / 16384.0f) - 1.0f;
        float output = decorr.process(input);

        // Skip warmup
        if (i > 5000) {
            crossCorr += input * output;
            inputPower += input * input;
            outputPower += output * output;
        }
    }

    // Normalized cross-correlation should be reduced by the decorrelator.
    // With 2 allpass stages and short delays, expect moderate decorrelation.
    // A value near 1.0 would indicate no decorrelation; near 0.0 would be full.
    float normCorr = crossCorr / (std::sqrt(inputPower * outputPower) + kEpsilon);
    EXPECT_LT(std::abs(normCorr), 0.6f)
        << "Decorrelator output is too correlated with input: " << normCorr;
}

TEST(DecorrelatorTest, SilenceInSilenceOut) {
    Decorrelator decorr;
    decorr.prepare(48000.0, kDecorrDelaysX, 2);

    for (int i = 0; i < 1000; ++i) {
        float output = decorr.process(0.0f);
        EXPECT_FLOAT_EQ(output, 0.0f)
            << "Decorrelator produced non-zero output from silence at sample " << i;
    }
}

TEST(DecorrelatorTest, ResetClearsState) {
    Decorrelator decorr;
    decorr.prepare(48000.0, kDecorrDelaysX, 2);

    // Feed signal to populate internal state
    for (int i = 0; i < 1000; ++i) {
        decorr.process(std::sin(2.0f * kPi * 440.0f * static_cast<float>(i) / 48000.0f));
    }

    // Reset and verify silence produces silence
    decorr.reset();
    for (int i = 0; i < 500; ++i) {
        float output = decorr.process(0.0f);
        EXPECT_FLOAT_EQ(output, 0.0f)
            << "Decorrelator not silent after reset at sample " << i;
    }
}

// ===== AnalysisBand tests =====

TEST(AnalysisBandTest, MonoSignalProducesHighICC) {
    AnalysisBand band;
    band.prepare(48000.0);

    // Feed identical L/R (mono) for enough samples to converge
    for (int i = 0; i < 5000; ++i) {
        float val = std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        band.process(val, val);
    }

    // After convergence, ICC should be very close to 1.0 for mono
    float val = std::sin(2.0f * kPi * 1000.0f * 5000.0f / 48000.0f);
    BandAnalysis result = band.process(val, val);
    EXPECT_GT(result.icc, 0.95f) << "Mono signal should produce ICC near 1.0";
}

TEST(AnalysisBandTest, HardPannedLeftGivesNegativeAzimuth) {
    AnalysisBand band;
    band.prepare(48000.0);

    // Feed signal only on L channel
    for (int i = 0; i < 5000; ++i) {
        float val = std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        band.process(val, 0.0f);
    }

    float val = std::sin(2.0f * kPi * 1000.0f * 5000.0f / 48000.0f);
    BandAnalysis result = band.process(val, 0.0f);
    EXPECT_LT(result.azimuth, -0.1f) << "Hard-panned L should give negative azimuth";
}

TEST(AnalysisBandTest, HardPannedRightGivesPositiveAzimuth) {
    AnalysisBand band;
    band.prepare(48000.0);

    // Feed signal only on R channel
    for (int i = 0; i < 5000; ++i) {
        float val = std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        band.process(0.0f, val);
    }

    float val = std::sin(2.0f * kPi * 1000.0f * 5000.0f / 48000.0f);
    BandAnalysis result = band.process(0.0f, val);
    EXPECT_GT(result.azimuth, 0.1f) << "Hard-panned R should give positive azimuth";
}

TEST(AnalysisBandTest, CenteredSignalGivesZeroAzimuth) {
    AnalysisBand band;
    band.prepare(48000.0);

    // Feed identical L/R
    for (int i = 0; i < 5000; ++i) {
        float val = std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        band.process(val, val);
    }

    float val = std::sin(2.0f * kPi * 1000.0f * 5000.0f / 48000.0f);
    BandAnalysis result = band.process(val, val);
    EXPECT_NEAR(result.azimuth, 0.0f, 0.1f) << "Centered signal should give azimuth near 0";
}

TEST(AnalysisBandTest, MidSideDecomposition) {
    AnalysisBand band;
    band.prepare(48000.0);

    BandAnalysis result = band.process(0.8f, 0.2f);
    EXPECT_FLOAT_EQ(result.mid, 0.5f) << "Mid should be (L+R)/2";
    EXPECT_FLOAT_EQ(result.side, 0.3f) << "Side should be (L-R)/2";
}

TEST(AnalysisBandTest, EnergyIsNonNegative) {
    AnalysisBand band;
    band.prepare(48000.0);

    for (int i = 0; i < 1000; ++i) {
        float val = std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        BandAnalysis result = band.process(val, -val);
        EXPECT_GE(result.energy, 0.0f) << "Energy should never be negative";
    }
}

// ===== FilterBank tests =====

TEST(FilterBankTest, AllBandsReceiveEnergyFromBroadbandNoise) {
    FilterBank filterBank;
    filterBank.prepare(48000.0);

    float bandL[kNumBands];
    float bandR[kNumBands];
    float bandEnergyL[kNumBands] = {};
    float bandEnergyR[kNumBands] = {};

    // Generate pseudo-random broadband noise using a simple LCG
    uint32_t seed = 12345;
    auto nextRandom = [&seed]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return (static_cast<float>(seed) / static_cast<float>(UINT32_MAX)) * 2.0f - 1.0f;
    };

    for (int i = 0; i < 20000; ++i) {
        float inL = nextRandom();
        float inR = nextRandom();
        filterBank.process(inL, inR, bandL, bandR);

        if (i >= 10000) {
            for (int b = 0; b < kNumBands; ++b) {
                bandEnergyL[b] += bandL[b] * bandL[b];
                bandEnergyR[b] += bandR[b] * bandR[b];
            }
        }
    }

    for (int b = 0; b < kNumBands; ++b) {
        EXPECT_GT(bandEnergyL[b], 0.001f)
            << "Band " << b << " L should receive energy from broadband noise";
        EXPECT_GT(bandEnergyR[b], 0.001f)
            << "Band " << b << " R should receive energy from broadband noise";
    }
}

TEST(FilterBankTest, LowFreqInLowestBand) {
    FilterBank filterBank;
    filterBank.prepare(48000.0);

    float bandL[kNumBands];
    float bandR[kNumBands];
    float bandEnergy[kNumBands] = {};

    // Generate a 50Hz sine (well below first crossover at 100Hz)
    for (int i = 0; i < 20000; ++i) {
        float val = std::sin(2.0f * kPi * 50.0f * static_cast<float>(i) / 48000.0f);
        filterBank.process(val, val, bandL, bandR);

        if (i >= 10000) {
            for (int b = 0; b < kNumBands; ++b) {
                bandEnergy[b] += bandL[b] * bandL[b];
            }
        }
    }

    // Band 0 should have the most energy
    for (int b = 1; b < kNumBands; ++b) {
        EXPECT_GT(bandEnergy[0], bandEnergy[b])
            << "50Hz signal: band 0 should have more energy than band " << b;
    }
}

TEST(FilterBankTest, HighFreqInHighestBand) {
    FilterBank filterBank;
    filterBank.prepare(48000.0);

    float bandL[kNumBands];
    float bandR[kNumBands];
    float bandEnergy[kNumBands] = {};

    // Generate a 20kHz sine (above highest crossover at 14kHz)
    for (int i = 0; i < 20000; ++i) {
        float val = std::sin(2.0f * kPi * 20000.0f * static_cast<float>(i) / 48000.0f);
        filterBank.process(val, val, bandL, bandR);

        if (i >= 10000) {
            for (int b = 0; b < kNumBands; ++b) {
                bandEnergy[b] += bandL[b] * bandL[b];
            }
        }
    }

    int lastBand = kNumBands - 1;
    for (int b = 0; b < lastBand; ++b) {
        EXPECT_GT(bandEnergy[lastBand], bandEnergy[b])
            << "20kHz signal: last band should have more energy than band " << b;
    }
}

// ===== HeightEstimator tests =====

TEST(HeightEstimatorTest, HFOnlySignalProducesHighElevation) {
    HeightEstimator estimator;
    estimator.prepare(48000.0);

    float bandEnergies[kNumBands] = {};
    for (int b = kHeightHFBandStart; b < kNumBands; ++b) {
        bandEnergies[b] = 1.0f;
    }

    float elevation = 0.0f;
    for (int i = 0; i < 5000; ++i) {
        elevation = estimator.process(bandEnergies);
    }

    EXPECT_GT(elevation, 0.3f) << "HF-only signal should produce high elevation";
}

TEST(HeightEstimatorTest, LFOnlySignalProducesLowElevation) {
    HeightEstimator estimator;
    estimator.prepare(48000.0);

    float bandEnergies[kNumBands] = {};
    for (int b = 0; b < kHeightHFBandStart; ++b) {
        bandEnergies[b] = 1.0f;
    }

    float elevation = 0.0f;
    for (int i = 0; i < 5000; ++i) {
        elevation = estimator.process(bandEnergies);
    }

    EXPECT_LT(elevation, 0.05f) << "LF-only signal should produce low elevation";
}

TEST(HeightEstimatorTest, ElevationInRange) {
    HeightEstimator estimator;
    estimator.prepare(48000.0);

    float bandEnergies[kNumBands];
    uint32_t seed = 54321;
    auto nextRandom = [&seed]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed) / static_cast<float>(UINT32_MAX);
    };

    for (int i = 0; i < 1000; ++i) {
        for (int b = 0; b < kNumBands; ++b) {
            bandEnergies[b] = nextRandom();
        }
        float elevation = estimator.process(bandEnergies);
        EXPECT_GE(elevation, 0.0f) << "Elevation should be >= 0";
        EXPECT_LE(elevation, kHeightMaxElevation + 0.01f) << "Elevation should be <= kHeightMaxElevation";
    }
}

// ===== SpatialAnalyzer integration tests =====

TEST(SpatialAnalyzerTest, MonoSignalProducesReasonableParams) {
    SpatialAnalyzer analyzer;
    analyzer.prepare(48000.0);

    for (int i = 0; i < 10000; ++i) {
        float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        analyzer.process(val, val);
    }

    float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * 10000.0f / 48000.0f);
    SpatialParams params = analyzer.process(val, val);

    EXPECT_GT(params.icc, 0.5f) << "Mono signal should produce high ICC";
    EXPECT_NEAR(params.azimuth, 0.0f, 0.3f) << "Mono signal should be centered";
    EXPECT_GE(params.diffuseness, 0.0f) << "Diffuseness should be non-negative";
    EXPECT_LE(params.diffuseness, 1.0f) << "Diffuseness should be <= 1";
    EXPECT_GE(params.elevation, 0.0f) << "Elevation should be non-negative";
    EXPECT_LE(params.elevation, kHeightMaxElevation + 0.01f) << "Elevation should be in range";
}

TEST(SpatialAnalyzerTest, SilenceProducesZeroParams) {
    SpatialAnalyzer analyzer;
    analyzer.prepare(48000.0);

    SpatialParams params{};
    for (int i = 0; i < 1000; ++i) {
        params = analyzer.process(0.0f, 0.0f);
    }

    EXPECT_NEAR(params.icc, 0.0f, 0.01f) << "Silence should produce ICC near 0";
    EXPECT_NEAR(params.azimuth, 0.0f, 0.01f) << "Silence should produce azimuth near 0";
    EXPECT_NEAR(params.elevation, 0.0f, 0.01f) << "Silence should produce elevation near 0";
}

TEST(SpatialAnalyzerTest, HardPannedLeftGivesNegativeAzimuth) {
    SpatialAnalyzer analyzer;
    analyzer.prepare(48000.0);

    for (int i = 0; i < 10000; ++i) {
        float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        analyzer.process(val, 0.0f);
    }

    float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * 10000.0f / 48000.0f);
    SpatialParams params = analyzer.process(val, 0.0f);
    EXPECT_LT(params.azimuth, -0.1f) << "Hard L panning should give negative azimuth";
}

TEST(SpatialAnalyzerTest, HardPannedRightGivesPositiveAzimuth) {
    SpatialAnalyzer analyzer;
    analyzer.prepare(48000.0);

    for (int i = 0; i < 10000; ++i) {
        float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        analyzer.process(0.0f, val);
    }

    float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * 10000.0f / 48000.0f);
    SpatialParams params = analyzer.process(0.0f, val);
    EXPECT_GT(params.azimuth, 0.1f) << "Hard R panning should give positive azimuth";
}

TEST(SpatialAnalyzerTest, DiffusenessRelationToICC) {
    SpatialAnalyzer analyzer;
    analyzer.prepare(48000.0);

    for (int i = 0; i < 10000; ++i) {
        float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);
        analyzer.process(val, val);
    }

    float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * 10000.0f / 48000.0f);
    SpatialParams params = analyzer.process(val, val);

    float expectedDiff = std::sqrt(1.0f - std::clamp(params.icc, 0.0f, 1.0f));
    EXPECT_NEAR(params.diffuseness, expectedDiff, 0.01f)
        << "Diffuseness should equal sqrt(1 - ICC)";
}

TEST(SpatialAnalyzerTest, HighFreqSignalProducesElevation) {
    SpatialAnalyzer analyzer;
    analyzer.prepare(48000.0);

    for (int i = 0; i < 20000; ++i) {
        float val = 0.5f * std::sin(2.0f * kPi * 16000.0f * static_cast<float>(i) / 48000.0f);
        analyzer.process(val, val);
    }

    float val = 0.5f * std::sin(2.0f * kPi * 16000.0f * 20000.0f / 48000.0f);
    SpatialParams params = analyzer.process(val, val);

    EXPECT_GT(params.elevation, 0.05f)
        << "High-frequency signal should produce some elevation";
}

// ===== ITU Downmix Matrix Constraint Tests =====
// Verify that for each layout, the decoder matrix D satisfies:
//   sum(ituL[s] * D[s][W]) = 1/sqrt(2)
//   sum(ituL[s] * D[s][Y]) = 1/sqrt(2)
//   sum(ituL[s] * D[s][X]) = 0
//   sum(ituL[s] * D[s][Z]) = 0
//   (and mirror for R)

static void verifyITUConstraints(SpeakerLayout layout, const char* name) {
    const auto& info = getLayoutInfo(layout);
    int n = info.numChannels;
    const float* D = info.decoderMatrix;
    const float* ituL = info.ituCoeffsL;
    const float* ituR = info.ituCoeffsR;

    float sumL_W = 0, sumL_X = 0, sumL_Y = 0, sumL_Z = 0;
    float sumR_W = 0, sumR_X = 0, sumR_Y = 0, sumR_Z = 0;

    for (int s = 0; s < n; ++s) {
        sumL_W += ituL[s] * D[s * kNumAmbiChannels + BFormat::W];
        sumL_X += ituL[s] * D[s * kNumAmbiChannels + BFormat::X];
        sumL_Y += ituL[s] * D[s * kNumAmbiChannels + BFormat::Y];
        sumL_Z += ituL[s] * D[s * kNumAmbiChannels + BFormat::Z];

        sumR_W += ituR[s] * D[s * kNumAmbiChannels + BFormat::W];
        sumR_X += ituR[s] * D[s * kNumAmbiChannels + BFormat::X];
        sumR_Y += ituR[s] * D[s * kNumAmbiChannels + BFormat::Y];
        sumR_Z += ituR[s] * D[s * kNumAmbiChannels + BFormat::Z];
    }

    float tol = 1e-3f;
    EXPECT_NEAR(sumL_W, kInvSqrt2, tol) << name << " L_W constraint";
    EXPECT_NEAR(sumL_Y, kInvSqrt2, tol) << name << " L_Y constraint";
    EXPECT_NEAR(sumL_X, 0.0f, tol) << name << " L_X constraint";
    EXPECT_NEAR(sumL_Z, 0.0f, tol) << name << " L_Z constraint";

    EXPECT_NEAR(sumR_W, kInvSqrt2, tol) << name << " R_W constraint";
    EXPECT_NEAR(sumR_Y, -kInvSqrt2, tol) << name << " R_Y constraint";
    EXPECT_NEAR(sumR_X, 0.0f, tol) << name << " R_X constraint";
    EXPECT_NEAR(sumR_Z, 0.0f, tol) << name << " R_Z constraint";
}

TEST(ITUConstraintTest, StereoMatrix) {
    verifyITUConstraints(SpeakerLayout::Stereo, "Stereo");
}
TEST(ITUConstraintTest, Surround51Matrix) {
    verifyITUConstraints(SpeakerLayout::Surround51, "5.1");
}
TEST(ITUConstraintTest, Surround714Matrix) {
    verifyITUConstraints(SpeakerLayout::Surround714, "7.1.4");
}
TEST(ITUConstraintTest, Surround916Matrix) {
    verifyITUConstraints(SpeakerLayout::Surround916, "9.1.6");
}
TEST(ITUConstraintTest, Surround222Matrix) {
    verifyITUConstraints(SpeakerLayout::Surround222, "22.2");
}
TEST(ITUConstraintTest, AmbiXMatrix) {
    verifyITUConstraints(SpeakerLayout::AmbiX, "AmbiX");
}

// ===== ITU Downmix Round-Trip Tests =====
// Encode stereo -> decode -> ITU downmix -> verify = original stereo
// This tests the full signal path (W and Y only, since X/Z cancel in downmix)

static void verifyITURoundTrip(SpeakerLayout layout, const char* name) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, layout);

    // Test with several stereo pairs
    float testPairs[][2] = {
        {0.7f, -0.3f}, {-0.5f, 0.5f}, {1.0f, 0.0f},
        {0.0f, 1.0f}, {0.3f, 0.3f}
    };

    for (auto& pair : testPairs) {
        float testL = pair[0];
        float testR = pair[1];

        // Encode with zero azimuth/icc (X and Z become zero)
        SpatialParams params{0.0f, 0.0f, 0.0f, 0.0f};
        float bFormat[kNumAmbiChannels];
        encoder.encode(testL, testR, params, bFormat);

        // With these params, X and Z should be zero (no direct, no diffuse)
        EXPECT_NEAR(bFormat[BFormat::X], 0.0f, 1e-6f)
            << name << " X should be zero with zero params";
        EXPECT_NEAR(bFormat[BFormat::Z], 0.0f, 1e-6f)
            << name << " Z should be zero with zero params";

        float speakerOutputs[kMaxOutputChannels];
        decoder.decode(bFormat, layout, speakerOutputs);

        const auto& info = getLayoutInfo(layout);
        float downL = 0.0f, downR = 0.0f;
        for (int s = 0; s < info.numChannels; ++s) {
            downL += info.ituCoeffsL[s] * speakerOutputs[s];
            downR += info.ituCoeffsR[s] * speakerOutputs[s];
        }

        EXPECT_NEAR(downL, testL, 1e-3f)
            << name << " ITU round-trip L failed for ("
            << testL << ", " << testR << ")";
        EXPECT_NEAR(downR, testR, 1e-3f)
            << name << " ITU round-trip R failed for ("
            << testL << ", " << testR << ")";
    }
}

TEST(ITURoundTripTest, Stereo) {
    verifyITURoundTrip(SpeakerLayout::Stereo, "Stereo");
}
TEST(ITURoundTripTest, Surround51) {
    verifyITURoundTrip(SpeakerLayout::Surround51, "5.1");
}
TEST(ITURoundTripTest, Surround714) {
    verifyITURoundTrip(SpeakerLayout::Surround714, "7.1.4");
}
TEST(ITURoundTripTest, Surround916) {
    verifyITURoundTrip(SpeakerLayout::Surround916, "9.1.6");
}
TEST(ITURoundTripTest, Surround222) {
    verifyITURoundTrip(SpeakerLayout::Surround222, "22.2");
}
TEST(ITURoundTripTest, AmbiX) {
    verifyITURoundTrip(SpeakerLayout::AmbiX, "AmbiX");
}

// ===== Level Consistency Test =====
// Verify total output power is within +/-0.5dB across all speaker layouts

TEST(LevelConsistencyTest, OutputPowerWithinHalfDB) {
    SpeakerLayout layouts[] = {
        SpeakerLayout::Stereo, SpeakerLayout::Surround51,
        SpeakerLayout::Surround714, SpeakerLayout::Surround916,
        SpeakerLayout::Surround222
    };

    float powers[5] = {};

    for (int li = 0; li < 5; ++li) {
        AmbisonicEncoder encoder;
        AmbisonicDecoder decoder;
        encoder.prepare(48000.0);
        decoder.prepare(48000.0, layouts[li]);

        const auto& info = getLayoutInfo(layouts[li]);
        float totalPower = 0.0f;
        int count = 0;

        for (int i = 0; i < 5000; ++i) {
            float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(i) / 48000.0f);

            SpatialParams params{0.0f, 0.0f, 0.0f, 0.0f};
            float bFormat[kNumAmbiChannels];
            encoder.encode(val, val, params, bFormat);

            float speakerOutputs[kMaxOutputChannels];
            decoder.decode(bFormat, layouts[li], speakerOutputs);

            if (i >= 2000) {
                // Compute ITU downmix power (this is what the listener hears)
                float downL = 0.0f, downR = 0.0f;
                for (int s = 0; s < info.numChannels; ++s) {
                    downL += info.ituCoeffsL[s] * speakerOutputs[s];
                    downR += info.ituCoeffsR[s] * speakerOutputs[s];
                }
                totalPower += downL * downL + downR * downR;
                count++;
            }
        }

        powers[li] = totalPower / static_cast<float>(count);
    }

    // Compare all layouts against stereo reference
    float refPower = powers[0];
    for (int li = 1; li < 5; ++li) {
        float ratioDb = 10.0f * std::log10(powers[li] / (refPower + kEpsilon));
        EXPECT_NEAR(ratioDb, 0.0f, 0.5f)
            << "Layout " << li << " power differs from stereo by " << ratioDb << " dB";
    }
}

// ===== Click-Free Layout Switching Test =====
// Switch layout during active signal and verify no output sample exceeds 1.0

TEST(ClickFreeTest, LayoutSwitchDuringPlayback) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Surround51);

    SpeakerLayout layouts[] = {
        SpeakerLayout::Surround51, SpeakerLayout::Surround714,
        SpeakerLayout::Stereo, SpeakerLayout::Surround916,
        SpeakerLayout::Surround222, SpeakerLayout::Surround51
    };

    int layoutIdx = 0;
    float maxSample = 0.0f;

    for (int i = 0; i < 20000; ++i) {
        float val = 0.5f * std::sin(2.0f * kPi * 440.0f * static_cast<float>(i) / 48000.0f);

        SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
        float bFormat[kNumAmbiChannels];
        encoder.encode(val, val, params, bFormat);

        // Switch layout every 3000 samples
        if (i > 0 && i % 3000 == 0 && layoutIdx < 5) {
            layoutIdx++;
        }

        float speakerOutputs[kMaxOutputChannels];
        decoder.decode(bFormat, layouts[layoutIdx], speakerOutputs);

        const auto& info = getLayoutInfo(layouts[layoutIdx]);
        for (int ch = 0; ch < info.numChannels; ++ch) {
            float absVal = std::abs(speakerOutputs[ch]);
            if (absVal > maxSample) maxSample = absVal;
        }
    }

    EXPECT_LE(maxSample, 1.0f)
        << "Layout switch produced sample > 1.0: " << maxSample;
}

// ===== Dry/Wet Test =====
// Main stereo (1-2) stays dry passthrough.
// At 0% wet: aux outputs are silent.
// At 100% wet: aux outputs carry decoded upmix.

TEST(DryWetTest, ZeroWetOnlyFrontLR) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;
    OutputWriter writer;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    writer.prepare(48000.0);

    constexpr int numCh = 8;
    std::array<float, numCh> outputChannels{};
    float* ptrs[numCh];
    for (int i = 0; i < numCh; ++i) ptrs[i] = &outputChannels[static_cast<size_t>(i)];

    // Feed signal at 0% wet for enough samples to converge smoothing.
    for (int s = 0; s < 5000; ++s) {
        float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(s) / 48000.0f);

        SpatialParams params{0.0f, 0.0f, 0.0f, 0.0f};
        float bFormat[kNumAmbiChannels];
        encoder.encode(val, val, params, bFormat);

        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);

        writer.writeSample(speakers, val, val, 0.0f, 0.0f, numCh, ptrs, 0);
    }

    // Main L/R should have dry signal, all aux channels should be near zero.
    float frontPower = outputChannels[0] * outputChannels[0] + outputChannels[1] * outputChannels[1];
    float auxPower = 0.0f;
    for (int ch = 2; ch < numCh; ++ch) {
        auxPower += outputChannels[static_cast<size_t>(ch)] * outputChannels[static_cast<size_t>(ch)];
    }

    EXPECT_GT(frontPower, 0.01f) << "Front L/R should have signal at 0% wet";
    EXPECT_NEAR(auxPower, 0.0f, 1e-4f) << "Aux outputs should be silent at 0% wet";
}

TEST(DryWetTest, FullWetAllChannelsActive) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;
    OutputWriter writer;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    writer.prepare(48000.0);

    constexpr int numCh = 8;
    float channelEnergy[numCh] = {};
    float mainTrackingError = 0.0f;
    int mainTrackingCount = 0;

    for (int s = 0; s < 10000; ++s) {
        float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(s) / 48000.0f);

        // Use non-zero ICC to generate X, giving signal to all speakers
        SpatialParams params{0.8f, 0.3f, 0.2f, 0.1f};
        float bFormat[kNumAmbiChannels];
        encoder.encode(val, -val, params, bFormat);

        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);

        std::array<float, numCh> outputChannels{};
        float* ptrs[numCh];
        for (int i = 0; i < numCh; ++i) ptrs[i] = &outputChannels[static_cast<size_t>(i)];

        writer.writeSample(speakers, val, -val, 1.0f, 0.0f, numCh, ptrs, 0);

        if (s >= 5000) {
            for (int ch = 0; ch < numCh; ++ch) {
                channelEnergy[ch] += outputChannels[static_cast<size_t>(ch)]
                                   * outputChannels[static_cast<size_t>(ch)];
            }
            mainTrackingError += std::abs(outputChannels[0] - val);
            mainTrackingError += std::abs(outputChannels[1] + val);
            mainTrackingCount += 2;
        }
    }

    // Main output should remain dry passthrough even at 100% wet.
    EXPECT_LT(mainTrackingError / static_cast<float>(mainTrackingCount), 1e-6f)
        << "Main output should track dry passthrough at 100% wet";

    // Aux channels 3-8 should carry mapped wet outputs:
    // [wet L, wet R, C, LFE, Ls, Rs]
    EXPECT_GT(channelEnergy[2], 0.01f) << "Wet L should be present on channel 3";
    EXPECT_GT(channelEnergy[3], 0.01f) << "Wet R should be present on channel 4";
    EXPECT_GT(channelEnergy[4], 0.01f) << "Center should be present on channel 5";
    EXPECT_GT(channelEnergy[6], 0.001f) << "Ls should be present on channel 7";
    EXPECT_GT(channelEnergy[7], 0.001f) << "Rs should be present on channel 8";
}

TEST(DryWetTest, AuxRoutingPreservesAllWetChannels) {
    OutputWriter writer;
    writer.prepare(48000.0);

    constexpr int numWet = 24;
    constexpr int numOut = numWet + 2;  // main dry stereo + wet aux

    float speakers[kMaxOutputChannels] = {};
    for (int ch = 0; ch < numWet; ++ch) {
        speakers[ch] = 0.01f * static_cast<float>(ch + 1);
    }

    std::array<float, numOut> outputChannels{};
    float* ptrs[numOut];
    for (int ch = 0; ch < numOut; ++ch) {
        ptrs[ch] = &outputChannels[static_cast<size_t>(ch)];
    }

    constexpr float dryL = 0.33f;
    constexpr float dryR = -0.22f;
    writer.writeSample(speakers, dryL, dryR, 1.0f, 0.0f, numOut, ptrs, 0);

    EXPECT_FLOAT_EQ(outputChannels[0], dryL);
    EXPECT_FLOAT_EQ(outputChannels[1], dryR);
    for (int wetCh = 0; wetCh < numWet; ++wetCh) {
        EXPECT_FLOAT_EQ(outputChannels[static_cast<size_t>(wetCh + 2)], speakers[wetCh])
            << "Wet channel " << wetCh << " was not routed to aux output " << (wetCh + 2);
    }
}

// ===== LFE Lowpass Test =====
// Verify LFE channel has lowpassed content (more energy at low freq than high freq)

TEST(LFETest, LFEChannelIsLowpassed) {
    AmbisonicDecoder decoder;

    // Test with low frequency (50Hz) - should pass through LFE
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    float lfeLowEnergy = 0.0f;
    for (int i = 0; i < 20000; ++i) {
        float w = std::sin(2.0f * kPi * 50.0f * static_cast<float>(i) / 48000.0f);
        float bFormat[kNumAmbiChannels] = {w, 0.0f, 0.0f, 0.0f};
        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);
        if (i >= 10000) {
            lfeLowEnergy += speakers[3] * speakers[3];
        }
    }

    // Test with high frequency (5kHz) - should be attenuated by LFE filter
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    float lfeHighEnergy = 0.0f;
    for (int i = 0; i < 20000; ++i) {
        float w = std::sin(2.0f * kPi * 5000.0f * static_cast<float>(i) / 48000.0f);
        float bFormat[kNumAmbiChannels] = {w, 0.0f, 0.0f, 0.0f};
        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);
        if (i >= 10000) {
            lfeHighEnergy += speakers[3] * speakers[3];
        }
    }

    // Low freq should have significantly more energy in LFE than high freq
    EXPECT_GT(lfeLowEnergy, lfeHighEnergy * 10.0f)
        << "LFE should pass low frequencies and attenuate high frequencies"
        << " (low energy=" << lfeLowEnergy << ", high energy=" << lfeHighEnergy << ")";
}

TEST(LFETest, LFEChannelHasContent) {
    AmbisonicDecoder decoder;
    decoder.prepare(48000.0, SpeakerLayout::Surround51);

    float lfeEnergy = 0.0f;
    for (int i = 0; i < 20000; ++i) {
        float w = std::sin(2.0f * kPi * 80.0f * static_cast<float>(i) / 48000.0f);
        float bFormat[kNumAmbiChannels] = {w, 0.0f, 0.0f, 0.0f};
        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);
        if (i >= 10000) {
            lfeEnergy += speakers[3] * speakers[3];
        }
    }

    EXPECT_GT(lfeEnergy, 0.001f) << "LFE channel should have content from 80Hz input";
}

// ===== Output Gain Tests =====

TEST(GainTest, ZeroDbProducesUnchangedOutput) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;
    OutputWriter writerWithGain;
    OutputWriter writerNoGain;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    writerWithGain.prepare(48000.0);
    writerNoGain.prepare(48000.0);

    constexpr int numCh = 6;

    for (int s = 0; s < 2000; ++s) {
        float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(s) / 48000.0f);

        SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
        float bFormat[kNumAmbiChannels];
        encoder.encode(val, val, params, bFormat);

        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);

        std::array<float, numCh> outA{}, outB{};
        float* ptrsA[numCh];
        float* ptrsB[numCh];
        for (int i = 0; i < numCh; ++i) {
            ptrsA[i] = &outA[static_cast<size_t>(i)];
            ptrsB[i] = &outB[static_cast<size_t>(i)];
        }

        writerWithGain.writeSample(speakers, val, val, 1.0f, 0.0f, numCh, ptrsA, 0);
        writerNoGain.writeSample(speakers, val, val, 1.0f, 0.0f, numCh, ptrsB, 0);

        for (int ch = 0; ch < numCh; ++ch) {
            EXPECT_FLOAT_EQ(outA[static_cast<size_t>(ch)], outB[static_cast<size_t>(ch)])
                << "0 dB gain should produce identical output on ch " << ch << " at sample " << s;
        }
    }
}

TEST(GainTest, Minus42DbAttenuatesSignificantly) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;
    OutputWriter writerAttenuated;
    OutputWriter writerReference;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    writerAttenuated.prepare(48000.0);
    writerReference.prepare(48000.0);

    constexpr int numCh = 8;
    float wetEnergyAttenuated = 0.0f;
    float wetEnergyReference = 0.0f;
    float dryEnergyAttenuated = 0.0f;
    float dryEnergyReference = 0.0f;

    for (int s = 0; s < 5000; ++s) {
        float val = 0.5f * std::sin(2.0f * kPi * 1000.0f * static_cast<float>(s) / 48000.0f);

        SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
        float bFormat[kNumAmbiChannels];
        encoder.encode(val, val, params, bFormat);

        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);

        std::array<float, numCh> outAtt{};
        std::array<float, numCh> outRef{};
        float* ptrsAtt[numCh];
        float* ptrsRef[numCh];
        for (int i = 0; i < numCh; ++i) {
            ptrsAtt[i] = &outAtt[static_cast<size_t>(i)];
            ptrsRef[i] = &outRef[static_cast<size_t>(i)];
        }

        writerAttenuated.writeSample(speakers, val, val, 1.0f, -42.0f, numCh, ptrsAtt, 0);
        writerReference.writeSample(speakers, val, val, 1.0f, 0.0f, numCh, ptrsRef, 0);

        if (s >= 2000) {
            for (int ch = 0; ch < 2; ++ch) {
                dryEnergyAttenuated += outAtt[static_cast<size_t>(ch)] * outAtt[static_cast<size_t>(ch)];
                dryEnergyReference += outRef[static_cast<size_t>(ch)] * outRef[static_cast<size_t>(ch)];
            }
            for (int ch = 2; ch < numCh; ++ch) {
                wetEnergyAttenuated += outAtt[static_cast<size_t>(ch)] * outAtt[static_cast<size_t>(ch)];
                wetEnergyReference += outRef[static_cast<size_t>(ch)] * outRef[static_cast<size_t>(ch)];
            }
        }
    }

    // -42 dB linear = 10^(-42/20) ~= 0.0079, energy ratio ~= 6.3e-5
    float wetRatio = wetEnergyAttenuated / (wetEnergyReference + kEpsilon);
    EXPECT_LT(wetRatio, 0.0005f) << "Gain at -42 dB should attenuate wet output significantly (ratio=" << wetRatio << ")";
    EXPECT_GT(wetRatio, 0.00001f) << "Gain at -42 dB should not mute wet output (ratio=" << wetRatio << ")";

    float dryRatio = dryEnergyAttenuated / (dryEnergyReference + kEpsilon);
    EXPECT_NEAR(dryRatio, 1.0f, 1e-6f)
        << "Dry output should remain unchanged by gain (ratio=" << dryRatio << ")";
}

TEST(GainTest, SilenceInSilenceOutWithGain) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;
    OutputWriter writer;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    writer.prepare(48000.0);

    constexpr int numCh = 6;
    std::array<float, numCh> outputChannels{};
    float* ptrs[numCh];
    for (int i = 0; i < numCh; ++i) ptrs[i] = &outputChannels[static_cast<size_t>(i)];

    for (int s = 0; s < 1000; ++s) {
        SpatialParams params{0.0f, 0.0f, 0.0f, 0.0f};
        float bFormat[kNumAmbiChannels];
        encoder.encode(0.0f, 0.0f, params, bFormat);
        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);
        writer.writeSample(speakers, 0.0f, 0.0f, 1.0f, -12.0f, numCh, ptrs, 0);
    }

    for (int ch = 0; ch < numCh; ++ch) {
        EXPECT_NEAR(outputChannels[static_cast<size_t>(ch)], 0.0f, 1e-6f)
            << "Non-silence output on channel " << ch << " with gain applied";
    }
}

TEST(GainTest, NoSampleExceedsOnePointZeroDuringGainTransition) {
    AmbisonicEncoder encoder;
    AmbisonicDecoder decoder;
    OutputWriter writer;

    encoder.prepare(48000.0);
    decoder.prepare(48000.0, SpeakerLayout::Surround51);
    writer.prepare(48000.0);

    constexpr int numCh = 6;
    float maxSample = 0.0f;
    float dryTrackingError = 0.0f;
    int dryTrackingCount = 0;

    // Gain starts at 0 dB (default), transitions to -24 dB and back
    for (int s = 0; s < 10000; ++s) {
        float val = 0.5f * std::sin(2.0f * kPi * 440.0f * static_cast<float>(s) / 48000.0f);

        SpatialParams params{0.5f, 0.0f, 0.5f, 0.0f};
        float bFormat[kNumAmbiChannels];
        encoder.encode(val, val, params, bFormat);

        float speakers[kMaxOutputChannels];
        decoder.decode(bFormat, SpeakerLayout::Surround51, speakers);

        float gainDb = (s < 3000) ? 0.0f : ((s < 6000) ? -24.0f : 0.0f);

        std::array<float, numCh> out{};
        float* ptrs[numCh];
        for (int i = 0; i < numCh; ++i) ptrs[i] = &out[static_cast<size_t>(i)];

        writer.writeSample(speakers, val, val, 1.0f, gainDb, numCh, ptrs, 0);

        dryTrackingError += std::abs(out[0] - val);
        dryTrackingError += std::abs(out[1] - val);
        dryTrackingCount += 2;

        for (int ch = 0; ch < numCh; ++ch) {
            float absVal = std::abs(out[static_cast<size_t>(ch)]);
            if (absVal > maxSample) maxSample = absVal;
        }
    }

    EXPECT_LE(maxSample, 1.0f)
        << "Gain transition produced sample > 1.0: " << maxSample;
    EXPECT_LT(dryTrackingError / static_cast<float>(dryTrackingCount), 1e-6f)
        << "Dry main output should not be impacted by gain transitions";
}

// ===== Plugin instantiation test =====

TEST(PluginTest, CanInstantiate) {
    auto plugin = std::make_unique<AudioPluginAudioProcessor>();
    EXPECT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->getName(), "UpmixRT");
}
