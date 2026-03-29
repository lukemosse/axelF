# PPG Wave 2.2/2.3 Module — Signal Topology & Construction Review

> **Purpose**: Complete architectural reference for the PPG Wave emulation module in the AxelF synthesizer workstation. Covers signal flow, DSP algorithms, parameter map, modulation routing, and known issues.

---

## 1. Architecture Summary

| Property | Value |
|---|---|
| **Polyphony** | 8 voices (JUCE LRU voice stealing) |
| **Oscillators** | 2 × wavetable (A/B), 1 × sub, 1 × noise, 1 × ring mod |
| **Filter** | SSM 2044 model — 4-pole cascade with tanh nonlinearity |
| **Filter modes** | LP24, LP12, BP12, HP12 |
| **Envelopes** | 3 × ADSR (Env1=Filter, Env2=Amplitude, Env3=Wave Position) |
| **LFO** | 1 × 6-shape with delay fade-in |
| **Wavetables** | 16 factory banks × 64 waveforms × 256 samples, 8 mipmap levels |
| **Anti-aliasing** | Bandlimited mipmaps (octave-spaced, auto-selected by frequency) |
| **Saturation** | tanh soft clip on filter stages + output VCA |
| **Parameter count** | 63 APVTS parameters (prefix: `ppg_`) |
| **Source files** | 10 files in `Source/Modules/PPGWave/` |

---

## 2. Voice Signal Flow

```
 ┌──────────────────────────── VOICE (×8) ────────────────────────────┐
 │                                                                     │
 │  MIDI Note On ──→ frequency ──→ Portamento Glide                   │
 │  Pitch Bend  ──→ ±semitones ─┐                                     │
 │                               │                                     │
 │  ┌────────────────────────────▼──────────────────────────────┐      │
 │  │              PITCH CALCULATION                            │      │
 │  │  baseFreq = portaFreq × 2^((bend + LFO×pitchAmt×12)/12)│      │
 │  │  freqA = baseFreq × 2^((rangeA×12 + semiA + detuneA)/12)│      │
 │  │  freqB = baseFreq × 2^((rangeB×12 + semiB + detuneB)/12)│      │
 │  └────────────────────────────┬──────────────────────────────┘      │
 │                               │                                     │
 │         ┌─────────────────────┼─────────────────────┐               │
 │         │                     │                     │               │
 │         ▼                     ▼                     ▼               │
 │  ┌────────────┐      ┌────────────┐      ┌──────────────┐          │
 │  │ WAVETABLE  │      │ WAVETABLE  │      │  SUB OSC     │          │
 │  │ OSC A      │      │ OSC B      │      │  Sine/Square │          │
 │  │            │      │            │      │  −1/−2 oct   │          │
 │  │ table 0-15 │      │ table 0-15 │      │  from freqA  │          │
 │  │ pos 0-63   │      │ pos 0-63   │      └──────┬───────┘          │
 │  │ ×level     │      │ ×level     │             │                   │
 │  └───┬──┬─────┘      └───┬──┬─────┘             │                   │
 │      │  │                │  │                    │                   │
 │      │  └────────┬───────┘  │                    │                   │
 │      │           ▼          │       ┌────────────┤                   │
 │      │    ┌────────────┐    │       │            │                   │
 │      │    │  RING MOD  │    │       │   ┌────────▼───────┐          │
 │      │    │  A × B     │    │       │   │  NOISE GEN     │          │
 │      │    └─────┬──────┘    │       │   │  White/Pink    │          │
 │      │          │           │       │   │  1-pole LP     │          │
 │      │          │           │       │   └────────┬───────┘          │
 │      │          │           │       │            │                   │
 │      ▼          ▼           ▼       ▼            ▼                   │
 │  ┌──────────────────────────────────────────────────────────┐       │
 │  │                    MIXER BUS (Σ)                         │       │
 │  │  out = A×mixA + B×mixB + sub×mixSub                     │       │
 │  │        + noise×mixNoise + ring×mixRing                   │       │
 │  │  (each mix level: 0–127 → normalized 0–1)               │       │
 │  └────────────────────────────┬─────────────────────────────┘       │
 │                               │                                     │
 │                               ▼                                     │
 │  ┌──────────────────────────────────────────────────────────┐       │
 │  │               SSM 2044 FILTER (4-pole)                   │       │
 │  │                                                          │       │
 │  │  Cutoff modulation:                                      │       │
 │  │    fc = baseCutoff                                       │       │
 │  │       × 2^(envAmt × ENV1 × 4.0)     ← ±4 octaves       │       │
 │  │       × 2^(lfoAmt × LFO  × 2.0)     ← ±2 octaves       │       │
 │  │       × 2^(keytrack × (note−60)/12)  ← keyboard track   │       │
 │  │    fc = clamp(fc, 20 Hz, 20 kHz)                        │       │
 │  │                                                          │       │
 │  │  Topology: tanh(in − k×s4) → 4× one-pole cascade       │       │
 │  │  Mode routing:                                           │       │
 │  │    LP24 → y4     LP12 → y2                               │       │
 │  │    BP12 → y2−y4  HP12 → in−y2−k×y4                      │       │
 │  └────────────────────────────┬─────────────────────────────┘       │
 │                               │                                     │
 │                               ▼                                     │
 │  ┌──────────────────────────────────────────────────────────┐       │
 │  │                    VCA / OUTPUT                           │       │
 │  │                                                          │       │
 │  │  ampMod = 1 − lfoAmpAmt × lfoFade × (1−lfoVal) × 0.5   │       │
 │  │  velGain = 1 − vcaVelSens × (1 − velocity)              │       │
 │  │  output = filtered × ENV2 × vcaLevel × velGain × ampMod │       │
 │  │  output = tanh(output)           ← soft clip             │       │
 │  │  output × antiClickGain          ← 32-sample fade-in    │       │
 │  └────────────────────────────┬─────────────────────────────┘       │
 │                               │                                     │
 │                               ▼                                     │
 │                     Stereo buffer (L=R, mono)                       │
 └─────────────────────────────────────────────────────────────────────┘
                               │
                               ▼
                     MasterMixer → Host DAW
```

---

## 3. Modulation Routing Matrix

| Source | Target(s) | Depth Control | Formula |
|---|---|---|---|
| **ENV 1** (Filter) | VCF cutoff | `ppg_vcf_env_amt` (−127 to +127) | `fc × 2^(envAmt × env1 × 4.0)` — ±4 octaves |
| **ENV 2** (Amplitude) | VCA level | hardwired | `output × env2` |
| **ENV 3** (Wave Pos) | Osc A wave position | `ppg_waveA_env_amt` (−63 to +63) | `basePos + envAmt × env3 × 63` |
| **ENV 3** (Wave Pos) | Osc B wave position | `ppg_waveB_env_amt` (−63 to +63) | `basePos + envAmt × env3 × 63` |
| **LFO** | Pitch (A+B) | `ppg_lfo_pitch_amt` (0–127) | `2^((lfo × pitchAmt × 12) / 12)` — in semitones |
| **LFO** | VCF cutoff | `ppg_lfo_cutoff_amt` (0–127) | `fc × 2^(lfoAmt × lfo × 2.0)` — ±2 octaves |
| **LFO** | VCA amplitude | `ppg_lfo_amp_amt` (0–127) | `1 − ampAmt × fade × (1−lfo) × 0.5` — tremolo |
| **LFO** | Osc A wave position | `ppg_waveA_lfo_amt` (0–63) | `lfoAmt × lfo × 32` positions |
| **LFO** | Osc B wave position | `ppg_waveB_lfo_amt` (0–63) | `lfoAmt × lfo × 32` positions |
| **Velocity** | VCA gain | `ppg_vca_vel_sens` (0–7) | `1 − velSens × (1 − velocity)` |
| **Keyboard** | VCF cutoff | `ppg_vcf_keytrack` (0/50/100%) | `fc × 2^(track × (note−60)/12)` |
| **Pitch Bend** | Pitch (A+B) | `ppg_bend_range` (1–12 semi) | added to pitch before freq calc |

---

## 4. Envelope Specifications

All three envelopes share the same ADSR implementation with range 0.5 ms – 10,000 ms for A/D/R.

| Envelope | Code Index | UI Name | Primary Target | Secondary Target |
|---|---|---|---|---|
| Env 1 | `envelopes[0]` | Filter Env | VCF cutoff (±4 oct) | — |
| Env 2 | `envelopes[1]` | Amp Env | VCA level | Voice active gate (stops voice when inactive) |
| Env 3 | `envelopes[2]` | Wave Env | Osc A/B wave position | — |

**Voice lifecycle**: A voice is active while `envelopes[1]` (Amp Env) `isActive()`. When Env 2 finishes its release phase, `clearCurrentNote()` is called and the voice is freed.

---

## 5. LFO Detail

| Property | Spec |
|---|---|
| Shapes | Triangle, Sawtooth, Reverse Saw, Square, Sine, Sample & Hold |
| Rate | 0.01 – 50 Hz (skew 0.3 for log feel) |
| Delay | 0 – 5 seconds (linear fade-in from note-on) |
| Sync | Optional tempo sync to host BPM |
| Targets | Pitch, filter cutoff, amplitude, wave position A, wave position B |

**Reverse Saw handling**: `lfoNegate` flag inverts the LFO value for RevSaw shape (negated standard saw).

**Delay implementation**: `lfoFade = min(1.0, samplesSinceNoteOn / lfoDelaySamples)` — linear ramp from 0 to 1.

---

## 6. Wavetable Oscillator Detail

### Table Structure
- **16 factory banks**, each containing **64 waveforms** of **256 samples**
- **8 octave-spaced mipmap levels** per bank (generated via DFT → truncate harmonics → IDFT)
- Total memory: ~2 MB for all mipmaps

### Factory Wavetable Banks

| # | Name | Character | Generation Method |
|---|---|---|---|
| 0 | Upper Waves | Saw → Pulse sweep | Morph: sawtooth (pos 0) → pulse (pos 63) |
| 1 | Resonant | Formant peaks | Additive with resonant peak; peak freq morphs |
| 2 | Resonant 2 | Sharp filter | Gaussian harmonic drop; peak = 2.0 + w×0.6 |
| 3 | Mallet Waves | Marimba-like | Exponential harmonic decay, brightness ∝ position |
| 4 | Anharmonic | Metallic bells | Inharmonic partials: ratio = h × (1 + inharm × h²) |
| 5 | Electric Piano | Tine character | Fixed harmonic series: 1, 2(0.5), 3(0.25), 7(0.1) |
| 6 | Bass Waves | Sub-bass morph | Sine (dark) → Sine + square (dirty) |
| 7 | Organ Waves | Drawbar morph | 9-drawbar Hammond layout [0.5, 1.5, 1, 2, 3, 4, 5, 6, 8]× |
| 8 | Vocal Waves | Vowel sweep | Formant: F1 800→500 Hz, F2 2000→1200 Hz ("Ah"→"Oh") |
| 9 | Pad Waves | Warm evolving | Soft additive: sine + harmonics with phase shift |
| 10 | Digital Harsh | Bitcrusher | Saw quantized to 2–16 bit resolution per position |
| 11 | Sync Sweep | Overtone build | Hard-sync emulation: ratio = 1.0 + w × 0.12 |
| 12 | Classic Analog | Sine→Saw→Square | Three-segment morph across position range |
| 13 | Additive | Clean build-up | Pure harmonic series: Σ sin(h×phase)/h, h=1 to w×31 |
| 14 | Noise Shapes | Tonal→Noisy | Sine mixed with white noise; amount = position |
| 15 | User Slot | (customizable) | Defaults to Classic Analog; user-loadable |

### Oscillator `processSample()` Algorithm

```
1. Select mipmap level based on playback frequency
2. Get integer wave position (posLow) and fraction
3. Read sample from waveform[posLow] at current phase   → sampleLow
4. Read sample from waveform[posLow+1] at current phase → sampleHigh
5. Output = lerp(sampleLow, sampleHigh, fraction)       → crossfade between waves
6. Advance phase accumulator (0.0–1.0, wraps)
```

### Wave Position Smoothing (Anti-Glitch)

```cpp
// Slew-rate limiter: max ±2.0 position change per sample
smoothWavePosA += clamp(targetPosA - smoothWavePosA, -2.0, 2.0);
```

This prevents audible clicks when wave position jumps abruptly from large envelope or LFO modulation.

---

## 7. SSM 2044 Filter Implementation

### Topology

```
Input ──→ [−k × s4 feedback] ──→ tanh(saturate)
                                       │
                     ┌─────────────────┐│
                     │                 ▼▼
                     │  Stage 1: s1 + g × (tanh(in) − s1)  ──→ y1
                     │  Stage 2: s2 + g × (tanh(y1) − s2)  ──→ y2
                     │  Stage 3: s3 + g × (tanh(y2) − s3)  ──→ y3
                     │  Stage 4: s4 + g × (tanh(y3) − s4)  ──→ y4
                     └─────────────────┘

Coefficients:
  g = 1 − exp(−2π × cutoff / sampleRate)    ← one-pole coefficient
  k = resonance × 4.0                        ← feedback gain (0–4)

Mode output routing:
  LP24 = y4              (all 4 poles)
  LP12 = y2              (first 2 poles)
  BP12 = y2 − y4         (difference of 2-pole and 4-pole)
  HP12 = input − y2 − k×y4   (highpass approximation)
```

### Characteristics
- **Nonlinearity**: `tanh` in every stage emulates diode saturation (Moog-style warmth)
- **Self-oscillation**: at k=4.0 (resonance=127), the filter will ring at the cutoff frequency
- **No oversampling**: harmonic content is controlled by wavetable mipmaps, filter aliasing is minimal
- **Denormal flush**: subnormal values < 1e-15 are zeroed to prevent CPU spikes

---

## 8. Voice Management

| Property | Value |
|---|---|
| Polyphony | 8 voices |
| Voice stealing | JUCE LRU (oldest active note) |
| Anti-click | 32-sample fade-in on stolen voices |
| Portamento modes | Off, Legato, Always |
| Portamento glide | Exponential: `portaFreq += (target − portaFreq) × portaRate` |
| Voice modes | Poly, Unison, Dual (parameter exists; unison stereo spread planned) |

---

## 9. Complete Parameter Map

### Oscillator A (9 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_waveA_table` | 0–15 | 0 (Upper Waves) | Wavetable bank select |
| `ppg_waveA_pos` | 0–63 | 8 | Base wave position |
| `ppg_waveA_env_amt` | −63 to +63 | 30 | Env3 → wave position depth |
| `ppg_waveA_lfo_amt` | 0–63 | 0 | LFO → wave position depth |
| `ppg_waveA_range` | 64'/32'/16'/8'/4'/2' | 8' (idx 3) | Octave range |
| `ppg_waveA_semi` | −12 to +12 | 0 | Semitone offset |
| `ppg_waveA_detune` | −50 to +50 cents | 0 | Fine tune |
| `ppg_waveA_keytrack` | On/Off | On | Keyboard tracking |
| `ppg_waveA_level` | 0–127 | 100 | Pre-mixer amplitude |

### Oscillator B (9 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_waveB_table` | 0–15 | 4 (Anharmonic) | Wavetable bank select |
| `ppg_waveB_pos` | 0–63 | 12 | Base wave position |
| `ppg_waveB_env_amt` | −63 to +63 | 20 | Env3 → wave position depth |
| `ppg_waveB_lfo_amt` | 0–63 | 0 | LFO → wave position depth |
| `ppg_waveB_range` | 64'/32'/16'/8'/4'/2' | 8' (idx 3) | Octave range |
| `ppg_waveB_semi` | −12 to +12 | 0 | Semitone offset |
| `ppg_waveB_detune` | −50 to +50 cents | 7 | Fine tune |
| `ppg_waveB_keytrack` | On/Off | On | Keyboard tracking |
| `ppg_waveB_level` | 0–127 | 80 | Pre-mixer amplitude |

### Sub Oscillator (3 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_sub_wave` | Sine/Square | Sine (idx 0) | ⚠️ See Bug #1 below |
| `ppg_sub_octave` | −1/−2 | −1 (idx 0) | Octave below Osc A |
| `ppg_sub_level` | 0–127 | 0 | Pre-mixer amplitude |

### Noise (2 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_noise_level` | 0–127 | 0 | Pre-mixer amplitude |
| `ppg_noise_color` | White/Pink | White (idx 0) | 1-pole LP filter coefficient |

### Mixer (5 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_mix_waveA` | 0–127 | 100 | Osc A send level |
| `ppg_mix_waveB` | 0–127 | 80 | Osc B send level |
| `ppg_mix_sub` | 0–127 | 0 | Sub osc send level |
| `ppg_mix_noise` | 0–127 | 0 | Noise send level |
| `ppg_mix_ringmod` | 0–127 | 0 | Ring mod (A×B) send level |

### Filter / VCF (6 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_vcf_cutoff` | 20–20000 Hz | 8000 | Skew 0.3 (logarithmic) |
| `ppg_vcf_resonance` | 0–127 | 30 | Maps to k=0–4 internally |
| `ppg_vcf_env_amt` | −127 to +127 | 50 | Env1 depth (±4 octaves) |
| `ppg_vcf_lfo_amt` | 0–127 | 0 | LFO depth (±2 octaves) |
| `ppg_vcf_keytrack` | 0%/50%/100% | 50% (idx 1) | Keyboard → cutoff tracking |
| `ppg_vcf_type` | LP24/LP12/BP12/HP12 | LP24 (idx 0) | Filter mode |

### VCA (2 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_vca_level` | 0–127 | 100 | Output level |
| `ppg_vca_vel_sens` | 0–7 | 3 | Velocity sensitivity (3-bit) |

### Envelope 1 — Filter (4 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_env1_attack` | 0.5–10000 ms | 50 | Skew 0.3 |
| `ppg_env1_decay` | 0.5–10000 ms | 800 | |
| `ppg_env1_sustain` | 0–127 | 50 | |
| `ppg_env1_release` | 0.5–10000 ms | 500 | |

### Envelope 2 — Amplitude (4 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_env2_attack` | 0.5–10000 ms | 20 | |
| `ppg_env2_decay` | 0.5–10000 ms | 0.5 | |
| `ppg_env2_sustain` | 0–127 | 127 | |
| `ppg_env2_release` | 0.5–10000 ms | 600 | |

### Envelope 3 — Wave Position (4 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_env3_attack` | 0.5–10000 ms | 300 | |
| `ppg_env3_decay` | 0.5–10000 ms | 2000 | |
| `ppg_env3_sustain` | 0–127 | 38 | |
| `ppg_env3_release` | 0.5–10000 ms | 1000 | |

### LFO (7 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_lfo_wave` | Tri/Saw/RevSaw/Sq/Sin/S&H | Triangle (idx 0) | |
| `ppg_lfo_rate` | 0.01–50 Hz | 4 | Skew 0.3 |
| `ppg_lfo_delay` | 0–5 sec | 0.5 | Fade-in time |
| `ppg_lfo_pitch_amt` | 0–127 | 5 | Depth in semitones (×12) |
| `ppg_lfo_cutoff_amt` | 0–127 | 0 | ±2 octaves |
| `ppg_lfo_amp_amt` | 0–127 | 0 | Tremolo depth |
| `ppg_lfo_sync` | Off/On | Off | Host BPM sync |

### Performance (5 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `ppg_bend_range` | 1–12 semi | 2 | |
| `ppg_porta_time` | 0–5 sec | 0 | 0 = off |
| `ppg_porta_mode` | Off/Legato/Always | Off (idx 0) | |
| `ppg_voice_mode` | Poly/Unison/Dual | Poly (idx 0) | |
| `ppg_unison_detune` | 0–50 cents | 15 | Unison spread |

**Total: 63 parameters**

---

## 10. Known Issues & Construction Notes — Resolution Log

### Bug #1: Sub Oscillator Waveform Mapping Reversed — **FIXED**

The voice code treated index 0 as square, but the parameter declared index 0 = "Sine". Fixed: swapped the logic in `generateSubSample()` so index 0 → sine, index 1 → square, matching the APVTS labels.

### Note #2: Dual Osc Level vs Mixer Level — **Intentional (No Change)**

On review, the dual gain staging serves a purpose: `oscALevel` scales the oscillator output **before** the ring modulator (`ringMod = sampleA * sampleB`), which means per-osc level controls ring mod intensity. `mixA` controls the direct path independently. Removing either stage would change ring modulator behavior. Architecture is correct as-is.

### Note #3: Pink Noise Implementation — **FIXED**

Replaced the crude 1-pole LP filter with Paul Kellet's economy pink noise algorithm (6-stage IIR producing -3 dB/octave slope). White noise mode is now unfiltered pass-through (was already effectively pass-through at coefficient 1.0).

### Note #4: No Oversampling on Filter — **FIXED (2× per-voice)**

Added 2× oversampling to the SSM 2044 filter in each voice's render loop. For every input sample, a linearly interpolated mid-point sample is processed first, then the real sample — giving the filter a 2× effective sample rate. Filter coefficients are calculated at 2× the host sample rate via `setSampleRate(sr * 2.0)`.

### Note #5: Unison Stereo Spread — **FIXED**

Implemented fully:
- **UnisonSynth** subclass overrides `noteOn`/`noteOff` to trigger all 8 voices on a single note
- Per-voice stereo spread via equal-power pan law (voice 0 = hard left, voice 7 = hard right)
- Per-voice detune offset from `ppg_unison_detune` parameter (-cents to +cents, spread linearly)
- In Poly/Dual modes: all voices centered, no detune offset

### Note #6: Sub Oscillator Anti-Aliasing — **FIXED**

Added polyBLEP (Polynomial Band-Limited Step) anti-aliasing to the square wave sub oscillator. Sine wave needs no anti-aliasing (no discontinuities). The polyBLEP smooths the transitions at phase 0 and 0.5, eliminating aliasing harmonics above Nyquist.

### Note #7: Filter Coefficient Update Every Sample — **FIXED**

Added dirty-checking in `SSM2044Filter::setParameters()`: cutoff, resonance, and mode are compared against stored values. `updateCoefficients()` (containing the `exp()` call) is only invoked when a parameter has actually changed. During envelope modulation, coefficients still update every sample (correctly), but during sustain/static periods the `exp()` is skipped.

---

## 11. Comparison: PPG vs DX7 Construction Quality

| Aspect | PPG Wave | DX7 | Assessment |
|---|---|---|---|
| **Anti-aliasing** | Mipmap wavetables + polyBLEP sub osc + 2× filter oversampling | Per-operator phase accumulation, no oversampling | PPG: strong |
| **Parameter smoothing** | Wave position slew limiter; filter coeff gated | Operator levels smoothed | Comparable |
| **Nonlinear modelling** | tanh in filter (2× oversampled) + output | Feedback operator saturation | Comparable |
| **Voice stealing** | 32-sample anti-click fade-in | Same mechanism | Comparable |
| **Modulation depth** | 3 envs, LFO with delay, keytrack, unison spread | 32 algorithms, 6 operators, per-op envelopes | DX7: richer |
| **Noise generation** | Paul Kellet pink noise (-3 dB/oct) | N/A | Good |
| **Unison mode** | 8-voice stereo spread + detuning | N/A | Implemented |
| **Code bugs** | None known (sub osc swap fixed) | None known | Clean |

---

## 12. Source File Reference

| File | Lines | Purpose |
|---|---|---|
| `Source/Modules/PPGWave/PPGWaveProcessor.h` | ~80 | Module processor class declaration |
| `Source/Modules/PPGWave/PPGWaveProcessor.cpp` | ~140 | processBlock, updateVoiceParameters |
| `Source/Modules/PPGWave/PPGWaveVoice.h` | ~90 | Voice class with all member variables |
| `Source/Modules/PPGWave/PPGWaveVoice.cpp` | ~275 | renderNextBlock, startNote, stopNote, sub/noise gen |
| `Source/Modules/PPGWave/PPGWaveOsc.h` | ~60 | Wavetable oscillator interface |
| `Source/Modules/PPGWave/PPGWaveOsc.cpp` | ~100 | processSample, mipmap selection, phase accumulation |
| `Source/Modules/PPGWave/SSM2044Filter.h` | ~40 | Filter class declaration |
| `Source/Modules/PPGWave/SSM2044Filter.cpp` | ~80 | processSample, coefficient calculation |
| `Source/Modules/PPGWave/PPGWaveParams.h` | ~20 | Parameter layout declaration |
| `Source/Modules/PPGWave/PPGWaveParams.cpp` | ~250 | createParameterLayout (all 63 params) |
| `Source/Modules/PPGWave/WavetableData.h` | ~300 | 16 factory wavetable generators + mipmap gen |
| `Source/Modules/PPGWave/PPGWaveEditor.h` | ~150 | UI component |
