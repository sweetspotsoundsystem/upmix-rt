# UpmixRT

Real-time stereo-to-multichannel upmix JUCE plugin via first-order Ambisonics.

## Build

```
cmake --preset default && cmake --build build
ctest --preset default
```

Requires Ninja (`brew install ninja`). Formats: Standalone, VST3, AU.

## Architecture

Stereo input is processed sample-by-sample through four stages:

```
L, R ──→ SpatialAnalyzer ──→ SpatialParams (icc, azimuth, diffuseness, elevation)
L, R ──→ AmbisonicEncoder ──→ B-format [W, X, Y, Z]
B-format ──→ AmbisonicDecoder ──→ speaker feeds [up to 64 ch]
speakers ──→ OutputWriter ──→ JUCE output buffer (main dry, aux wet)
```

The filter bank is **analysis-only** -- audio never passes through it. W and Y are computed directly from raw input samples (phaseless). X and Z add spatial enrichment via decorrelated allpass chains.

### Signal path (per sample in processBlock)

1. `SpatialAnalyzer::process(L, R)` -- 8-band LR2 filter bank extracts per-band ICC, azimuth, energy; energy-weighted averages produce `SpatialParams`
2. `AmbisonicEncoder::encode(L, R, params)` -- W = (L+R)/sqrt2, Y = (L-R)/sqrt2 (exact), X/Z from spatial enrichment + decorrelated diffuse
3. `AmbisonicDecoder::decode(bFormat, layout)` -- ITU-constrained matrix multiply + LFE lowpass + click-free crossfade on layout change
4. `OutputWriter::writeSample(...)` -- main 1-2 are dry passthrough, channels 3+ carry smoothed wet upmix, output gain applied

## Invariants

These properties are verified by tests and must never be broken:

- **Phaseless encoding**: `W = (L+R) * kInvSqrt2` and `Y = (L-R) * kInvSqrt2` must be bit-exact on every sample, regardless of spatial parameters
- **ITU downmix reconstruction**: for every layout, `ITU_downmix(decode(encode(L,R))) == (L, R)` within float precision (<1e-4)
- **Constants.h is frozen** -- do not modify enums, structs, parameter IDs, or constant values
- **Zero reported latency** -- the direct signal path is pure arithmetic; decorrelator delays are intentional spatial effects
- **Real-time safety** -- no allocations, locks, or system calls in processBlock

## ITU downmix constraint

Each decoder matrix `D[spk][ambi_ch]` in `SpeakerLayout.h` satisfies:

```
sum(ituL[spk] * D[spk][W]) = 1/sqrt(2)     # W reconstructs L
sum(ituL[spk] * D[spk][Y]) = 1/sqrt(2)     # Y reconstructs L
sum(ituL[spk] * D[spk][X]) = 0             # X cancels in downmix
sum(ituL[spk] * D[spk][Z]) = 0             # Z cancels in downmix
```

Mirror for R with Y target = -1/sqrt(2). If you modify decoder matrices, you must re-verify these constraints. The test suite checks them for all layouts.

## Layouts

| Layout | Channels | LFE index |
|--------|----------|-----------|
| Stereo | 2 | none |
| 5.1 | 6 | 3 |
| 7.1.4 | 12 | 3 |
| 9.1.6 | 16 | 3 |
| 22.2 | 24 | 3 |
| AmbiX | 4 | none |

## Key files

| File | Role |
|------|------|
| `plugin/include/UpmixRT/Constants.h` | Frozen interface: enums, structs, constants |
| `plugin/include/UpmixRT/SpeakerLayout.h` | Decoder matrices + ITU coefficients (constexpr) |
| `plugin/source/PluginProcessor.cpp` | Main processBlock loop |
| `plugin/source/AmbisonicEncoder.cpp` | Phaseless W/Y + enriched X/Z |
| `plugin/source/AmbisonicDecoder.cpp` | ITU-constrained decode + LFE + crossfade |
| `plugin/source/SpatialAnalyzer.cpp` | Orchestrates filter bank + analysis bands |
| `plugin/source/Decorrelator.cpp` | Allpass chains for diffuse field |
| `test/source/AudioProcessorTest.cpp` | 60 tests covering all invariants |

## Code conventions

- Namespace: `audio_plugin`
- C++20, strict warnings (`-Werror`), no exceptions
- Use `std::clamp()` for float clamping (not `std::max`/`std::min` -- triggers `-Wdouble-promotion`)
- All DSP state allocated in `prepare()`, never in `process()`
- EMA smoothing: `alpha = 1 - exp(-1 / (sampleRate * timeSec))`

## Parameters

| ID | Type | Range | Default |
|----|------|-------|---------|
| `layout` | choice | 0-5 (Stereo/5.1/7.1.4/9.1.6/22.2/AmbiX) | 1 (5.1) |
| `drywet` | float | 0.0-1.0 | 1.0 |
| `gain` | float | -42.0-0.0 dB | 0.0 (unity) |

## Testing

Tests are in `test/source/AudioProcessorTest.cpp`. Run with `ctest --preset default`.

When adding new DSP code, add tests that verify:
- Phaseless property (W and Y remain exact)
- ITU downmix round-trip for all layouts
- Silence in produces silence out
- No samples exceed 1.0 during parameter transitions
- Allpass magnitude preservation for decorrelators
