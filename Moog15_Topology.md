# Moog Modular 15 Module — Signal Topology & Construction Review

> **Purpose**: Complete architectural reference for the Moog Modular 15 emulation module in the AxelF synthesizer workstation. Covers signal flow, DSP algorithms, parameter map, modulation routing, and known issues.

---

## 1. Architecture Summary

| Property | Value |
|---|---|
| **Polyphony** | 1 voice (monophonic bass synth) |
| **Oscillators** | 3 × VCO (Sawtooth/Triangle/Pulse/Square) |
| **Filter** | Moog Ladder model — 4-pole cascade with tanh saturation + drive |
| **Filter modes** | LP24 only |
| **Envelopes** | 2 × ADSR (Env1=Filter, Env2=Amplitude) |
| **LFO** | 1 × 5-shape with mod wheel integration |
| **Anti-aliasing** | PolyBLEP on saw/pulse/square; 2× processor-level oversampling |
| **Saturation** | tanh on filter input + drive control + cubic soft-clip on output |
| **Glide** | Portamento with configurable rate |
| **Parameter count** | 36 APVTS parameters (prefix: `moog_`) |
| **Source files** | 8 files in `Source/Modules/Moog15/` + 3 shared DSP files |

---

## 2. Voice Signal Flow

```
 ┌──────────────────── VOICE (×1, MONOPHONIC) ────────────────────────┐
 │                                                                     │
 │  MIDI Note On ──→ glideTarget ──→ Glide (portamento)               │
 │  Pitch Bend  ──→ ±semitones ──┐                                    │
 │                                │                                    │
 │  ┌─────────────────────────────▼──────────────────────────────┐     │
 │  │              PITCH CALCULATION                             │     │
 │  │  pitchMod = 2^(bend/12) × (1 + lfoPitchMod)              │     │
 │  │  lfoPitchMod = totalLfoDepth × LFO × 0.02                │     │
 │  │                + lfoPitchAmt × LFO × 0.02                 │     │
 │  │  vco1Freq = currentFreq × pitchMod                        │     │
 │  │  vco2Freq = currentFreq × pitchMod × 2^(detune2/1200)    │     │
 │  │  vco3Freq = currentFreq × pitchMod × 2^(detune3/1200)    │     │
 │  └─────────────────────────────┬──────────────────────────────┘     │
 │                                │                                    │
 │     ┌──────────────────────────┼──────────────────────┐             │
 │     │                          │                      │             │
 │     ▼                          ▼                      ▼             │
 │ ┌────────────┐          ┌────────────┐         ┌────────────┐      │
 │ │   VCO-1    │          │   VCO-2    │         │   VCO-3    │      │
 │ │ Saw/Tri/   │          │ Saw/Tri/   │         │ Saw/Tri/   │      │
 │ │ Pulse/Sq   │          │ Pulse/Sq   │         │ Pulse/Sq   │      │
 │ │ PolyBLEP   │          │ PolyBLEP   │         │ PolyBLEP   │      │
 │ │ ×range     │          │ ×range     │         │ ×range     │      │
 │ │ level 0.8  │          │ level 0.6  │         │ level 0.4  │      │
 │ └───┬────────┘          └───┬────────┘         └───┬────────┘      │
 │     │                       │                      │                │
 │     │                       │      ┌───────────────┘                │
 │     │                       │      │                                │
 │     │                       │      │  ┌────────────────────┐        │
 │     │                       │      │  │  WHITE NOISE (LCG) │        │
 │     │                       │      │  │  xorshift PRNG     │        │
 │     │                       │      │  └─────────┬──────────┘        │
 │     │                       │      │            │                   │
 │     ▼                       ▼      ▼            ▼                   │
 │ ┌──────────────────────────────────────────────────────────────┐    │
 │ │                    MIXER BUS (Σ)                             │    │
 │ │  out = vco1 × mix1 + vco2 × mix2 + vco3 × mix3             │    │
 │ │        + noise × noiseLevel                                  │    │
 │ │  (mix levels smoothed with 5ms ramp)                        │    │
 │ └─────────────────────────┬────────────────────────────────────┘    │
 │                           │                                         │
 │                           ▼                                         │
 │ ┌──────────────────────────────────────────────────────────────┐    │
 │ │             MOOG LADDER FILTER (4-pole LPF)                 │    │
 │ │                                                              │    │
 │ │  Cutoff modulation:                                          │    │
 │ │    modCutoff = baseCutoff                                    │    │
 │ │              + vcfEnvDepth × ENV1 × 10000 Hz                 │    │
 │ │              + lfoDepth × LFO × 3000 Hz                      │    │
 │ │              + vcfLfoAmount × LFO × 2000 Hz                  │    │
 │ │              + keyTrackOffset                                │    │
 │ │    modCutoff = clamp(modCutoff, 20 Hz, 20 kHz)              │    │
 │ │                                                              │    │
 │ │  Topology: tanh(in × drive − resonance × 4.0 × feedback)   │    │
 │ │            → 4× one-pole cascade (Huovilainen model)        │    │
 │ │  Drive: 1.0–5.0 (controls tanh saturation intensity)        │    │
 │ └─────────────────────────┬────────────────────────────────────┘    │
 │                           │                                         │
 │                           ▼                                         │
 │ ┌──────────────────────────────────────────────────────────────┐    │
 │ │                    VCA / OUTPUT                               │    │
 │ │                                                              │    │
 │ │  output = filtered × ENV2 × velocity                        │    │
 │ │  output = softClip(output)      ← cubic soft-knee           │    │
 │ │  → stereo buffer (L=R, mono)                                │    │
 │ └─────────────────────────┬────────────────────────────────────┘    │
 │                           │                                         │
 └───────────────────────────┼─────────────────────────────────────────┘
                             │
                             ▼
                   ┌─────────────────────┐
                   │  2× Oversampler     │
                   │  (processor-level)  │
                   │  polyphase IIR      │
                   └─────────┬───────────┘
                             │
                             ▼
                   MasterMixer → Host DAW
```

---

## 3. Modulation Routing Matrix

| Source | Target(s) | Depth Control | Formula |
|---|---|---|---|
| **ENV 1** (Filter) | VCF cutoff | `moog_vcf_env_depth` (−1.0 to +1.0) | `cutoff + envDepth × env1 × 10000 Hz` |
| **ENV 2** (Amplitude) | VCA level | hardwired | `output × env2` |
| **LFO** | Pitch (VCO1+2+3) | `moog_lfo_depth` (0–1.0) | `freq × (1 + depth × lfo × 0.02)` — ±2% |
| **LFO** | Pitch (via pitch amt) | `moog_lfo_pitch_amount` (0–1.0) | `freq × (1 + pitchAmt × lfo × 0.02)` — stacked |
| **LFO** | VCF cutoff (depth path) | `moog_lfo_depth` (0–1.0) | `cutoff + lfoDepth × lfo × 3000 Hz` |
| **LFO** | VCF cutoff (vcf path) | `moog_vcf_lfo_amount` (0–1.0) | `cutoff + vcfLfoAmt × lfo × 2000 Hz` |
| **Mod Wheel** | LFO depth boost | CC#1 (0–1.0) | `totalDepth = depth + modWheel × (1 − depth)` |
| **Velocity** | VCA gain | hardwired | `output × velocity` |
| **Keyboard** | VCF cutoff | `moog_vcf_key_track` (Off/33%/67%/100%) | Off=0; 33%=(note−60)×50 Hz; 67%=(note−60)×100 Hz |
| **Pitch Bend** | Pitch (all VCOs) | ±semitones from wheel | `freq × 2^(bend/12)` |
| **Glide** | Pitch portamento | `moog_glide_time` (0–2 s) | `currentFreq += (target − currentFreq) × glideSpeed` |

---

## 4. Envelope Specifications

Both envelopes share the `ADSREnvelope` implementation. Range: 1 ms – 10 s for A/D/R.

| Envelope | UI Name | Primary Target | Defaults (A/D/S/R) |
|---|---|---|---|
| Env 1 | Filter Env | VCF cutoff (±10 kHz) | 5ms / 500ms / 0.3 / 300ms |
| Env 2 | Amp Env | VCA level; voice active gate | 5ms / 400ms / 0.5 / 200ms |

**State machine**: Same exponential-decay ADSR as Jupiter-8 (shared `ADSREnvelope` class).

**Voice lifecycle**: Monophonic — new MIDI note steals the single voice. Env 2 gates voice active state.

---

## 5. LFO Detail

| Property | Spec |
|---|---|
| Shapes | Sine, Triangle, Sawtooth, Square, Sample & Hold |
| Rate | 0.01 – 30 Hz (log skew 0.3) |
| Delay | None (no delay parameter on Moog module) |
| Targets | Pitch (all VCOs via two separate depth controls), Filter cutoff (two paths) |
| Mod Wheel | CC#1 blends into LFO depth: `totalDepth = depth + modWheel × (1−depth)` |

**Dual LFO filter modulation**: The Moog module uniquely has two LFO→filter paths that stack additively:
1. `lfoDepthAmount × lfo × 3000 Hz` — general LFO depth (also affects pitch)
2. `vcfLfoAmount × lfo × 2000 Hz` — dedicated VCF LFO amount

Total filter LFO swing can reach ±5000 Hz at full settings.

---

## 6. Oscillator Detail (Moog15VCO)

### Waveforms & Anti-Aliasing

| Waveform | Formula | Anti-Alias |
|---|---|---|
| **Sawtooth** (0) | `2 × phase − 1` | PolyBLEP residual subtracted |
| **Triangle** (1) | `2 × abs(2×phase − 1) − 1` | None (no discontinuities) |
| **Pulse** (2) | `phase < pulseWidth ? 1 : −1` | PolyBLEP on 2 edges (width-dependent) |
| **Square** (3) | `phase < 0.5 ? 1 : −1` | PolyBLEP on rising + falling edges |

### Range Mapping

```
phaseIncrement = frequency × (16.0 / range) / sampleRate
```
Range parameter: 2.0–32.0 (default 16 = unity). Range=32→0.5× frequency, range=16→1×, range=8→2×, etc.

### Three-Oscillator Configuration

| VCO | Range Default | Detune Range | Detune Default | Level Default |
|---|---|---|---|---|
| VCO-1 | 16 (unity) | — | — | 0.8 |
| VCO-2 | 16 (unity) | ±100 cents | 0 | 0.6 |
| VCO-3 | 16 (unity) | ±100 cents | 0 | 0.4 |

**No cross-modulation, no hard sync, no ring mod**. Oscillators are purely additive, mixed via independent level controls.

---

## 7. Moog Ladder Filter Implementation

### Topology

```
Input ──→ [tanh(input × drive − resonance × 4.0 × feedback)] ──→ saturated
                                                                      │
                     ┌────────────────────────────────────────────────┘
                     │
                     │  fc = 2 × sr × tan(π × cutoff / sr)
                     │  g = fc / (2 × sr)
                     │  G = g / (1 + g)
                     │
                     │  Stage 1: v = G(x − z1[0]); s[0] = v + z1[0]; z1[0] = s[0] + v
                     │  Stage 2: v = G(x − z1[1]); s[1] = v + z1[1]; z1[1] = s[1] + v
                     │  Stage 3: v = G(x − z1[2]); s[2] = v + z1[2]; z1[2] = s[2] + v
                     │  Stage 4: v = G(x − z1[3]); s[3] = v + z1[3]; z1[3] = s[3] + v
                     │
                     └──→ feedback = s[3]; output = s[3]
```

### Characteristics

- **Huovilainen model**: OTA-based topology with proper frequency warping
- **Drive control**: Input gain 1.0–5.0 applied before tanh saturation (unique to Moog module)
- **Resonance range**: 0–1 maps to 0–4.0× internal feedback coefficient
- **Self-oscillation**: Occurs near resonance=1.0 with feedback gain ~4.0
- **Slope**: −24 dB/octave (4-pole lowpass)
- **Saturation**: `tanh()` on the combined input/feedback signal; drive multiplies input before saturation

### Key Difference from IR3109 (Jupiter-8/JX-3P)

The Moog Ladder has a **drive parameter** that increases input saturation independently of resonance. The IR3109 only has resonance-controlled feedback saturation. This gives the Moog a warmer, dirtier character at moderate settings.

---

## 8. Glide (Portamento)

| Property | Value |
|---|---|
| Glide time | 0 – 2 seconds (`moog_glide_time`) |
| Curve | Exponential approach: `currentFreq += (target − currentFreq) × glideSpeed` |
| Speed calc | `glideSpeed = 1.0 / (glideRate × sampleRate)` |
| Threshold | Glide stops when `abs(currentFreq − target) < 0.01 Hz` |
| Note behavior | New note sets `glideTarget`; if glide off or first note, frequency jumps immediately |

---

## 9. Voice Management

| Property | Value |
|---|---|
| Polyphony | 1 voice (monophonic) |
| Voice stealing | New MIDI note always steals the single voice |
| Retrigger | Envelopes retrigger without resetting amplitude (avoids clicks) |
| Portamento | Exponential glide between notes |
| Parameter smoothing | 20ms for cutoff/reso/drive, 5ms for VCO mix levels |
| Oversampling | 2× processor-wide (polyphase IIR half-band) |

**Oversampling rationale**: Ladder filter drive + high resonance produce nonlinear harmonics above Nyquist; 2× oversampling reduces aliasing.

---

## 10. Complete Parameter Map

### VCO-1 (3 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_vco1_waveform` | Choice | Sawtooth, Triangle, Pulse, Square | 0 (Sawtooth) | |
| `moog_vco1_range` | Float | 2–32 | 16 | Unity at 16 |
| `moog_vco1_pw` | Float | 0–1 | 0.5 | Pulse width (pulse waveform) |

### VCO-2 (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_vco2_waveform` | Choice | Sawtooth, Triangle, Pulse, Square | 0 (Sawtooth) | |
| `moog_vco2_detune` | Float | −100 to +100 cents | 0 | Fine pitch offset |
| `moog_vco2_range` | Float | 2–32 | 16 | |
| `moog_vco2_pw` | Float | 0–1 | 0.5 | |

### VCO-3 (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_vco3_waveform` | Choice | Sawtooth, Triangle, Pulse, Square | 0 (Sawtooth) | |
| `moog_vco3_detune` | Float | −100 to +100 cents | 0 | Fine pitch offset |
| `moog_vco3_range` | Float | 2–32 | 16 | |
| `moog_vco3_pw` | Float | 0–1 | 0.5 | |

### Mixer (3 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_mix_vco1` | Float | 0–1 | 0.8 | VCO-1 output level |
| `moog_mix_vco2` | Float | 0–1 | 0.6 | VCO-2 output level |
| `moog_mix_vco3` | Float | 0–1 | 0.4 | VCO-3 output level |

### VCF Ladder (6 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_vcf_cutoff` | Float (log) | 20–20000 Hz | 400 | Skew 0.3 |
| `moog_vcf_resonance` | Float | 0.0–1.0 | 0.4 | Maps to 4.0× feedback internally |
| `moog_vcf_drive` | Float | 1.0–5.0 | 1.5 | Input saturation control |
| `moog_vcf_env_depth` | Float | −1.0 to +1.0 | 0.6 | Env1 depth (±10 kHz) |
| `moog_vcf_key_track` | Choice | Off, 33%, 67%, 100% | 0 (Off) | Keyboard tracking |
| `moog_vcf_lfo_amount` | Float | 0.0–1.0 | 0.0 | Dedicated VCF LFO (±2 kHz) |

### Envelope 1 — Filter (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_env1_attack` | Float | 0.001–10 s | 0.005 | Skew 0.3 |
| `moog_env1_decay` | Float | 0.001–10 s | 0.5 | |
| `moog_env1_sustain` | Float | 0.0–1.0 | 0.3 | |
| `moog_env1_release` | Float | 0.001–10 s | 0.3 | |

### Envelope 2 — Amplitude (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_env2_attack` | Float | 0.001–10 s | 0.005 | |
| `moog_env2_decay` | Float | 0.001–10 s | 0.4 | |
| `moog_env2_sustain` | Float | 0.0–1.0 | 0.5 | |
| `moog_env2_release` | Float | 0.001–10 s | 0.2 | |

### LFO (5 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_lfo_rate` | Float | 0.01–30 Hz | 5.0 | Log skew 0.3 |
| `moog_lfo_depth` | Float | 0.0–1.0 | 0.0 | General LFO depth (pitch + filter) |
| `moog_lfo_waveform` | Choice | Sine, Triangle, Sawtooth, Square, S&H | 0 (Sine) | |
| `moog_lfo_pitch_amount` | Float | 0.0–1.0 | 0.0 | Dedicated pitch LFO depth |
| `moog_glide_time` | Float | 0–2 s | 0.05 | Portamento time |

### Noise (1 param)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `moog_noise_level` | Float | 0–1 | 0.0 | White noise mix level |

**Total: 36 parameters**

---

## 11. Known Issues & Construction Notes — Resolution Log

### Issue #1: Key Tracking Mode 3 (100%) Not Implemented — ✅ RESOLVED

~~The `vcfKeyTrackMode` switch handles modes 0 (Off), 1 (33%), and 2 (67%), but Choice index 3 (100%) has no handler.~~ **Fixed:** Added `else if (vcfKeyTrackMode == 3)` handler with `150.0f` Hz/semitone scaling, providing true 100% keyboard tracking.

### Issue #2: Dual LFO Filter Paths Are Confusingly Redundant — ℹ️ INTENTIONAL

Two separate parameters modulate the filter cutoff via the same LFO signal:
- `moog_lfo_depth` × LFO (general depth, also affects pitch)
- `moog_vcf_lfo_amount` × LFO (dedicated VCF LFO)

**Assessment:** This is intentional design — it enables separate pitch vs filter LFO depth control. `moog_lfo_depth` is a global LFO depth affecting both pitch and filter, while `moog_vcf_lfo_amount` is a dedicated filter-only modulation amount. No change needed.

### Issue #3: LFO Filter Modulation Uses Linear Hz, Not Musical Scaling — ✅ RESOLVED

~~The filter LFO modulation adds a linear Hz offset (`lfo × 3000` or `lfo × 2000`).~~ **Fixed:** Both LFO filter paths now use multiplicative (log-frequency) scaling: `cutoff × 2^(amount × lfo × 2)`, providing ±2 octaves of musically consistent modulation. The two paths combine multiplicatively for independent control.

### Issue #4: No Triangle Anti-Aliasing Consideration — ℹ️ NON-ISSUE

Triangle waves have no discontinuities and the 2× oversampling adequately mitigates any mild corner-point aliasing at very high frequencies. No change needed.

### Issue #5: Noise Generator Has No Color Control — ✅ RESOLVED

~~The Moog 15 only offers white noise.~~ **Fixed:** Added `moog_noise_color` Choice parameter (White/Pink). Pink noise uses the Paul Kellet 7-state filter algorithm, matching the PPG Wave's pink noise implementation. Output scaled by 0.11 for unity gain with white noise.

### Issue #6: Glide Speed Calculation May Overshoot — ✅ RESOLVED

~~The glide formula can overshoot if `glideRate × sr < 1.0`.~~ **Fixed:** Added `std::min(1.0f, ...)` clamp to the glide speed calculation, preventing any overshoot regardless of glide rate or sample rate.

---

## 12. Comparison: Moog 15 vs PPG Wave Construction Quality

| Aspect | Moog 15 | PPG Wave | Assessment |
|---|---|---|---|
| **Anti-aliasing** | PolyBLEP on 3 of 4 waveforms + 2× OS | Mipmap wavetables + polyBLEP sub + 2× filter OS | Comparable |
| **Parameter smoothing** | 20ms cutoff/reso/drive, 5ms mix | Wave position slew limiter; filter coeff gated | Comparable |
| **Nonlinear modelling** | tanh on ladder input with drive + soft-clip | tanh per filter stage + output tanh | Moog: drive adds expression |
| **Voice management** | Monophonic with envelope retrigger | 8-voice poly with anti-click fade | Different paradigms |
| **Modulation** | 2 envs, LFO (dual filter paths), glide | 3 envs, LFO+delay+sync, wave pos mod | PPG: richer |
| **Filter character** | Ladder + drive (warm, saturated) | SSM 2044 4-mode (versatile) | Different strengths |
| **Code bugs** | None (all fixed) | None (all fixed) | All clean |

---

## 13. Source File Reference

| File | Lines | Purpose |
|---|---|---|
| `Source/Modules/Moog15/Moog15Processor.h` | ~60 | Module processor, 1-voice synth, 2× oversampler |
| `Source/Modules/Moog15/Moog15Processor.cpp` | ~117 | processBlock with oversampling, parameter dispatch |
| `Source/Modules/Moog15/Moog15Voice.h` | ~88 | Voice: 3 VCOs, ladder filter, 2 envs, LFO, smoothers |
| `Source/Modules/Moog15/Moog15Voice.cpp` | ~340 | renderNextBlock (140-line sample loop), glide, noise |
| `Source/Modules/Moog15/Moog15VCO.h` | ~34 | Phase accumulator, 4-waveform VCO |
| `Source/Modules/Moog15/Moog15VCO.cpp` | ~102 | getNextSample with PolyBLEP |
| `Source/Modules/Moog15/Moog15Params.h` | ~10 | Parameter layout declaration |
| `Source/Modules/Moog15/Moog15Params.cpp` | ~180 | createParameterLayout (36 params) |
| `Source/Common/DSP/LadderFilter.h` | ~40 | Ladder filter class declaration |
| `Source/Common/DSP/LadderFilter.cpp` | ~78 | processSample, 4-pole cascade with drive |
| `Source/Common/DSP/ADSREnvelope.h/.cpp` | ~93 | Shared ADSR state machine |
| `Source/Common/DSP/LFOGenerator.h/.cpp` | ~93 | Shared multi-waveform LFO |
