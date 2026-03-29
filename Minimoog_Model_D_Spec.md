# Minimoog Model D Module — Signal Topology & Construction Spec

> **Purpose**: Complete specification for a Minimoog Model D emulation to replace the Moog 15 module in the AxelF workstation. Designed for drop-in compatibility: same `moog_` parameter prefix, same slot in the module chain, same mono bass/lead role.

---

## 1. Architecture Summary

| Property | Value |
|---|---|
| **Polyphony** | Monophonic (single JUCE voice, voice steal = retrigger) |
| **Oscillators** | 3 × VCO (polyBLEP), each with 6 waveforms |
| **Noise** | 1 × White/Pink generator |
| **External Input** | 1 × feedback path (output → mixer, emulates the Model D's trick) |
| **Filter** | Moog transistor ladder — 4-pole (24 dB/oct) lowpass, self-oscillating |
| **Filter Modes** | LP only (authentic Model D has no mode switch) |
| **Contour Generators** | 2 × ADS + separate Release (Filter Contour + Loudness Contour) |
| **Oversampling** | 2× at processor level (same as existing Moog 15) |
| **MIDI timestamp scaling** | Yes — scale by oversampling factor before synth render |
| **Anti-click** | Built into contour generators + mandatory micro-glide |
| **Parameter count** | 37 APVTS parameters (prefix: `moog_`) |
| **Source files** | Drop-in replacement in `Source/Modules/Moog15/` |

---

## 2. Why Model D Instead of Moog 15

The Moog System 15 is a semi-modular with flexible patching — that flexibility created ambiguity in the voice restart logic that we couldn't fully resolve. The Model D has a **fixed signal path** with well-documented behavior at every stage:

1. **No voice-state ambiguity** — the Model D's VCOs run continuously; they never stop or reset. You "open the VCA" to hear them. This eliminates oscillator-phase-related pops entirely.
2. **Contour generators (not ADSRs)** — the Model D's Attack-Decay-Sustain contours with a separate release switch behave differently from standard ADSRs. Retrigger always restarts from the current level — no zeroing, no discontinuity.
3. **Filter feedback** — the canonical Minimoog sound includes feeding the headphone output back into the external input of the mixer for controlled overdrive. This is a key sonic feature missing from the Moog 15.
4. **Exhaustively documented** — the Model D circuit has been analyzed down to individual transistor pairs. Every nonlinearity is known.

---

## 3. Voice Signal Flow

```
 ┌─────────────────────── MONO VOICE ───────────────────────────────┐
 │                                                                   │
 │  MIDI Note → Glide circuit → currentFreq                         │
 │  Pitch Bend → ±bend semitones                                    │
 │                                                                   │
 │  ┌───────────────────────────────────────────────────────────┐    │
 │  │                  PITCH CALCULATION                        │    │
 │  │  modPitch = currentFreq                                   │    │
 │  │           × 2^(bendSemitones / 12)                        │    │
 │  │           × 2^(osc3LFO × modMix × pitchModDepth)         │    │
 │  │  (Osc 3 can double as LFO when "Osc 3 Ctrl" is on)       │    │
 │  │                                                           │    │
 │  │  vco1Freq = modPitch × rangeMultiplier[vco1Range]         │    │
 │  │  vco2Freq = modPitch × rangeMultiplier[vco2Range]         │    │
 │  │           × 2^(vco2Detune / 1200)                         │    │
 │  │  vco3Freq = modPitch × rangeMultiplier[vco3Range]         │    │
 │  │           × 2^(vco3Detune / 1200)                         │    │
 │  │  (if Osc3 Ctrl: vco3Freq is free-running, ignores kbd)    │    │
 │  └───────────────────────────────────────────────────────────┘    │
 │                                                                   │
 │  ┌──────────┐   ┌──────────┐   ┌──────────┐                      │
 │  │  VCO 1   │   │  VCO 2   │   │  VCO 3   │                      │
 │  │          │   │          │   │          │                      │
 │  │ Tri/Shark│   │ Tri/Shark│   │ Tri/Shark│                      │
 │  │ Saw/RevSw│   │ Saw/RevSw│   │ Saw/RevSw│                      │
 │  │ Square   │   │ Square   │   │ Square   │                      │
 │  │ Pulse    │   │ Pulse    │   │ Pulse    │                      │
 │  │ ×level   │   │ ×level   │   │ ×level   │                      │
 │  └────┬─────┘   └────┬─────┘   └────┬─────┘                      │
 │       │              │              │                             │
 │       ▼              ▼              ▼                             │
 │  ┌──────────────────────────────────────────────────────────┐     │
 │  │                     MIXER (Σ)                            │     │
 │  │                                                          │     │
 │  │  mix = vco1 × vol1 × sw1                                │     │
 │  │      + vco2 × vol2 × sw2                                │     │
 │  │      + vco3 × vol3 × sw3                                │     │
 │  │      + noise × noiseVol × noiseSw                        │     │
 │  │      + feedback × extVol × extSw                         │     │
 │  │                                                          │     │
 │  │  Each source has a volume knob (0–10) AND on/off switch  │     │
 │  │  (In our version: volume 0 = off, simplifying to 5 knobs)│     │
 │  └────────────────────────┬─────────────────────────────────┘     │
 │                           │                                       │
 │                           ▼                                       │
 │  ┌──────────────────────────────────────────────────────────┐     │
 │  │              MOOG TRANSISTOR LADDER FILTER               │     │
 │  │                                                          │     │
 │  │  Cutoff modulation:                                      │     │
 │  │    fc = baseCutoff                                       │     │
 │  │       × 2^(contourAmt × filterContour × 4.0)            │     │
 │  │       × 2^(modMix × osc3orNoise × filterModDepth)       │     │
 │  │       × 2^(keyTrack × (note − 60) / 12)                 │     │
 │  │    fc = clamp(fc, 20 Hz, 20 kHz)                        │     │
 │  │                                                          │     │
 │  │  Topology (Huovilainen improved Moog model):             │     │
 │  │    Thermal voltage scaling: Vt = 1.22                    │     │
 │  │    4× matched one-pole stages with tanh saturation       │     │
 │  │    Resonance compensation (slight gain boost at k > 3)   │     │
 │  │    Self-oscillation at k = 4.0                           │     │
 │  │                                                          │     │
 │  │  24 dB/oct lowpass only (authentic Model D)              │     │
 │  └────────────────────────┬─────────────────────────────────┘     │
 │                           │                                       │
 │                           ▼                                       │
 │  ┌──────────────────────────────────────────────────────────┐     │
 │  │                     VCA / OUTPUT                         │     │
 │  │                                                          │     │
 │  │  output = filtered × loudnessContour × masterVol         │     │
 │  │  output = tanh(output × 1.2)          ← tube-like clip  │     │
 │  │                                                          │     │
 │  │  feedback = output × feedbackGain     ← fed back to     │     │
 │  │                                         mixer ext input  │     │
 │  └────────────────────────┬─────────────────────────────────┘     │
 │                           │                                       │
 │                           ▼                                       │
 │                    Stereo buffer (L=R, mono)                      │
 └───────────────────────────────────────────────────────────────────┘
                           │
                           ▼
                    MasterMixer → Host DAW
```

---

## 4. Key Design Differences from Moog 15

| Aspect | Moog 15 (current) | Model D (new) |
|---|---|---|
| **VCOs** | Oscillators owned by voice, start/stop with voice | **Free-running** — phase accumulators never reset. VCA opens to hear them. |
| **Envelopes** | Standard ADSR, `noteOn()` changes state but not output | **ADS contour** — retrigger always restarts attack from current level (no state machine reset) |
| **Filter reset** | Conditional reset logic | **Never reset** — analog filter runs continuously, state persists across notes |
| **Anti-click** | 32-sample fade-in counter | **Built into contour** — attack ramp from zero handles fade-in naturally when starting from silence |
| **Glide** | Optional, separate logic | **Always-on micro-glide** (1ms minimum) prevents pitch discontinuity; user glide time adds on top |
| **Feedback** | None | **Output → mixer feedback** path for classic Minimoog growl |
| **Osc 3 as LFO** | Separate LFO generator | **Osc 3 doubles as LFO** when "Osc 3 Ctrl" engaged (disconnected from keyboard) |
| **Filter mode** | LP24 only | LP24 only (same, authentic) |
| **Modulation** | Separate LFO + mod routing params | **Mod wheel controls mix** of Osc3 + Noise → pitch and/or filter |

---

## 5. Oscillator Specification

### Waveforms (per VCO)

| Index | Name | Description |
|---|---|---|
| 0 | Triangle | Standard triangle, polyBLAMP anti-aliased |
| 1 | Shark Tooth | Triangle with asymmetric rise/fall (Model D signature) |
| 2 | Sawtooth | Falling saw, polyBLEP anti-aliased |
| 3 | Reverse Saw | Rising saw, polyBLEP anti-aliased |
| 4 | Square | 50% duty cycle, polyBLEP anti-aliased |
| 5 | Pulse | Variable width (0.05–0.95), polyBLEP anti-aliased |

### Range Settings

| Footage | Multiplier | Use |
|---|---|---|
| 32' | 0.5× | Sub bass |
| 16' | 1.0× | Normal |
| 8' | 2.0× | +1 octave |
| 4' | 4.0× | +2 octaves |
| 2' | 8.0× | +3 octaves |
| Lo (VCO 3 only) | Free LFO | 0.1–30 Hz, ignores keyboard CV |

### Free-Running Oscillators

**Critical design point**: Unlike the Moog 15, the Model D's oscillators are **always running**. Their phase accumulators are initialized once at construction and never reset. `startNote()` only updates `glideTarget` and triggers the contour generators — it does NOT touch oscillator state.

This eliminates an entire class of note-start transients because the oscillators are already producing a continuous waveform before the VCA opens.

---

## 6. Contour Generators (NOT ADSRs)

The Model D uses Attack-Decay-Sustain contour generators with a separate "decay switch" that doubles as release when the key is released. This is simpler and more robust than a full ADSR:

### Behavior

```
Note On:
  contourOutput ramps UP at attackRate from current level toward 1.0
  When contourOutput reaches 1.0 → switch to Decay phase
  Decay: contourOutput ramps DOWN at decayRate toward sustainLevel
  At sustainLevel → hold

Note Off:
  contourOutput ramps DOWN at decayRate toward 0.0
  (Note: uses decay rate as release rate — authentic Model D behavior)
  When contourOutput reaches 0.0 → Idle

Retrigger (new note while active):
  contourOutput ramps UP at attackRate from CURRENT LEVEL toward 1.0
  (No discontinuity — no zeroing — just starts climbing from wherever it is)
```

### Implementation

```cpp
float MoogContour::getNextSample()
{
    switch (state)
    {
        case Idle:
            return 0.0f;

        case Attack:
            output += attackRate;
            if (output >= 1.0f) { output = 1.0f; state = Decay; }
            break;

        case Decay:
            output += (sustainLevel - output) * decayRate;  // exponential
            if (std::abs(output - sustainLevel) < 0.0001f)
            { output = sustainLevel; state = Sustain; }
            break;

        case Sustain:
            output = sustainLevel;
            break;

        case Release:
            output *= (1.0f - decayRate);  // exponential decay toward 0
            if (output < 0.0001f) { output = 0.0f; state = Idle; }
            break;
    }
    return output;
}

void MoogContour::noteOn()
{
    // DO NOT reset output — retrigger from current level
    state = Attack;
}
```

### Key Difference from ADSREnvelope

The existing `ADSREnvelope` uses `output += attackRate` which is linear, and `noteOn()` only sets state. But `reset()` zeros output. The Model D contour **never needs a reset() call** — `noteOn()` always restarts attack from the current level. This is what makes it inherently click-free.

---

## 7. Filter — Huovilainen Moog Ladder Model

We use the Antti Huovilainen improved ladder model instead of the simplified version currently in `LadderFilter.cpp`. Key improvements:

- **Thermal voltage scaling** simulates transistor pair nonlinearity
- **Delay-free feedback** via the Huovilainen compensation term
- **Better self-oscillation** character at high resonance
- **Never reset** — the filter runs continuously, even between notes

### Topology

```
Input ──→ [input × drive] ──→ [−k × comp × stage[3]] ──→ tanh()
                                                            │
                              ┌─────────────────────────────┘
                              │
    Stage 1: s1 += G × (tanh(x/Vt) − tanh(s1/Vt))  ──→ y1
    Stage 2: s2 += G × (tanh(y1/Vt) − tanh(s2/Vt))  ──→ y2
    Stage 3: s3 += G × (tanh(y2/Vt) − tanh(s3/Vt))  ──→ y3
    Stage 4: s4 += G × (tanh(y3/Vt) − tanh(s4/Vt))  ──→ y4

    G  = 1 − exp(−2π × fc / sampleRate)
    Vt = 1.22  (thermal voltage scaling)
    k  = resonance × 4.0
    comp = 0.5  (delay-free feedback compensation)

Output: y4
```

### Key Design Point: No Reset

```cpp
// startNote() does NOT call filter.reset()
// The filter state (stage[0..3]) persists across all notes
// This is how real analog circuits work — the caps hold their charge
```

---

## 8. Feedback Path

The Model D's famous "fat" sound comes partly from routing the headphone output back into the External Input on the mixer. In our emulation:

```cpp
// At end of sample loop:
float output = filtered * loudnessContour * masterVol;
output = std::tanh(output * 1.2f);  // soft clip

// Store for next sample's feedback
feedbackSample = output;

// In mixer, next sample:
float extIn = feedbackSample * feedbackGain;  // 0.0 = off, up to 0.8
mix += extIn;
```

`feedbackGain` is controlled by the `moog_feedback` parameter (0.0–0.8). At 0.0 it's off (clean). At 0.5 it adds warmth and harmonics. Above 0.7 it gets into growling overdrive territory.

---

## 9. Modulation Routing (Mod Wheel)

The Model D's modulation system is simpler than the Moog 15's separate LFO parameters:

```
Mod Wheel (0–1)
    │
    ├── × filterModDepth ──→ multiply into VCF cutoff
    │
    └── × pitchModDepth  ──→ multiply into VCO pitch

Mod Source = mix of:
    Osc 3 output × osc3ModAmt
    + Noise × noiseModAmt
```

When "Osc 3 Ctrl" is engaged, VCO 3 runs at LFO rate (ignores keyboard CV) and its output becomes the primary modulation source. This is how the real Model D does vibrato and filter wobble.

---

## 10. Glide (Portamento)

```
Always-on micro-glide: minimum 1ms transition time
User glide knob: 0–10 sec (adds on top of micro-glide)
Glide mode: always active (authentic Model D — no legato-only option)

// In renderNextBlock, every sample:
float effectiveGlide = std::max(glideTime, 0.001f);
float rate = 1.0f / (effectiveGlide * sampleRate);
currentFreq += (glideTarget - currentFreq) * rate;
```

The micro-glide is imperceptible as pitch movement but eliminates the waveform discontinuity that occurs when oscillator frequency changes instantly under non-zero VCA amplitude.

---

## 11. Complete Parameter Map

### VCO-1 (4 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_vco1_waveform` | 0–5 | 2 (Saw) | Tri/Shark/Saw/RevSaw/Sq/Pulse |
| `moog_vco1_range` | 0–4 | 1 (16') | 32'/16'/8'/4'/2' |
| `moog_vco1_level` | 0.0–1.0 | 0.8 | Mixer volume (0 = off) |
| `moog_vco1_pw` | 0.05–0.95 | 0.5 | Pulse width (Pulse waveform only) |

### VCO-2 (5 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_vco2_waveform` | 0–5 | 2 (Saw) | Same 6 waveforms |
| `moog_vco2_range` | 0–4 | 1 (16') | 32'/16'/8'/4'/2' |
| `moog_vco2_detune` | −100 to +100 cents | 7.0 | Fine tune |
| `moog_vco2_level` | 0.0–1.0 | 0.6 | Mixer volume |
| `moog_vco2_pw` | 0.05–0.95 | 0.5 | Pulse width |

### VCO-3 (6 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_vco3_waveform` | 0–5 | 2 (Saw) | Same 6 waveforms |
| `moog_vco3_range` | 0–5 | 1 (16') | 32'/16'/8'/4'/2'/Lo |
| `moog_vco3_detune` | −100 to +100 cents | −5.0 | Fine tune |
| `moog_vco3_level` | 0.0–1.0 | 0.4 | Mixer volume |
| `moog_vco3_pw` | 0.05–0.95 | 0.5 | Pulse width |
| `moog_vco3_ctrl` | Off/On | Off (0) | On = LFO mode (ignores keyboard) |

### Noise & Feedback (3 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_noise_level` | 0.0–1.0 | 0.0 | Mixer volume |
| `moog_noise_color` | White/Pink | White (0) | Paul Kellet pink filter |
| `moog_feedback` | 0.0–0.8 | 0.0 | Output → mixer feedback gain |

### Filter / VCF (5 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_vcf_cutoff` | 20–20000 Hz | 2000 | Skew 0.3 (log) |
| `moog_vcf_resonance` | 0.0–1.0 | 0.2 | Maps to k=0–4 internally |
| `moog_vcf_env_depth` | −1.0 to +1.0 | 0.3 | Filter contour → cutoff (±4 oct) |
| `moog_vcf_key_track` | Off/33%/67%/100% | 33% (1) | Keyboard → cutoff |
| `moog_vcf_drive` | 1.0–5.0 | 1.5 | Input saturation |

### Filter Contour (3 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_env1_attack` | 0.001–10.0 s | 0.005 | Skew 0.3 |
| `moog_env1_decay` | 0.001–10.0 s | 0.5 | Also used as release time |
| `moog_env1_sustain` | 0.0–1.0 | 0.3 | |

### Loudness Contour (3 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_env2_attack` | 0.001–10.0 s | 0.003 | Skew 0.3 |
| `moog_env2_decay` | 0.001–10.0 s | 0.2 | Also used as release time |
| `moog_env2_sustain` | 0.0–1.0 | 0.85 | |

### Glide (1 param)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_glide_time` | 0.0–10.0 s | 0.05 | 0 = micro-glide only (1ms) |

### Modulation (4 params)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_lfo_pitch_amount` | 0.0–1.0 | 0.0 | Mod source → pitch depth |
| `moog_vcf_lfo_amount` | 0.0–1.0 | 0.0 | Mod source → filter depth |
| `moog_lfo_rate` | 0.1–30.0 Hz | 5.0 | LFO rate (when Osc3 not used as mod source) |
| `moog_lfo_waveform` | 0–4 | 0 (Sine) | Sine/Tri/Saw/Sq/S&H |

### Master (1 param)
| Parameter ID | Range | Default | Notes |
|---|---|---|---|
| `moog_master_vol` | 0.0–1.0 | 0.8 | Output level before mixer bus |

**Total: 37 parameters** (matching the existing Moog 15 count)

---

## 12. Parameter Compatibility

Parameters that map 1:1 from Moog 15 → Model D keep the same ID. Removed parameters are ignored by the preset loader. New parameters get defaults that produce sound.

| Moog 15 Param | Model D Equivalent | Notes |
|---|---|---|
| `moog_vco1_waveform` | `moog_vco1_waveform` | Index mapping changes (0=Tri now) |
| `moog_vco1_range` | `moog_vco1_range` | Was float 2–32, now choice 0–4 |
| `moog_mix_vco1/2/3` | `moog_vco1/2/3_level` | Renamed for clarity |
| `moog_env1/2_release` | **Removed** | Decay IS the release (Model D authentic) |
| `moog_lfo_depth` | **Removed** | Use `moog_lfo_pitch_amount` / `moog_vcf_lfo_amount` |
| — | `moog_feedback` | **New** — output feedback gain |
| — | `moog_vco3_ctrl` | **New** — Osc 3 as LFO switch |
| — | `moog_master_vol` | **New** — output level |

---

## 13. Voice Lifecycle — The Pop-Free Design

This is the core reason for building a new module:

```
┌─ Construction ─────────────────────────────────────────────────┐
│  VCO 1, 2, 3: phase accumulators initialized, start running   │
│  Filter: all stages at 0.0                                     │
│  Contours: output = 0.0, state = Idle                          │
│  feedbackSample = 0.0                                          │
└────────────────────────────────────────────────────────────────┘

┌─ startNote() ──────────────────────────────────────────────────┐
│  1. glideTarget = new frequency                                │
│  2. filterContour.noteOn()  ← starts attack from current level│
│  3. loudnessContour.noteOn() ← starts attack from current level│
│  4. That's it. Nothing else changes.                           │
│     - Oscillators keep running (no reset)                      │
│     - Filter keeps running (no reset)                          │
│     - currentFreq glides to target via micro-glide in render   │
└────────────────────────────────────────────────────────────────┘

┌─ stopNote(allowTailOff=true) ──────────────────────────────────┐
│  1. filterContour.noteOff()  ← enters Release (uses decayRate)│
│  2. loudnessContour.noteOff() ← enters Release                │
└────────────────────────────────────────────────────────────────┘

┌─ stopNote(allowTailOff=false) — voice steal ───────────────────┐
│  1. filterContour.noteOff()                                    │
│  2. loudnessContour.noteOff()                                  │
│  3. clearCurrentNote()                                         │
│  (Next startNote() will retrigger from current contour level)  │
└────────────────────────────────────────────────────────────────┘

┌─ renderNextBlock() ────────────────────────────────────────────┐
│  For each sample:                                              │
│    1. Advance contour generators                               │
│    2. If loudnessContour is Idle → clearCurrentNote(), break   │
│    3. Micro-glide: currentFreq → glideTarget (≥1ms always)    │
│    4. Apply pitch mod (osc3/LFO + mod wheel)                   │
│    5. VCO 1, 2, 3: advance phase, generate polyBLEP samples   │
│    6. Mixer: sum VCOs + noise + feedback                       │
│    7. Filter: Huovilainen ladder (never reset, always running) │
│    8. VCA: filtered × loudnessContour × masterVol              │
│    9. Soft clip: tanh(output × 1.2)                            │
│   10. Store feedbackSample for next iteration                  │
│   11. addSample to buffer                                      │
└────────────────────────────────────────────────────────────────┘
```

**Why this can't pop:**
- Oscillators: continuous waveform, no phase reset → no waveform discontinuity
- Pitch: always micro-glides → no instant frequency jump under amplitude
- Contours: retrigger from current level → no amplitude discontinuity
- Filter: never reset → no state discontinuity
- No anti-click counter needed — the contour attack IS the anti-click

---

## 14. Preset Mapping — "Creamy Lead" Equivalent

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Moog15>
  <PARAM id="moog_vco1_waveform" value="2"/>       <!-- Saw -->
  <PARAM id="moog_vco1_range" value="1"/>           <!-- 16' -->
  <PARAM id="moog_vco1_level" value="0.8"/>
  <PARAM id="moog_vco1_pw" value="0.5"/>
  <PARAM id="moog_vco2_waveform" value="2"/>        <!-- Saw -->
  <PARAM id="moog_vco2_range" value="1"/>           <!-- 16' -->
  <PARAM id="moog_vco2_detune" value="6.0"/>
  <PARAM id="moog_vco2_level" value="0.6"/>
  <PARAM id="moog_vco2_pw" value="0.5"/>
  <PARAM id="moog_vco3_waveform" value="2"/>        <!-- Saw -->
  <PARAM id="moog_vco3_range" value="1"/>           <!-- 16' -->
  <PARAM id="moog_vco3_detune" value="-5.0"/>
  <PARAM id="moog_vco3_level" value="0.5"/>
  <PARAM id="moog_vco3_pw" value="0.5"/>
  <PARAM id="moog_vco3_ctrl" value="0"/>            <!-- Off -->
  <PARAM id="moog_noise_level" value="0.0"/>
  <PARAM id="moog_noise_color" value="0"/>
  <PARAM id="moog_feedback" value="0.15"/>          <!-- Slight warmth -->
  <PARAM id="moog_vcf_cutoff" value="2000.0"/>
  <PARAM id="moog_vcf_resonance" value="0.2"/>
  <PARAM id="moog_vcf_drive" value="1.8"/>
  <PARAM id="moog_vcf_env_depth" value="0.3"/>
  <PARAM id="moog_vcf_key_track" value="1"/>        <!-- 33% -->
  <PARAM id="moog_env1_attack" value="0.005"/>
  <PARAM id="moog_env1_decay" value="0.5"/>
  <PARAM id="moog_env1_sustain" value="0.6"/>
  <PARAM id="moog_env2_attack" value="0.003"/>
  <PARAM id="moog_env2_decay" value="0.2"/>
  <PARAM id="moog_env2_sustain" value="0.85"/>
  <PARAM id="moog_glide_time" value="0.05"/>
  <PARAM id="moog_lfo_rate" value="5.0"/>
  <PARAM id="moog_lfo_waveform" value="0"/>
  <PARAM id="moog_lfo_pitch_amount" value="0.0"/>
  <PARAM id="moog_vcf_lfo_amount" value="0.0"/>
  <PARAM id="moog_master_vol" value="0.8"/>
</Moog15>
```

---

## 15. Source File Plan

| File | Purpose |
|---|---|
| `Source/Modules/Moog15/Moog15Processor.h/cpp` | Processor (rewritten: 2× oversample + MIDI scaling) |
| `Source/Modules/Moog15/Moog15Voice.h/cpp` | Voice (rewritten: free-running VCOs, contours, feedback) |
| `Source/Modules/Moog15/Moog15VCO.h/cpp` | VCO (extended: 6 waveforms, shark tooth, free-running) |
| `Source/Modules/Moog15/MoogContour.h/cpp` | **New** — ADS contour generator (replaces ADSREnvelope usage) |
| `Source/Modules/Moog15/MoogLadder.h/cpp` | **New** — Huovilainen ladder (replaces shared LadderFilter) |
| `Source/Modules/Moog15/Moog15Params.h/cpp` | Parameter layout (rewritten for 37 Model D params) |
| `Source/Modules/Moog15/Moog15Editor.h` | UI (updated knob layout) |

Files reused as-is: None — this is a clean rewrite of the DSP core while keeping the module slot and `moog_` prefix.
