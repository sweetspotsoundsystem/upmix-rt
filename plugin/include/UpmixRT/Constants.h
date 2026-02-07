#pragma once

namespace audio_plugin {

// ===== B-Format channel indices =====
enum BFormat : int { W = 0, X = 1, Y = 2, Z = 3, kNumAmbiChannels = 4 };

// ===== Speaker layout enum =====
enum class SpeakerLayout : int {
    Stereo = 0,
    Surround51 = 1,
    Surround714 = 2,
    Surround916 = 3,
    Surround222 = 4,
    AmbiX = 5,
    kNumLayouts
};

// ===== Spatial analysis result (filter bank -> encoder) =====
struct SpatialParams {
    float icc;           // Energy-weighted ICC [0..1]
    float azimuth;       // Energy-weighted azimuth [-pi/2..+pi/2]
    float diffuseness;   // sqrt(1 - icc) [0..1]
    float elevation;     // Height factor [0..0.5]
};

// ===== Per-band analysis result =====
struct BandAnalysis {
    float mid;           // (L+R)/2
    float side;          // (L-R)/2
    float icc;           // Smoothed ICC for this band
    float azimuth;       // Smoothed azimuth for this band
    float energy;        // Smoothed energy for this band
};

// ===== APVTS Parameter IDs (stable contract) =====
namespace ParamID {
    inline constexpr const char* kLayout = "layout";
    inline constexpr const char* kDryWet = "drywet";
    inline constexpr const char* kGain = "gain";
}

// ===== Channel counts per layout =====
constexpr int kLayoutChannelCount[] = { 2, 6, 12, 16, 24, 4 };

// ===== Constants =====
constexpr int kNumInputChannels = 2;
constexpr int kMaxOutputChannels = 64;
constexpr int kNumBands = 8;
constexpr int kNumCrossovers = 7;
constexpr float kCrossoverFreqs[] = {100, 250, 630, 1600, 4000, 8000, 14000};

// Smoothing
constexpr float kICCSmoothingTimeSec = 0.008f;
constexpr float kAzimuthSmoothingTimeSec = 0.010f;
constexpr float kEnergySmoothingTimeSec = 0.005f;

// Decorrelator (capped < 5ms @ 48kHz)
constexpr int kDecorrDelaysX[] = {37, 113};    // 2.4ms max
constexpr int kDecorrDelaysZ[] = {149, 197};   // 4.1ms max
constexpr float kDecorrRefSampleRate = 48000.0f;
constexpr float kAllpassCoeff = 0.7f;

// Height
constexpr int kHeightHFBandStart = 5;
constexpr float kHeightMaxElevation = 0.5f;

// LFE
constexpr float kLFECutoffHz = 120.0f;
constexpr float kLFEGainLinear = 0.316f;  // -10dB

// Transitions
constexpr float kDryWetSmoothTimeSec = 0.020f;   // 20ms
constexpr float kGainSmoothTimeSec = 0.020f;     // 20ms
constexpr float kLayoutCrossfadeTimeSec = 0.020f; // 20ms

constexpr float kEpsilon = 1e-10f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kSqrt2 = 1.41421356237309504880f;
constexpr float kInvSqrt2 = 0.70710678118654752440f;

}  // namespace audio_plugin
