# Roland JX-3P Module — Signal Topology & Construction Review

> **Purpose**: Complete architectural reference for the Roland JX-3P emulation module in the AxelF synthesizer workstation. Covers signal flow, DSP algorithms, parameter map, modulation routing, and known issues.

---

## 1. Architecture Summary

| Property | Value |
|---|---|
| **Polyphony** | 6 voices (JUCE LRU voice stealing) |
| **Oscillators** | 2 × DCO (Sawtooth/Pulse/Square) with cross-mod and hard sync |
| **Filter** | IR3109 model — 4-pole OTA cascade with tanh soft-clip feedback |
| **Filter modes** | LP24 only |
| **Envelopes** | 2 × ADSR (Env1=Filter, Env2=Amplitude) |
| **LFO** | 1 × 5-shape with configurable delay fade-in |
| **Chorus** | Stereo 2-mode chorus (Mode I / Mode II) with LFO-modulated delay |
| **Anti-aliasing** | PolyBLEP on all DCO waveforms; 2× processor-level oversampling |
| **Saturation** | tanh on filter feedback + cubic soft-clip on output |
| **Parameter count** | 25 APVTS parameters (prefix: `jx3p_`) |
| **Source files** | 11 files in `Source/Modules/JX3P/` |

---

## 2. Voice Signal Flow

```
 ┌──────────────────────────── VOICE (×6) ────────────────────────────┐
 │                                                                     │
 │  MIDI Note On ──→ frequency                                        │
 │  Pitch Bend  ──→ ±2 semitones ─┐                                   │
 │                                 │                                   │
 │  ┌──────────────────────────────▼──────────────────────────────┐    │
 │  │              PITCH CALCULATION                              │    │
 │  │  pitchMod = 2^(bend/12) × (1 + lfoDepth × LFO × 0.02)    │    │
 │  │  crossModMul = 1 + crossMod × prevDco2Out                  │    │
 │  │  dco1Freq = noteFreq × pitchMod × crossModMul              │    │
 │  │  dco2Freq = noteFreq × pitchMod × 2^(detune/1200)          │    │
 │  └──────────────────────────────┬──────────────────────────────┘    │
 │                                 │                                   │
 │        ┌────────────────────────┼────────────────────┐              │
 │        │                        │                    │              │
 │        ▼                        ▼                    │              │
 │  ┌────────────┐          ┌────────────┐              │              │
 │  │   DCO-1    │          │   DCO-2    │              │              │
 │  │ Saw/Pulse/ │          │ Saw/Pulse/ │              │              │
 │  │ Square     │          │ Square     │              │              │
 │  │ PolyBLEP   │          │ PolyBLEP   │              │              │
 │  │ ×range     │          │ ×range     │              │              │
 │  └───┬────────┘          └───┬────────┘              │              │
 │      │  ▲ hard sync          │                       │              │
 │      │  │ (dco1 phase wrap   │                       │              │
 │      │  │  resets dco2)      │                       │              │
 │      │  └────────────────────┘                       │              │
 │      │                       ┌───────────────────────┘              │
 │      │                       │ FM: dco2→dco1 cross-mod             │
 │      │                       │                                      │
 │      ▼                       ▼                                      │
 │  ┌──────────────────────────────────────────────────────────────┐   │
 │  │                    MIXER BUS (Σ)                             │   │
 │  │  out = dco1 × mix1 + dco2 × mix2                            │   │
 │  │  (mix levels smoothed with 5ms ramp)                        │   │
 │  └─────────────────────────┬────────────────────────────────────┘   │
 │                            │                                        │
 │                            ▼                                        │
 │  ┌──────────────────────────────────────────────────────────────┐   │
 │  │               IR3109 FILTER (4-pole LPF)                    │   │
 │  │                                                              │   │
 │  │  Cutoff modulation:                                          │   │
 │  │    modCutoff = baseCutoff                                    │   │
 │  │              + vcfEnvDepth × ENV1 × 10000 Hz                 │   │
 │  │              + vcfLfoAmount × LFO × 2000 Hz                  │   │
 │  │              + keyTrackOffset                                │   │
 │  │    modCutoff = clamp(modCutoff, 20 Hz, 20 kHz)              │   │
 │  │                                                              │   │
 │  │  Topology: tanh(in − resonance × 3.8 × feedback)            │   │
 │  │            → 4× one-pole cascade                             │   │
 │  │  g = tan(π × cutoff / sr), G = g/(1+g)                     │   │
 │  └─────────────────────────┬────────────────────────────────────┘   │
 │                            │                                        │
 │                            ▼                                        │
 │  ┌──────────────────────────────────────────────────────────────┐   │
 │  │                    VCA / OUTPUT                               │   │
 │  │                                                              │   │
 │  │  output = filtered × ENV2 × velocity                        │   │
 │  │  output = softClip(output)      ← cubic soft-knee           │   │
 │  │  → stereo buffer (L=R, mono per voice)                       │   │
 │  └─────────────────────────┬────────────────────────────────────┘   │
 │                            │                                        │
 └────────────────────────────┼────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────────┐
                    │  2× Oversampler     │
                    │  (processor-level)  │
                    │  polyphase IIR      │
                    └─────────┬───────────┘
                              │
                              ▼
                    ┌─────────────────────┐
                    │  STEREO CHORUS      │
                    │  (post-synth,       │
                    │   native sr)        │
                    │  Mode I / Mode II   │
                    └─────────┬───────────┘
                              │
                              ▼
                    MasterMixer → Host DAW
```

---

## 3. Modulation Routing Matrix

| Source | Target(s) | Depth Control | Formula |
|---|---|---|---|
| **ENV 1** (Filter) | VCF cutoff | `jx3p_vcf_env_depth` (−1.0 to +1.0) | `cutoff + envDepth × env1 × 10000 Hz` |
| **ENV 2** (Amplitude) | VCA level | hardwired | `output × env2` |
| **LFO** | Pitch (DCO1+2) | `jx3p_lfo_depth` (0–1.0) | `freq × (1 + depth × lfo × 0.02)` — ±2% |
| **LFO** | VCF cutoff | `jx3p_vcf_lfo_amount` (0–1.0) | `cutoff + amount × lfo × 2000 Hz` |
| **Mod Wheel** | LFO depth boost | CC#1 (0–1.0) | `totalDepth = depth + modWheel × (1 − depth)` |
| **Velocity** | VCA gain | hardwired | `output × velocity` |
| **Keyboard** | VCF cutoff | `jx3p_vcf_key_track` (Off/Half/Full) | Off=0; Half=(note−60)×50 Hz; Full=(note−60)×100 Hz |
| **Pitch Bend** | Pitch (DCO1+2) | ±2 semitones fixed | `freq × 2^(bend/12)` |
| **Cross-Mod** | DCO-1 frequency | `jx3p_dco_cross_mod` (0–1.0) | `dco1Freq × (1 + crossMod × prevDco2Out)` — FM |
| **Hard Sync** | DCO-2 phase | `jx3p_dco_sync` (Off/On) | DCO-2 phase resets when DCO-1 wraps |

---

## 4. Envelope Specifications

Both envelopes share the `ADSREnvelope` implementation. Range: 1 ms – 10 s for A/D/R.

| Envelope | UI Name | Primary Target | Defaults (A/D/S/R) |
|---|---|---|---|
| Env 1 | Filter Env | VCF cutoff (±10 kHz) | 10ms / 300ms / 0.6 / 300ms |
| Env 2 | Amp Env | VCA level; voice active gate | 10ms / 250ms / 0.7 / 350ms |

**State machine**: Shared ADSR with exponential decay/release (same as Jupiter-8 and Moog 15).

**Voice lifecycle**: Env 2 gates voice active state. When release finishes, `clearCurrentNote()` frees the voice. `stopNote()` with `allowTailOff=false` immediately resets both envelopes and clears the voice.

---

## 5. LFO Detail

| Property | Spec |
|---|---|
| Shapes | Sine, Triangle, Sawtooth, Square, Sample & Hold |
| Rate | 0.05 – 50.0 Hz (log skew 0.3) |
| Delay | 0 – 3 seconds (linear fade-in from note-on) |
| Targets | Pitch (DCO1+2), Filter cutoff |
| Mod Wheel | CC#1 blends into LFO depth |

**Delay implementation**: Same as Jupiter-8 — `lfoDelayRamp = min(1.0, lfoDelaySamples / lfoDelayTarget)`, linear ramp.

---

## 6. Oscillator Detail (JX3PDCO)

### Waveforms & Anti-Aliasing

| Waveform | Formula | Anti-Alias |
|---|---|---|
| **Sawtooth** (0) | `2 × phase − 1` | PolyBLEP residual subtracted |
| **Pulse** (1) | `phase < pw ? 1 : −1` | PolyBLEP on rising + falling edges (width-aware) |
| **Square** (2) | `phase < 0.5 ? 1 : −1` | PolyBLEP on rising + falling edges |

### Range Mapping

```
phaseIncrement = frequency × (8.0 / range) / sampleRate
```
Range parameter: 2.0–16.0. Range=16→0.5× frequency (sub), range=8→1× (unity), range=4→2× (octave up).

### Hard Sync & Cross-Modulation

- **Hard sync**: DCO-1's `phaseWrapped` flag checked each sample. If enabled, `dco2.resetPhase()` on wrap.
- **Cross-mod**: `crossModMul = 1 + crossMod × prevDco2Out` applied to DCO-1 frequency. Previous sample's DCO-2 output used for 1-sample delay FM.

### Pulse Width

Pulse width parameter (`jx3p_pulse_width`, 0.05–0.95, default 0.5) affects the Pulse waveform. Square always uses 50% duty cycle.

---

## 7. IR3109 Filter Implementation

Same shared `IR3109Filter` class as Jupiter-8. See Jupiter-8 topology for full implementation.

### Key Specs
- **4-pole cascade**: g = tan(π×fc/sr), G = g/(1+g)
- **Feedback**: resonance × 3.8 × tanh(feedback)
- **Slope**: −24 dB/octave
- **Self-oscillation**: near resonance=1.0
- **No drive control** (unlike Moog Ladder)

---

## 8. Stereo Chorus (JX3PChorus)

### Architecture

```
 Synth output (stereo) ──→ [ Delay Line L ] ──→ wet L ──┐
                   │                                      ├──→ mix → L output
                   └──→ dry L ────────────────────────────┘
                   
                   ──→ [ Delay Line R ] ──→ wet R ──┐
                   │     (inverted LFO,              ├──→ mix → R output
                   └──→ dry R ──  spread offset) ────┘
```

### Mode Specifications

| Property | Mode I | Mode II |
|---|---|---|
| Base delay | 7 ms | 12 ms |
| LFO modulation | ±(depth × 3 ms) | ±(depth × 3 ms) |
| Wet amount | 50% | 70% |
| Character | Subtle, light chorus | Deeper, ensemble-like |

### Stereo Spread

- Right channel uses **inverted LFO** relative to left
- `spreadOffset = stereoSpread × 2.0 ms` added to right channel delay
- Creates decorrelated stereo width from 0 (mono) to wide

### Implementation Details

- **Delay buffer**: 4096 samples max (~93ms at 44.1 kHz)
- **Interpolation**: Linear (1st-order) for fractional delay reads
- **LFO**: Hardcoded sine wave (not user-selectable)
- **Rate**: 0.1–5.0 Hz; **Depth**: 0.0–1.0
- **Processing**: Post-synth, at native sample rate (not oversampled)

---

## 9. Voice Management

| Property | Value |
|---|---|
| Polyphony | 6 voices |
| Voice stealing | JUCE LRU (oldest active note; round-robin allocation) |
| Portamento | Not implemented (no glide parameter) |
| Unison modes | Not implemented |
| Parameter smoothing | 20ms for cutoff/resonance, 5ms for mix levels |
| Oversampling | 2× processor-wide before chorus |

**Note start**: `startNote()` sets sample rate on all DSP objects (DCOs, filter, envs, LFO), initializes smoothers (20ms/5ms), triggers env1/env2 `noteOn()`.

**Note stop**: `stopNote()` sends `noteOff()` to both envelopes. If `allowTailOff=false`, immediately resets and clears.

---

## 10. Complete Parameter Map

### DCO-1 (2 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jx3p_dco1_waveform` | Choice | Sawtooth, Pulse, Square | 0 (Sawtooth) | |
| `jx3p_dco1_range` | Float | 2.0–16.0 | 8.0 | Octave range |

### DCO-2 (3 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jx3p_dco2_waveform` | Choice | Sawtooth, Pulse, Square | 0 (Sawtooth) | |
| `jx3p_dco2_detune` | Float | −50 to +50 cents | 0.0 | Fine pitch offset |
| `jx3p_dco2_range` | Float | 2.0–16.0 | 8.0 | Octave range |

### VCF (5 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jx3p_vcf_cutoff` | Float (log) | 20–20000 Hz | 5000 | Skew 0.3 |
| `jx3p_vcf_resonance` | Float | 0.0–1.0 | 0.2 | Maps to 3.8× internal feedback |
| `jx3p_vcf_env_depth` | Float | −1.0 to +1.0 | 0.4 | Env1 depth (±10 kHz) |
| `jx3p_vcf_lfo_amount` | Float | 0.0–1.0 | 0.0 | LFO mod (±2 kHz) |
| `jx3p_vcf_key_track` | Choice | Off, Half, Full | 0 (Off) | Keyboard tracking |

### Envelope 1 — Filter (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jx3p_env_attack` | Float | 0.001–10.0 s | 0.01 | Skew 0.3 |
| `jx3p_env_decay` | Float | 0.001–10.0 s | 0.3 | |
| `jx3p_env_sustain` | Float | 0.0–1.0 | 0.6 | |
| `jx3p_env_release` | Float | 0.001–10.0 s | 0.3 | |

### Envelope 2 — Amplitude (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jx3p_env2_attack` | Float | 0.001–10.0 s | 0.01 | |
| `jx3p_env2_decay` | Float | 0.001–10.0 s | 0.25 | |
| `jx3p_env2_sustain` | Float | 0.0–1.0 | 0.7 | |
| `jx3p_env2_release` | Float | 0.001–10.0 s | 0.35 | |

### LFO (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jx3p_lfo_rate` | Float | 0.05–50.0 Hz | 5.0 | Log skew 0.3 |
| `jx3p_lfo_depth` | Float | 0.0–1.0 | 0.0 | Base pitch mod depth |
| `jx3p_lfo_waveform` | Choice | Sine, Triangle, Sawtooth, Square, S&H | 0 (Sine) | |
| `jx3p_lfo_delay` | Float | 0.0–3.0 s | 0.0 | Delay before LFO starts |

### Chorus (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jx3p_chorus_mode` | Choice | Off, I, II | 1 (Mode I) | Chorus character |
| `jx3p_chorus_rate` | Float | 0.1–5.0 Hz | 0.8 | Chorus LFO speed |
| `jx3p_chorus_depth` | Float | 0.0–1.0 | 0.5 | Modulation intensity |
| `jx3p_chorus_spread` | Float | 0.0–1.0 | 0.5 | Stereo width (0=mono, 1=wide) |

### Mixing & Modulation (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jx3p_mix_dco1` | Float | 0.0–1.0 | 0.7 | DCO-1 output level |
| `jx3p_mix_dco2` | Float | 0.0–1.0 | 0.5 | DCO-2 output level |
| `jx3p_dco_cross_mod` | Float | 0.0–1.0 | 0.0 | DCO-2→DCO-1 FM depth |
| `jx3p_dco_sync` | Choice | Off, On | 0 (Off) | Hard sync enable |

**Total: 25 parameters**

---

## 11. Known Issues & Construction Notes — Resolution Log

### Issue #1: No Noise Generator — ✅ RESOLVED

~~This emulation omits the noise generator entirely.~~ **Fixed:** Added xorshift PRNG noise generator with `jx3p_noise_level` parameter (0.0–1.0). Noise is mixed into the oscillator output path before the filter, matching the hardware signal flow. Per-voice `noiseState` PRNG seeded at 98765.

### Issue #2: No Portamento / Glide — ✅ RESOLVED

~~All note transitions are instantaneous.~~ **Fixed:** Added `jx3p_portamento` parameter (0.0–2.0 seconds). Uses exponential glide coefficient `1 - exp(-1/(rate×sr))` for natural-sounding pitch transitions. When portamento is at minimum (≤0.001), notes snap instantly (no glide). Voice tracks `portaFreq` independently from `noteFrequency`.

### Issue #3: Pulse Width Shared Between DCOs — ✅ RESOLVED

~~The same pulse width applies to both DCO-1 and DCO-2.~~ **Fixed:** Added `jx3p_dco2_pw` parameter (0.05–0.95) for independent DCO-2 pulse width. DCO-1 continues to use `jx3p_pulse_width` (also added to params, was previously read via `safeLoad` fallback but never declared). Each DCO now has fully independent pulse width control.

### Issue #4: LFO Filter Modulation Uses Linear Hz — ✅ RESOLVED

~~The filter LFO modulation adds a linear Hz offset (`vcfLfoAmount × lfo × 2000`).~~ **Fixed:** Replaced with multiplicative (log-frequency) scaling: `cutoff × 2^(amount × lfo × 2)`, providing ±2 octaves of musically consistent modulation. Matches the same fix applied to Jupiter-8 and Moog 15.

### Issue #5: Chorus Linear Interpolation May Produce Artifacts — ✅ RESOLVED

~~The chorus delay line uses linear (1st-order) interpolation.~~ **Fixed:** Replaced with Hermite (cubic) interpolation using 4 sample points. The `readInterpolated()` method now computes cubic polynomial coefficients `c0,c1,c2,c3` from points `idx0..idx3` and evaluates `((c3×frac+c2)×frac+c1)×frac+c0`. This preserves high-frequency content during modulation sweeps.

### Issue #6: No Sub-Oscillator — ℹ️ CORRECT OMISSION

The real JX-3P has no sub-oscillator. The range parameter (2–16) on each DCO allows playing at sub-octave frequencies. This is architecturally consistent with the hardware. No change needed.

### Issue #7: Chorus Processes at Native Sample Rate — ℹ️ CORRECT DESIGN

The chorus runs post-downsample at the native sample rate, not at 2× oversampled rate. This is correct — the chorus's LFO-modulated delay has no aliasing concerns. Cross-mod and sync artifacts in the voice are handled at 2× while chorus operates at 1×. No change needed.

---

## 12. Comparison: JX-3P vs Jupiter-8 vs PPG Wave Construction Quality

| Aspect | JX-3P | Jupiter-8 | PPG Wave | Assessment |
|---|---|---|---|---|
| **Anti-aliasing** | PolyBLEP all waves + 2× OS | PolyBLEP + sub BLEP + 2× OS | Mipmap + polyBLEP + 2× filter OS | All comparable |
| **Parameter smoothing** | 20ms cutoff/reso, 5ms mix | Same | Wave pos slew; filter coeff gated | Comparable |
| **Nonlinear modelling** | tanh feedback + cubic soft-clip | Same | tanh per-stage + output tanh | Comparable |
| **Effects** | Stereo chorus (2 modes) | Arpeggiator (variable rate) | None (unison spread) | JX-3P: unique chorus |
| **Modulation** | 2 envs, LFO+delay, cross-mod, sync, portamento | Same + arp, uni detune, HPF, voice modes | 3 envs, wave position mod | All now feature-rich |
| **Filter** | IR3109 LP24 + noise source | IR3109 LP24 + HPF | SSM 2044 4-mode | PPG: most versatile |
| **Missing features** | None (all fixed) | None (all fixed) | None (all fixed) | All complete |
| **Unique** | Built-in stereo chorus | Arpeggiator | Wavetable scanning | — |

---

## 13. Source File Reference

| File | Lines | Purpose |
|---|---|---|
| `Source/Modules/JX3P/JX3PProcessor.h` | ~70 | Module processor, 6-voice synth, 2× oversampler, chorus |
| `Source/Modules/JX3P/JX3PProcessor.cpp` | ~130 | processBlock with oversampling + chorus, parameter dispatch |
| `Source/Modules/JX3P/JX3PVoice.h` | ~80 | Voice: 2 DCOs, filter, 2 envs, LFO, smoothers |
| `Source/Modules/JX3P/JX3PVoice.cpp` | ~200 | renderNextBlock (sample-by-sample), startNote, stopNote |
| `Source/Modules/JX3P/JX3PDCO.h` | ~40 | DCO: phase, waveform, range, pulse width |
| `Source/Modules/JX3P/JX3PDCO.cpp` | ~130 | getNextSample with PolyBLEP, 3 waveforms |
| `Source/Modules/JX3P/JX3PChorus.h` | ~40 | Chorus: delay buffers, LFO, stereo spread |
| `Source/Modules/JX3P/JX3PChorus.cpp` | ~120 | processStereo, interpolated delay, 2 modes |
| `Source/Modules/JX3P/JX3PParams.h` | ~10 | Parameter layout declaration |
| `Source/Modules/JX3P/JX3PParams.cpp` | ~130 | createParameterLayout (25 params) |
| `Source/Modules/JX3P/JX3PEditor.h/.cpp` | ~500 | UI components |
