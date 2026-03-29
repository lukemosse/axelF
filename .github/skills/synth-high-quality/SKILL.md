---
name: synth-high-quality
description: >
  Use this skill when building, designing, reviewing, or auditing a software
  synthesizer with a focus on audio quality, fidelity, or analog authenticity.
  Trigger when the user asks about oscillator aliasing, filter modeling, ZDF
  filters, oversampling, VA synthesis, band-limited waveforms, BLEP, wavetable
  mipmapping, unison quality, modulation accuracy, gain staging, denormals, SIMD
  voice parallelism, or anything related to making a synth sound better or more
  "analog". Also trigger for questions like "why does my synth sound digital/harsh/
  thin/sterile", "how do I improve my filter", "my FM aliases badly", or "how do I
  model analog drift". Apply proactively when writing or reviewing any synth DSP
  code where quality is a concern — don't wait to be asked.
---

# High-Quality Software Synthesizer Design

A reference for building software synthesizers at the highest possible audio
quality. Covers oscillators, filters, modulation, voice architecture, signal
path, and analog modeling.

---

## Quality Pillar Overview

| Pillar | Key Concern | Primary Technique |
|---|---|---|
| Oscillators | Aliasing | BLEP / wavetable mipmap / oversampling |
| Filters | Accuracy + character | ZDF / TPT / nonlinear integrators |
| Modulation | Smoothness + speed | Sample-accurate, band-limited LFOs |
| Voice arch | Width + life | Unison drift, per-voice spread |
| Signal path | Headroom + warmth | Gain staging, soft saturation, DC blocking |
| Oversampling | Nonlinear aliasing | 2x–8x with good decimation filter |
| Analog modeling | Liveness | Component spread, thermal drift, saturation |
| CPU efficiency | Quality headroom | SIMD, denormal prevention, fast math |

---

## 1. Oscillator Quality

### Band-Limited Waveforms

Naive sawtooth, square, and pulse waves contain harmonics above Nyquist that
fold back as audible aliasing — false tones unrelated to the note pitch.

**BLEP (Band-Limited Step):**
Correct the discontinuity at each waveform edge using a precomputed correction
residual. Cheap, effective, and sample-accurate.

```cpp
// At each discontinuity (e.g. saw reset), add a BLEP correction:
// t = fractional position of discontinuity within current sample (0..1)
// scale = height of the step (+1 for rising, -1 for falling)
float blep(float t, float scale) {
    // Precomputed MinBLEP table lookup (see minblep-generation tools)
    // or use a polynomial approximation:
    if (t < 1.0f) {
        float u = t - 0.5f;
        return scale * (u * u * 2.0f - 0.5f); // simple 1-sample poly BLEP
    }
    return 0.0f;
}
```

For best results use **MinBLEP** (minimum-phase BLEP) — precompute a table using
the Parks-McClellan algorithm. Many open-source implementations exist (e.g.
Surge's `OscillatorBase`).

**Wavetable with Mipmapping:**
Store multiple band-limited copies of each waveform, each with harmonics above
`Nyquist / (2^mip)` removed. Select the mip level based on current pitch.

```cpp
int mipLevel = (int)floorf(log2f(sample_rate / (2.0f * freq)));
mipLevel = clamp(mipLevel, 0, NUM_MIPS - 1);
float sample = wavetable[mipLevel][phaseIndex];
```

**Phase accumulator precision:** Use `double` for phase, cast to `float` for
the waveform lookup. At high pitches, `float` phase accumulation introduces
pitch quantisation errors.

```cpp
double phase_acc = 0.0;
double phase_inc = freq / sample_rate;

// Per sample:
phase_acc += phase_inc;
if (phase_acc >= 1.0) phase_acc -= 1.0;
float sample = lookupWaveform((float)phase_acc);
```

---

## 2. Filter Quality

Filters define a synth's character more than almost anything else.

### Zero-Delay Feedback (ZDF) / TPT Filters

Classical digital filter implementations have a **one-sample delay in the
feedback path**. This causes:
- Incorrect resonance behaviour, especially at high cutoff frequencies
- Inaccurate self-oscillation
- Cutoff frequency error that worsens as cutoff approaches Nyquist

**ZDF (Zero-Delay Feedback)** solves this by solving the implicit feedback
equation each sample using the **Trapezoidal integrator**:

```
Trapezoidal integrator state update:
  v  = (x - z) / (1 + g)     where g = tan(pi * fc / fs)
  y  = v + z
  z  = y + v                 (new state)
```

The **TPT (Topology-Preserving Transform)** framework by Vadim Zavalishin
provides a systematic way to derive ZDF versions of any analog filter topology.

**Authoritative reference:** *The Art of VA Filter Design* by Vadim Zavalishin
— free PDF at https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.0.pdf

### Nonlinear (Saturating) Filters

Linear filters sound harsh at high resonance. Real analog filters saturate:

```cpp
// Add tanh saturation inside each integrator stage:
float NonlinearIntegrator::process(float x) {
    float v = (tanhf(x) - tanhf(state)) * g / (1.0f + g);
    float y = v + state;
    state = y + v;
    return tanhf(y); // saturate output too
}
```

`tanhf()` models transistor/op-amp soft saturation. For CPU efficiency, use a
fast tanh approximation:

```cpp
// Pade approximation — accurate to ~0.5% up to |x| < 4
inline float fastTanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
```

### Oversampling the Filter

For nonlinear filters, oversample at **2x–4x** to eliminate aliasing from the
saturation nonlinearity. Upsample input → process filter → downsample output.
Use a steep elliptic or Chebyshev Type II FIR for the decimation filter.

---

## 3. Modulation System

### Sample-Accurate Modulation

LFOs, envelopes, and mod matrix values must update **every sample**, not once
per block. Block-rate modulation causes audible zipper noise and loses the
character of fast modulation (e.g., LFO speeds above ~100Hz become trivially
audible as pitched tones when block-quantised).

```cpp
// In the render loop, update all mod sources per sample:
for (int i = 0; i < blockSize; ++i) {
    float lfoVal  = lfo.tick();
    float envVal  = envelope.tick();
    float cutoff  = baseCutoff + lfoVal * lfoDepth + envVal * envDepth;
    filter.setCutoff(cutoff); // smoother handles per-sample zipper internally
    output[i] = filter.process(osc.tick());
}
```

### Band-Limited LFOs

At fast LFO rates (>50Hz), triangle and sawtooth LFO waveforms alias audibly.
Apply the same BLEP treatment as audio oscillators, or use a sine-based LFO
which is naturally band-limited.

### Exponential Scaling

**Pitch and filter cutoff** should use exponential (1V/oct) scaling so that
equal mod amounts produce equal musical intervals at any base frequency:

```cpp
float cutoffHz = baseCutoff * powf(2.0f, modAmount / 12.0f); // semitone scaling
float pitch_hz = 440.0f * powf(2.0f, (noteNumber - 69 + pitchBend) / 12.0f);
```

Use linear scaling for pan, level, and mix amounts.

---

## 4. Voice Architecture

### Unison Quality

Cheap unison shifts voices by fixed cent offsets. High-quality unison:

```cpp
struct UnisonVoice {
    float detune_cents;       // base spread
    float drift_phase;        // independent slow LFO phase
    float drift_depth;        // ±0.5–2 cents of slow random drift
    float stereo_pan;         // spread across stereo field
};

// Per-sample detune with drift:
float freq_mult = centsToRatio(
    voice.detune_cents +
    sinf(voice.drift_phase) * voice.drift_depth
);
voice.drift_phase += drift_rate; // drift_rate ≈ 0.1–2 Hz in radians/sample
```

Additionally vary per-voice:
- Initial phase offset (or leave free-running for more organic sound)
- Slight per-voice filter cutoff offset (±2–5 cents equivalent)
- Stereo panning spread

### Per-Voice Analog Drift

Real analog polysynths have component tolerances — each voice sounds slightly
different. Model this with slow, independent random walk per voice:

```cpp
// Initialise once per voice (random seed per voice):
voice.pitch_drift   = gaussianRandom() * 0.5f; // ±0.5 cents initial offset
voice.filter_drift  = gaussianRandom() * 0.02f; // ±2% cutoff offset
voice.level_drift   = 1.0f + gaussianRandom() * 0.01f; // ±1% level

// Drift slowly over time (update at a slow rate, e.g. once per buffer):
voice.pitch_drift  += gaussianRandom() * 0.001f;
voice.pitch_drift   = clamp(voice.pitch_drift, -1.5f, 1.5f);
```

---

## 5. Signal Path & Gain Staging

### Internal Headroom

- Run all internal arithmetic in **32-bit float minimum**
- Use **64-bit double** for phase accumulators and long feedback paths
- Keep signal levels at **−12 to −18 dBFS** through the chain to leave headroom
  for resonance peaks, unison stacking, and modulation extremes

### DC Blocking

Apply a DC blocker after any stage that can produce DC offset (pulse oscillators,
asymmetric waveshapers, feedback FM). See the `synth-click-prevention` skill for
the canonical implementation.

### Output Soft Saturation

A gentle soft clipper at the master output prevents hard digital clipping and
adds warmth analogous to analog output transformers:

```cpp
// Soft knee limiter (tanh-based):
inline float softLimit(float x, float drive = 1.0f) {
    return tanhf(x * drive) / drive;
}

// Or a polynomial soft clip (cheaper):
inline float softClip(float x) {
    // Clamps to ±1 with soft knee
    x = clamp(x, -1.5f, 1.5f);
    return x - (x * x * x) / 3.0f; // cubic soft clip
}
```

Apply `drive` values of 1.1–1.3 for subtle warmth; higher for intentional grit.

---

## 6. Oversampling Strategy

| Nonlinearity Severity | Recommended Oversampling |
|---|---|
| None (purely linear) | 1x (none needed) |
| Mild saturation (tanh in filter) | 2x |
| Moderate (waveshaper, FM) | 4x |
| Heavy (hard clip, ring mod, aggressive FM) | 8x |

**Decimation filter choice:**
- **Minimum-phase FIR** — lower latency, slightly non-flat phase (preferred for
  instruments where latency matters)
- **Linear-phase FIR** — flat phase, higher latency (preferred for mixing/mastering)
- **Elliptic IIR** — very cheap, steep roll-off, but phase nonlinearity; OK for
  moderate oversampling ratios

```cpp
// Typical 2x oversampling loop:
for (int i = 0; i < blockSize; ++i) {
    // Upsample (insert zero or use polyphase filter)
    float up0 = upsampleFilter.process(input[i]);
    float up1 = upsampleFilter.process(0.0f);

    // Process at 2x rate
    float proc0 = nonlinearStage.process(up0);
    float proc1 = nonlinearStage.process(up1);

    // Decimate
    output[i] = decimateFilter.process(proc0, proc1);
}
```

---

## 7. Analog Modeling Principles

**The core principle:** model the *physics* of the circuit, not just its
transfer function. Analog character comes from:

- **Nonlinearity** in every active stage (transistors, op-amps)
- **Component tolerance spread** across voices and units
- **Thermal drift** over time (values shift as components warm up)
- **Parasitic effects** (capacitance, inductance causing subtle frequency shifts)
- **Power supply sag** under heavy load (transient pitch/level drops on loud notes)

**Checklist for authentic analog modeling:**
- [ ] Nonlinear saturation inside filter feedback loops (not just at output)
- [ ] Per-voice component spread (pitch, filter, level)
- [ ] Slow thermal drift on filter cutoff and oscillator pitch
- [ ] Input transformer / output transformer saturation (even-order harmonics)
- [ ] VCA exponential law with slight compression at high levels
- [ ] Envelope CV slew — real envelopes have finite slew rate on attack

---

## 8. CPU Efficiency for Quality

Efficiency enables higher oversampling ratios, more voices, and more complex
algorithms. It is not the enemy of quality.

### SIMD Voice Parallelism

Process 4 (SSE) or 8 (AVX) voices in parallel using SIMD:

```cpp
#include <immintrin.h>

// Process 4 voices simultaneously:
__m128 phase4    = _mm_loadu_ps(phases);
__m128 phaseInc4 = _mm_loadu_ps(phaseIncs);
phase4 = _mm_add_ps(phase4, phaseInc4);
// wrap, lookup, etc. — 4 voices for the price of ~1
```

In Rust, use `std::simd` or the `packed_simd` crate. In JUCE, use
`juce::FloatVectorOperations`.

### Denormal Prevention

Denormal floats (values very close to zero) cause massive CPU slowdowns in the
filter state variables of quiet or silent voices.

```cpp
// Add a DC offset too small to hear but large enough to prevent denormals:
const float ANTI_DENORMAL = 1e-20f;

// Apply to filter state each sample:
state += ANTI_DENORMAL;
```

Or flush denormals at the thread level (x86):
```cpp
_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
```

### Fast Math for Audio

```cpp
// Fast approximations where full precision isn't needed:
// sin (LFO, oscillator) — use a polynomial or table
// exp (envelope curves, exponential FM) — use expf() or a fast approx
// tanh (saturation) — use fastTanh() polynomial above
// pow2 (pitch conversion) — use bit-manipulation trick or table
inline float fastPow2(float x) {
    union { float f; int i; } u;
    u.i = (int)(x * 8388608.0f) + 0x3f800000;
    return u.f;
}
```

---

## Key Reference Materials

| Resource | Topic | Access |
|---|---|---|
| *The Art of VA Filter Design* — Vadim Zavalishin | ZDF/TPT filters | Free PDF (NI website) |
| Julius O. Smith — Physical Audio Signal Processing | Physical modeling | Free at ccrma.stanford.edu |
| Julius O. Smith — Introduction to Digital Filters | Filter theory | Free at ccrma.stanford.edu |
| Surge Synthesizer source code | Production BLEP, filters, modulation | github.com/surge-synthesizer/surge |
| Vital Synthesizer source code | Modern wavetable, ZDF | github.com/mtytel/vital |
| *Designing Software Synthesizer Plugins* — Will Pirkle | Broad synth DSP | Book (paid) |
