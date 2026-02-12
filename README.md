# UpmixRT

Real-time stereo-to-multichannel upmix audio plugin using first-order Ambisonics. Available as VST3, AU, and Standalone.

## How it works

UpmixRT doesn't simply copy the stereo signal to every speaker. It analyzes the spatial properties of the input and reconstructs a 3D sound field that distributes audio intelligently across the target speaker layout.

### 1. Spatial analysis

The stereo input is split into 8 frequency bands. For each band, the plugin measures:

- **Inter-channel correlation (ICC)** — how similar L and R are. High correlation means centered content (vocals, bass); low correlation means wide, ambient content (reverb, room tone).
- **Azimuth** — where the sound sits in the stereo panorama.
- **Diffuseness** — derived from ICC. Ambient and reverberant content scores high.
- **Elevation** — inferred from the spectral energy distribution.

These per-band values are energy-weighted into a single set of spatial parameters per sample.

### 2. Ambisonic encoding

The spatial parameters drive a first-order Ambisonic (B-format) encoder that produces four channels:

| Channel | Role | Source |
|---------|------|--------|
| **W** | Omnidirectional (sum) | `(L + R) / sqrt(2)` — exact, phaseless |
| **Y** | Left-right (difference) | `(L - R) / sqrt(2)` — exact, phaseless |
| **X** | Front-back | Correlated content placed forward; decorrelated diffuse content spread via allpass chains |
| **Z** | Up-down | Driven by estimated elevation + decorrelated diffuse content |

W and Y preserve the original stereo image bit-exactly. X and Z add spatial depth without altering the L/R information.

### 3. Speaker decoding

A per-layout decoder matrix maps the 4 B-format channels to the target speaker configuration. Each speaker gets a unique blend weighted by its physical position — not a copy of the input.

The LFE channel receives a lowpass-filtered version of W at -10 dB.

### 4. Output routing

- **Main output 1-2** is always dry stereo passthrough.
- **Upmix channels** are routed only to multi-out aux outputs (labeled `1/2`, `3/4`, ...).
- For full **22.2 wet** routing, enable aux outputs up to **23-24**.
- **Dry/Wet** controls only the aux upmix level.

### Key properties

- **Content-adaptive**: correlated content (vocals, dialog) stays up front; diffuse content (reverb, ambience) spreads to surrounds and height channels.
- **Mathematically reversible**: decoder matrices are constrained so that an ITU-standard downmix of the output reconstructs the original stereo input within float precision.
- **Zero reported latency**: the main signal path (W, Y) is pure arithmetic. Only the X/Z decorrelators introduce a small delay (~4 ms) for spatial enrichment.
- **Real-time safe**: no allocations, locks, or system calls in the audio path. All processing is sample-by-sample.

## Supported layouts

| Layout | Channels |
|--------|----------|
| Stereo | 2 |
| 5.1 | 6 |
| 7.1.4 | 12 |
| 9.1.6 | 16 |
| 22.2 | 24 |
| AmbiX (B-format) | 4 |

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Layout | Stereo / 5.1 / 7.1.4 / 9.1.6 / 22.2 / AmbiX | 5.1 | Target speaker configuration |
| Dry/Wet | 0–100% | 100% | Wet level for aux multi-out upmix channels |
| Gain | -42 to 0 dB | 0 dB | Wet aux output gain (channels 3+) |

## Building

Requires CMake 3.22+, a C++20 compiler, and Ninja.

```bash
# macOS: install Ninja if needed
brew install ninja

# Build
cmake --preset default && cmake --build build

# Run tests
ctest --preset default
```

JUCE and GoogleTest are fetched automatically via CPM.

## License

[MIT](LICENSE.md)
