# Jupiter-8 Module — Signal Topology & Construction Review

> **Purpose**: Complete architectural reference for the Roland Jupiter-8 emulation module in the AxelF synthesizer workstation. Covers signal flow, DSP algorithms, parameter map, modulation routing, and known issues.

---

## 1. Architecture Summary

| Property | Value |
|---|---|
| **Polyphony** | 8 voices (JUCE LRU voice stealing) |
| **Oscillators** | 2 × DCO (Sawtooth/Pulse/Sub-Osc), 1 × sub osc, 1 × white noise |
| **Filter** | IR3109 model — 4-pole OTA cascade with tanh soft-clip feedback |
| **Filter modes** | LP24 only |
| **Envelopes** | 2 × ADSR (Env1=Filter, Env2=Amplitude) |
| **LFO** | 1 × 5-shape with configurable delay fade-in |
| **Anti-aliasing** | PolyBLEP on saw/pulse waveforms; 2× processor-level oversampling |
| **Saturation** | tanh on filter feedback + cubic soft-clip on output |
| **Arpeggiator** | 4 modes (Up/Down/UpDown/Random), 1–3 octave range |
| **Parameter count** | 32 APVTS parameters (prefix: `jup8_`) |
| **Source files** | 10 files in `Source/Modules/Jupiter8/` |

---

## 2. Voice Signal Flow

```
 ┌──────────────────────────── VOICE (×8) ────────────────────────────┐
 │                                                                     │
 │  MIDI Note On ──→ frequency ──→ Portamento Glide                   │
 │  Pitch Bend  ──→ ±2 semitones ─┐                                   │
 │                                 │                                   │
 │  ┌──────────────────────────────▼──────────────────────────────┐    │
 │  │              PITCH CALCULATION                              │    │
 │  │  pitchMod = 2^(bend/12) × (1 + lfoDepth × LFO × 0.02)    │    │
 │  │  crossModMul = 1 + crossMod × prevDco2Out                  │    │
 │  │  dco1Freq = portaFreq × pitchMod × crossModMul             │    │
 │  │  dco2Freq = portaFreq × pitchMod × 2^(detune/1200)         │    │
 │  └──────────────────────────────┬──────────────────────────────┘    │
 │                                 │                                   │
 │     ┌───────────────────────────┼───────────────────┐               │
 │     │                           │                   │               │
 │     ▼                           ▼                   ▼               │
 │ ┌────────────┐          ┌────────────┐      ┌──────────────┐       │
 │ │   DCO-1    │          │   DCO-2    │      │  SUB OSC     │       │
 │ │ Saw/Pulse/ │          │ Saw/Pulse/ │      │  Square wave │       │
 │ │ Sub-Osc    │          │ Noise(alt) │      │  −1 octave   │       │
 │ │ PolyBLEP   │          │ PolyBLEP   │      │  from DCO-1  │       │
 │ │ ×range     │          │ ×range     │      └───────┬──────┘       │
 │ └───┬────────┘          └───┬────────┘              │               │
 │     │  ▲ hard sync          │                       │               │
 │     │  │ (dco1 phase wrap   │                       │               │
 │     │  │  resets dco2)      │                       │               │
 │     │  └────────────────────┘                       │               │
 │     │                                               │               │
 │     │           ┌───────────────────────┐           │               │
 │     │           │   WHITE NOISE (LCG)   │           │               │
 │     │           │   xorshift PRNG       │           │               │
 │     │           └───────────┬───────────┘           │               │
 │     │                       │                       │               │
 │     ▼                       ▼                       ▼               │
 │ ┌──────────────────────────────────────────────────────────────┐    │
 │ │                    MIXER BUS (Σ)                             │    │
 │ │  out = dco1 × mix1 + dco2 × mix2 + sub × subLevel          │    │
 │ │        + noise × noiseLevel                                  │    │
 │ │  (mix levels smoothed with 5ms ramp)                        │    │
 │ └─────────────────────────┬────────────────────────────────────┘    │
 │                           │                                         │
 │                           ▼                                         │
 │ ┌──────────────────────────────────────────────────────────────┐    │
 │ │               IR3109 FILTER (4-pole LPF)                    │    │
 │ │                                                              │    │
 │ │  Cutoff modulation:                                          │    │
 │ │    modCutoff = baseCutoff                                    │    │
 │ │              + vcfEnvDepth × ENV1 × 10000 Hz                 │    │
 │ │              + vcfLfoAmount × LFO × 3000 Hz                  │    │
 │ │              + keyTrackOffset                                │    │
 │ │    modCutoff = clamp(modCutoff, 20 Hz, 20 kHz)              │    │
 │ │                                                              │    │
 │ │  Topology: tanh(in − k×3.8×feedback) → 4× one-pole cascade │    │
 │ │  k = resonance (0–1), coefficient g = tan(π×fc/sr)          │    │
 │ └─────────────────────────┬────────────────────────────────────┘    │
 │                           │                                         │
 │                           ▼                                         │
 │ ┌──────────────────────────────────────────────────────────────┐    │
 │ │                    VCA / OUTPUT                               │    │
 │ │                                                              │    │
 │ │  output = filtered × ENV2 × velocity                        │    │
 │ │  output = softClip(output)      ← cubic soft-knee           │    │
 │ │  → stereo buffer (L=R, mono per voice)                       │    │
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
| **ENV 1** (Filter) | VCF cutoff | `jup8_vcf_env_depth` (−1.0 to +1.0) | `cutoff + envDepth × env1 × 10000 Hz` |
| **ENV 2** (Amplitude) | VCA level | hardwired | `output × env2` |
| **LFO** | Pitch (DCO1+2) | `jup8_lfo_depth` (0–1.0) | `freq × (1 + depth × lfo × 0.02)` — ±2% |
| **LFO** | VCF cutoff | `jup8_vcf_lfo_amount` (0–1.0) | `cutoff + amount × lfo × 3000 Hz` |
| **Mod Wheel** | LFO depth boost | CC#1 (0–1.0) | `totalDepth = depth + modWheel × (1 − depth)` |
| **Velocity** | VCA gain | hardwired | `output × velocity` |
| **Keyboard** | VCF cutoff | `jup8_vcf_key_track` (Off/50%/100%) | Off=0; 50%=(note−60)×50 Hz; 100%=(note−60)×100 Hz |
| **Pitch Bend** | Pitch (DCO1+2) | ±2 semitones fixed | `freq × 2^(bend/12)` |
| **Cross-Mod** | DCO-1 frequency | `jup8_dco_cross_mod` (0–1.0) | `dco1Freq × (1 + crossMod × prevDco2Out)` — FM |
| **Hard Sync** | DCO-2 phase | `jup8_dco_sync` (Off/On) | DCO-2 phase resets when DCO-1 wraps |
| **Portamento** | Pitch glide | `jup8_portamento` (0–5 sec) | `portaFreq += (target − portaFreq) × portaCoeff` |

---

## 4. Envelope Specifications

Both envelopes share the `ADSREnvelope` implementation. Range: 1 ms – 10 s for A/D/R.

| Envelope | UI Name | Primary Target | Defaults (A/D/S/R) |
|---|---|---|---|
| Env 1 | Filter Env | VCF cutoff (±10 kHz) | 10ms / 300ms / 0.0 / 400ms |
| Env 2 | Amp Env | VCA level; voice active gate | 10ms / 250ms / 0.7 / 350ms |

**State machine**:
```
Attack:   output += attackRate;               if (output ≥ 1.0) → Decay
Decay:    output -= decayRate × (output − sustain + ε)   → exponential
Sustain:  output = sustainLevel                           → hold until noteOff
Release:  output -= releaseRate × (output + ε)            → exponential to 0
```

**Voice lifecycle**: A voice is active while Env 2 `isActive()`. When Env 2 finishes release, `clearCurrentNote()` frees the voice.

---

## 5. LFO Detail

| Property | Spec |
|---|---|
| Shapes | Sine, Triangle, Sawtooth, Square, Sample & Hold |
| Rate | 0.1 – 30 Hz (log skew 0.3) |
| Delay | 0 – 5 seconds (linear fade-in from note-on) |
| Targets | Pitch (DCO1+2), Filter cutoff (separate control) |
| Amplitude mod | Not available (no tremolo on Jupiter-8) |

**Delay implementation**: `lfoDelayRamp = min(1.0, lfoDelaySamples / lfoDelayTarget)` — linear ramp from 0→1 over the delay period. `effectiveLfo = lfoVal × lfoDelayRamp`.

---

## 6. Oscillator Detail (Jupiter8DCO)

### Waveforms & Anti-Aliasing

| Waveform | Formula | Anti-Alias |
|---|---|---|
| **Sawtooth** (0) | `2 × phase − 1` | PolyBLEP residual subtracted |
| **Pulse** (1) | `phase < pw ? 1 : −1` | PolyBLEP on rising + falling edges |
| **Sub-Osc** (2) | `fmod(phase×0.5, 1) < 0.5 ? 1 : −1` | None (half-frequency square) |

### Range Mapping

```
phaseIncrement = frequency × (8.0 / range) / sampleRate
```
Range parameter: 2.0–16.0 (feet). Range=16'→0.5× frequency, range=8'→1×, range=4'→2×, range=2'→4×.

### Hard Sync

DCO-1's `phaseWrapped` flag is set when `phase ≥ 1.0`. Voice checks: if sync enabled and DCO-1 wrapped → `dco2.resetPhase()`. This produces the classic sync-swept timbral sweep.

### DCO-2 Noise Mode

When `dco2Waveform == 2` (Noise), DCO-2 output is replaced by xorshift LCG white noise instead of the sub-osc waveform:
```cpp
noiseState ^= noiseState << 13;
noiseState ^= noiseState >> 17;
noiseState ^= noiseState << 5;
dco2Out = (float)(int32_t)noiseState / 2147483648.0f;
```

---

## 7. IR3109 Filter Implementation

### Topology

```
Input ──→ [− resonance × 3.8 × tanh(feedback)] ──→ saturated input
                                                        │
                     ┌──────────────────────────────────┘
                     │
                     │  g = tan(π × cutoff / sampleRate)
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
- **Bilinear transform**: `tan(π·f/sr)` for correct frequency warping
- **Soft-clip feedback**: `tanh()` on feedback path prevents hard oscillation
- **Resonance range**: 0–1 maps to 0–3.8× internal feedback gain
- **Slope**: −24 dB/octave (4-pole lowpass)
- **Self-oscillation**: near resonance=1.0 (feedback gain ~3.8)

---

## 8. Arpeggiator (Jupiter8Arpeggiator)

| Property | Value |
|---|---|
| Modes | Up, Down, UpDown, Random |
| Octave range | 1–3 octaves |
| Step rate | Fixed 1/16th note (0.25 beats) |
| Timing | Sample-accurate beat counting |

**Sequence building**: For each held note, expand across octave range → sorted array. `getNextNote()` indexes this array per mode. UpDown uses ping-pong indexing with period `(seqSize−1)×2`.

**MIDI routing**: Arpeggiator processes input MIDI messages before synth voice dispatch. Non-note events (CC, pitch bend) pass through unchanged.

---

## 9. Voice Management

| Property | Value |
|---|---|
| Polyphony | 8 voices |
| Voice stealing | JUCE LRU (oldest active note) |
| Portamento | Exponential glide: `portaFreq += (target − portaFreq) × portaCoeff` |
| Voice modes | Poly, Unison, Solo Last, Solo Low, Solo High (parameter exists; **not implemented**) |
| Oversampling | 2× processor-wide, polyphase IIR half-band |

---

## 10. Complete Parameter Map

### DCO-1 (3 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_dco1_waveform` | Choice | Sawtooth, Pulse, Sub-Osc | 0 (Sawtooth) | DCO-1 waveform |
| `jup8_dco1_range` | Float | 2.0–16.0 (feet) | 8.0 | Octave range |
| `jup8_pulse_width` | Float | 0.0–100.0% | 50.0 | PWM duty cycle (both DCOs) |

### DCO-2 (3 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_dco2_waveform` | Choice | Sawtooth, Pulse, Noise | 0 (Sawtooth) | Index 2 = noise substitution |
| `jup8_dco2_range` | Float | 2.0–16.0 (feet) | 8.0 | Octave range |
| `jup8_dco2_detune` | Float | −50.0 to +50.0 cents | 6.0 | Fine pitch offset |

### Mixer (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_mix_dco1` | Float | 0.0–1.0 | 0.7 | DCO-1 output level |
| `jup8_mix_dco2` | Float | 0.0–1.0 | 0.5 | DCO-2 output level |
| `jup8_sub_level` | Float | 0.0–1.0 | 0.0 | Sub-oscillator level (−1 octave) |
| `jup8_noise_level` | Float | 0.0–1.0 | 0.0 | White noise mix level |

### VCF (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_vcf_cutoff` | Float (log) | 20–20000 Hz | 13000 | Skew 0.3 |
| `jup8_vcf_resonance` | Float | 0.0–1.0 | 0.15 | Maps to 3.8× internal feedback |
| `jup8_vcf_env_depth` | Float | −1.0 to +1.0 | 0.3 | Env1 depth (±10 kHz) |
| `jup8_vcf_hpf` | Choice | Off, 1, 2, 3 | 0 | **Stub — not implemented in voice** |

### Envelope 1 — Filter (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_env1_attack` | Float | 0.001–10.0 s | 0.01 | Skew 0.3 |
| `jup8_env1_decay` | Float | 0.001–10.0 s | 0.3 | |
| `jup8_env1_sustain` | Float | 0.0–1.0 | 0.0 | |
| `jup8_env1_release` | Float | 0.001–10.0 s | 0.4 | |

### Envelope 2 — Amplitude (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_env2_attack` | Float | 0.001–10.0 s | 0.01 | |
| `jup8_env2_decay` | Float | 0.001–10.0 s | 0.25 | |
| `jup8_env2_sustain` | Float | 0.0–1.0 | 0.7 | |
| `jup8_env2_release` | Float | 0.001–10.0 s | 0.35 | |

### LFO (4 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_lfo_rate` | Float | 0.1–30.0 Hz | 5.5 | Skew 0.3 (log) |
| `jup8_lfo_depth` | Float | 0.0–1.0 | 0.0 | Pitch mod ±2% |
| `jup8_lfo_waveform` | Choice | Sine, Triangle, Sawtooth, Square, S&H | 0 (Sine) | |
| `jup8_lfo_delay` | Float | 0.0–5.0 s | 0.0 | Delay before LFO starts |

### Advanced Modulation (3 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_vcf_lfo_amount` | Float | 0.0–1.0 | 0.0 | Separate LFO→VCF (±3 kHz) |
| `jup8_vcf_key_track` | Choice | Off, 50%, 100% | 0 (Off) | Keyboard tracking on filter |
| `jup8_dco_cross_mod` | Float | 0.0–1.0 | 0.0 | DCO-2→DCO-1 FM depth |

### Performance (3 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_portamento` | Float | 0.0–5.0 s | 0.0 | Glide time |
| `jup8_dco_sync` | Choice | Off, On | 0 (Off) | Hard sync DCO-2→DCO-1 |
| `jup8_voice_mode` | Choice | Poly, Unison, Solo Last, Solo Low, Solo High | 0 (Poly) | **Stub — not implemented** |

### Arpeggiator (3 params)
| Parameter ID | Type | Range | Default | Notes |
|---|---|---|---|---|
| `jup8_arp_on` | Choice | Off, On | 0 (Off) | Arpeggiator enable |
| `jup8_arp_mode` | Choice | Up, Down, Up/Down, Random | 0 (Up) | Direction |
| `jup8_arp_range` | Choice | 1 Oct, 2 Oct, 3 Oct | 0 (1 Oct) | 0-indexed; add 1 in code |

**Total: 32 parameters**

---

## 11. Known Issues & Construction Notes — Resolution Log

### Issue #1: HPF Parameter Not Wired — ✅ RESOLVED

Parameter `jup8_vcf_hpf` (Choice: Off/1/2/3) ~~exists in the APVTS but is **not connected** in the voice rendering loop~~. **Fixed:** Added 1-pole high-pass filter before the VCF with fixed corner frequencies: Off, 250 Hz, 500 Hz, 1 kHz (matching real Jupiter-8 HPF modes). HPF state stored per-voice with `hpfPrevIn`/`hpfPrevOut` members.

### Issue #2: Voice Mode Not Implemented — ✅ RESOLVED

Parameter `jup8_voice_mode` (Poly/Unison/Solo Last/Solo Low/Solo High) ~~is declared but ignored~~. **Fixed:** Implemented all 5 voice modes in Jupiter8Processor:
- **Poly**: Normal 8-voice polyphonic allocation
- **Unison**: All 8 voices triggered simultaneously with ±25 cents detune spread across voices
- **Solo Last**: Monophonic, last-note priority with note stack
- **Solo Low**: Monophonic, lowest-note priority
- **Solo High**: Monophonic, highest-note priority

### Issue #3: Sub-Oscillator Has No PolyBLEP — ✅ RESOLVED

~~The sub-oscillator uses a naïve half-frequency square wave without anti-aliasing.~~ **Fixed:** Added full PolyBLEP anti-aliasing to the sub-oscillator square wave, applied at both transition points (0 and 0.5). Uses the sub-oscillator's own phase increment for correct BLEP width.

### Issue #4: Sub-Oscillator Uses `thread_local` Static Phase — ✅ RESOLVED

~~The sub-oscillator phase accumulator is declared `static thread_local float subPhase`.~~ **Fixed:** Moved `subPhase` to a per-voice member variable in `Jupiter8Voice`. Each voice now maintains its own independent sub-oscillator phase accumulator.

### Issue #5: LFO Filter Modulation Scale Mismatch — ✅ RESOLVED

~~The VCF LFO uses `vcfLfoAmount × effectiveLfo × 3000.0f` which is a linear Hz offset.~~ **Fixed:** Replaced with multiplicative (log-frequency) scaling: `cutoff × 2^(amount × lfo × 2)`, providing ±2 octaves of musically consistent modulation regardless of base cutoff frequency.

### Issue #6: Noise Generator Shared Between DCO-2 and Mixer — ✅ RESOLVED

~~The same `noiseState` xorshift PRNG is used for both DCO-2 noise substitution and the noise mixer path.~~ **Fixed:** Added separate `noiseState2` PRNG (seeded differently at 67890) for the noise mixer path. DCO-2 noise and mixer noise are now fully independent.

### Issue #7: Arpeggiator Rate Fixed at 1/16th — ✅ RESOLVED

~~The arpeggiator step rate is hardcoded at 0.25 beats.~~ **Fixed:** Added `jup8_arp_rate` Choice parameter with 4 options: 1/32 (0.125 beats), 1/16 (0.25), 1/8 (0.5), 1/4 (1.0). Default is 1/16 for backward compatibility.

---

## 12. Comparison: Jupiter-8 vs PPG Wave Construction Quality

| Aspect | Jupiter-8 | PPG Wave | Assessment |
|---|---|---|---|
| **Anti-aliasing** | PolyBLEP on DCO + 2× oversampling | Mipmap wavetables + polyBLEP sub + 2× filter OS | PPG: slightly better |
| **Parameter smoothing** | 20ms cutoff/reso, 5ms mixer | Wave position slew limiter; filter coeff gated | Comparable |
| **Nonlinear modelling** | tanh feedback + cubic soft-clip | tanh per-stage + output tanh | Comparable |
| **Voice stealing** | JUCE LRU | 32-sample anti-click fade-in | PPG: has anti-click |
| **Modulation** | 2 envs, LFO+delay, cross-mod, sync, arp | 3 envs, LFO+delay+sync, wave position mod | PPG: richer |
| **Filter modes** | LP24 only (HPF stub) | LP24/LP12/BP12/HP12 | PPG: more modes |
| **Code bugs** | thread_local sub-osc phase (Issue #4) | None (all fixed) | PPG: cleaner |
| **Features** | Arpeggiator, portamento | Unison stereo spread | Different strengths |

---

## 13. Source File Reference

| File | Lines | Purpose |
|---|---|---|
| `Source/Modules/Jupiter8/Jupiter8Processor.h` | ~82 | Module processor, 8-voice synth, 2× oversampler |
| `Source/Modules/Jupiter8/Jupiter8Processor.cpp` | ~150 | processBlock, arpeggiator integration, parameter dispatch |
| `Source/Modules/Jupiter8/Jupiter8Voice.h` | ~120 | Voice class: 2 DCOs, filter, 2 envs, LFO, smoothers |
| `Source/Modules/Jupiter8/Jupiter8Voice.cpp` | ~280 | renderNextBlock (sample-by-sample), startNote, stopNote |
| `Source/Modules/Jupiter8/Jupiter8DCO.h` | ~40 | Oscillator: phase, waveform, range, pulse width |
| `Source/Modules/Jupiter8/Jupiter8DCO.cpp` | ~100 | getNextSample with PolyBLEP, updatePhaseIncrement |
| `Source/Modules/Jupiter8/Jupiter8Arpeggiator.h` | ~200 | 4-mode arp with octave expansion, beat timing |
| `Source/Modules/Jupiter8/Jupiter8Params.h` | ~12 | Parameter layout declaration |
| `Source/Modules/Jupiter8/Jupiter8Params.cpp` | ~150 | createParameterLayout (32 params) |
| `Source/Modules/Jupiter8/Jupiter8Editor.h` | ~100 | UI: DCO, VCF, ENV, LFO, ARP sections |
