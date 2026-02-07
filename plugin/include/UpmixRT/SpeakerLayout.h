#pragma once

#include "Constants.h"

namespace audio_plugin {

// ITU-constrained decoder matrices.
// Each matrix: decoderMatrix[speaker][ambi_channel]
// Constraint: ITU_downmix(decode(W,X,Y,Z)) == (L, R)

struct LayoutInfo {
    SpeakerLayout layout;
    int numChannels;
    const char* name;
    // Decoder matrix: [numChannels][kNumAmbiChannels]
    // Stored as flat array: row-major, speaker x ambi_ch
    const float* decoderMatrix;
    // ITU downmix coefficients for L and R
    const float* ituCoeffsL;
    const float* ituCoeffsR;
    // LFE channel index (-1 if none)
    int lfeChannelIndex;
};

const LayoutInfo& getLayoutInfo(SpeakerLayout layout);

// ===== Stereo (2ch) =====
// L, R — passthrough with spatial processing
// Decoder: W and Y reconstruct L/R, X and Z add width
inline constexpr float kDecoderStereo[] = {
    // spk    W          X          Y          Z
    /* L */  kInvSqrt2,  0.0f,      kInvSqrt2,  0.0f,
    /* R */  kInvSqrt2,  0.0f,     -kInvSqrt2,  0.0f,
};
inline constexpr float kItuCoeffsStereoL[] = { 1.0f, 0.0f };
inline constexpr float kItuCoeffsStereoR[] = { 0.0f, 1.0f };

// ===== 5.1 (6ch) =====
// L, R, C, LFE, Ls, Rs
// ITU-R BS.775: L_down = L + 0.707*C + 0.707*Ls
//               R_down = R + 0.707*C + 0.707*Rs
// Constraint: X and Z contributions cancel in ITU downmix
inline constexpr float kDecoder51[] = {
    // spk    W          X          Y          Z
    /* L  */  0.3734f,   0.2268f,   0.4714f,   0.0000f,
    /* R  */  0.3734f,   0.2268f,  -0.4714f,   0.0000f,
    /* C  */  0.2079f,  -0.3666f,   0.0000f,   0.0000f,
    /* LFE*/  0.0000f,   0.0000f,   0.0000f,   0.0000f,
    /* Ls */  0.2641f,   0.0458f,   0.3334f,   0.0000f,
    /* Rs */  0.2641f,   0.0458f,  -0.3334f,   0.0000f,
};
inline constexpr float kItuCoeffs51L[] = { 1.0f, 0.0f, 0.707f, 0.0f, 0.707f, 0.0f };
inline constexpr float kItuCoeffs51R[] = { 0.0f, 1.0f, 0.707f, 0.0f, 0.0f, 0.707f };

// ===== 7.1.4 (12ch) =====
// L, R, C, LFE, Ls, Rs, Lss, Rss, Ltf, Rtf, Ltr, Rtr
inline constexpr float kDecoder714[] = {
    // spk     W          X          Y          Z
    /* L   */  0.2907f,   0.1884f,   0.3488f,  -0.0739f,
    /* R   */  0.2907f,   0.1884f,  -0.3488f,  -0.0739f,
    /* C   */  0.1849f,  -0.2426f,   0.0000f,  -0.1044f,
    /* LFE */  0.0000f,   0.0000f,   0.0000f,   0.0000f,
    /* Ls  */  0.1593f,  -0.0116f,   0.2004f,  -0.0522f,
    /* Rs  */  0.1593f,  -0.0116f,  -0.2004f,  -0.0522f,
    /* Lss */  0.1354f,  -0.0858f,   0.1644f,  -0.0369f,
    /* Rss */  0.1354f,  -0.0858f,  -0.1644f,  -0.0369f,
    /* Ltf */  0.1054f,   0.1542f,   0.1344f,   0.2031f,
    /* Rtf */  0.1054f,   0.1542f,  -0.1344f,   0.2031f,
    /* Ltr */  0.1054f,  -0.0858f,   0.1344f,   0.2031f,
    /* Rtr */  0.1054f,  -0.0858f,  -0.1344f,   0.2031f,
};
inline constexpr float kItuCoeffs714L[] = {
    1.0f, 0.0f, 0.707f, 0.0f, 0.707f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f
};
inline constexpr float kItuCoeffs714R[] = {
    0.0f, 1.0f, 0.707f, 0.0f, 0.0f, 0.707f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f
};

// ===== 9.1.6 (16ch) =====
// L, R, C, LFE, Ls, Rs, Lss, Rss, Ltf, Rtf, Ltr, Rtr, Ltm, Rtm, Lw, Rw
inline constexpr float kDecoder916[] = {
    // spk     W          X          Y          Z
    /* L   */  0.2858f,   0.1375f,   0.3322f,  -0.0665f,
    /* R   */  0.2858f,   0.1375f,  -0.3322f,  -0.0665f,
    /* C   */  0.2063f,  -0.2299f,   0.0000f,  -0.0940f,
    /* LFE */  0.0000f,   0.0000f,   0.0000f,   0.0000f,
    /* Ls  */  0.1530f,  -0.0313f,   0.1761f,  -0.0332f,
    /* Rs  */  0.1530f,  -0.0313f,  -0.1761f,  -0.0332f,
    /* Lss */  0.1224f,  -0.0850f,   0.1409f,  -0.0266f,
    /* Rss */  0.1224f,  -0.0850f,  -0.1409f,  -0.0266f,
    /* Ltf */  0.0924f,   0.1050f,   0.1109f,   0.1534f,
    /* Rtf */  0.0924f,   0.1050f,  -0.1109f,   0.1534f,
    /* Ltr */  0.0924f,  -0.0750f,   0.1109f,   0.1534f,
    /* Rtr */  0.0924f,  -0.0750f,  -0.1109f,   0.1534f,
    /* Ltm */  0.0818f,   0.0112f,   0.0957f,   0.1801f,
    /* Rtm */  0.0818f,   0.0112f,  -0.0957f,   0.1801f,
    /* Lw  */  0.1030f,   0.1187f,   0.2261f,  -0.0332f,
    /* Rw  */  0.1030f,   0.1187f,  -0.2261f,  -0.0332f,
};
inline constexpr float kItuCoeffs916L[] = {
    1.0f, 0.0f, 0.707f, 0.0f, 0.5f, 0.0f, 0.4f, 0.0f,
    0.4f, 0.0f, 0.4f, 0.0f, 0.3f, 0.0f, 0.5f, 0.0f
};
inline constexpr float kItuCoeffs916R[] = {
    0.0f, 1.0f, 0.707f, 0.0f, 0.0f, 0.5f, 0.0f, 0.4f,
    0.0f, 0.4f, 0.0f, 0.4f, 0.0f, 0.3f, 0.0f, 0.5f
};

// ===== 22.2 (24ch) =====
// Full NHK Super Hi-Vision layout
// Channels: FL, FR, FC, LFE1, BL, BR, FLc, FRc, BC, LFE2,
//           SiL, SiR, TpFL, TpFR, TpFC, TpC, TpBL, TpBR,
//           TpSiL, TpSiR, TpBC, BtFC, BtFL, BtFR
inline constexpr float kDecoder222[] = {
    // spk      W          X          Y          Z
    /* FL   */  0.2313f,   0.1108f,   0.3578f,  -0.0476f,
    /* FR   */  0.2313f,   0.1108f,  -0.3578f,  -0.0476f,
    /* FC   */  0.1999f,  -0.1363f,   0.0000f,  -0.0672f,
    /* LFE1 */  0.0000f,   0.0000f,   0.0000f,   0.0000f,
    /* BL   */  0.1257f,  -0.0645f,   0.1889f,  -0.0237f,
    /* BR   */  0.1257f,  -0.0645f,  -0.1889f,  -0.0237f,
    /* FLc  */  0.1457f,   0.0755f,   0.1689f,  -0.0237f,
    /* FRc  */  0.1457f,   0.0755f,  -0.1689f,  -0.0237f,
    /* BC   */  0.1164f,  -0.0981f,   0.0000f,  -0.0336f,
    /* LFE2 */  0.0000f,   0.0000f,   0.0000f,   0.0000f,
    /* SiL  */  0.1105f,  -0.0276f,   0.1611f,  -0.0190f,
    /* SiR  */  0.1105f,  -0.0276f,  -0.1611f,  -0.0190f,
    /* TpFL */  0.0754f,   0.0693f,   0.1133f,   0.1058f,
    /* TpFR */  0.0754f,   0.0693f,  -0.1133f,   0.1058f,
    /* TpFC */  0.0705f,   0.0924f,   0.0000f,   0.1210f,
    /* TpC  */  0.0654f,   0.0093f,   0.0000f,   0.1458f,
    /* TpBL */  0.0654f,  -0.0507f,   0.1033f,   0.1058f,
    /* TpBR */  0.0654f,  -0.0507f,  -0.1033f,   0.1058f,
    /* TpSiL*/  0.0654f,  -0.0107f,   0.1033f,   0.1258f,
    /* TpSiR*/  0.0654f,  -0.0107f,  -0.1033f,   0.1258f,
    /* TpBC */  0.0605f,  -0.0676f,   0.0000f,   0.1210f,
    /* BtFC */  0.0554f,   0.0693f,   0.0000f,  -0.1142f,
    /* BtFL */  0.0477f,   0.0446f,   0.0667f,  -0.1071f,
    /* BtFR */  0.0477f,   0.0446f,  -0.0667f,  -0.1071f,
};
inline constexpr float kItuCoeffs222L[] = {
    1.0f, 0.0f, 0.707f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f,
    0.354f, 0.0f, 0.4f, 0.0f, 0.3f, 0.0f, 0.2f, 0.15f,
    0.3f, 0.0f, 0.3f, 0.0f, 0.2f, 0.15f, 0.15f, 0.0f
};
inline constexpr float kItuCoeffs222R[] = {
    0.0f, 1.0f, 0.707f, 0.0f, 0.0f, 0.5f, 0.0f, 0.5f,
    0.354f, 0.0f, 0.0f, 0.4f, 0.0f, 0.3f, 0.2f, 0.15f,
    0.0f, 0.3f, 0.0f, 0.3f, 0.2f, 0.15f, 0.0f, 0.15f
};

// ===== AmbiX (4ch) =====
// Raw B-format output: W, Y, Z, X (ACN/SN3D order)
inline constexpr float kDecoderAmbiX[] = {
    // ch      W          X          Y          Z
    /* W  */  1.0f,       0.0f,      0.0f,      0.0f,
    /* Y  */  0.0f,       0.0f,      1.0f,      0.0f,
    /* Z  */  0.0f,       0.0f,      0.0f,      1.0f,
    /* X  */  0.0f,       1.0f,      0.0f,      0.0f,
};
inline constexpr float kItuCoeffsAmbiXL[] = { kInvSqrt2, kInvSqrt2, 0.0f, 0.0f };
inline constexpr float kItuCoeffsAmbiXR[] = { kInvSqrt2, -kInvSqrt2, 0.0f, 0.0f };

}  // namespace audio_plugin
