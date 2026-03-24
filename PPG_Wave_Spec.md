# PPG Wave 2.2/2.3 — Module Specification
## AxelF Synthesizer Workstation — Additional Module

**Version:** 1.0 Draft  
**Target Slot:** Module 6 (new slot, or swappable with DX7 slot 4)  
**Parameter Prefix:** `ppg_`  
**Default MIDI Channel:** 5

---

## Overview

The PPG Wave 2.2/2.3 (1982–84) was the first commercially successful wavetable synthesizer. It produces sounds that no subtractive or FM synth can achieve: metallic sweeps, evolving digital textures, vocal-like morphing timbres, and harsh harmonics that transition smoothly into warm analog-filtered tones. The combination of digital wavetable oscillators with analog Curtis CEM filters creates the PPG's unique "digital warmth."

**Sonic role in AxelF:** Evolving pads, metallic stabs, digital textures, glass-like leads, and timbres unreachable by the existing analog subtractive and FM modules.

---

## Architecture

- **Synthesis:** Wavetable (digital oscillators) + analog-modelled filter
- **Voice Count:** 8 voices, polyphonic
- **Oscillators per voice:** 2 wavetable oscillators (WaveA / WaveB)
- **Voice Assign Modes:** Poly, Unison (8-voice stack), Dual (split at configurable note)
- **Voice Stealing:** Oldest-note-first, with release-phase voices stolen before held voices. In Unison mode, no stealing occurs (monophonic retrigger).
- **Stereo Output:** Mono sum in Poly mode. In Unison mode, voices are spread across the stereo field using `ppg_unison_detune` — detuned voices are panned alternately L/R with increasing spread, producing pseudo-stereo width proportional to the detune amount.

---

## Wavetable Oscillators (WaveA / WaveB)

### Wavetable Structure

Each oscillator reads from a **wavetable**: an ordered sequence of 64 single-cycle waveforms. The "wave position" parameter selects which waveform in the table is currently played. Modulating wave position with an envelope or LFO creates the PPG's signature evolving-timbre sound.

Each single-cycle waveform is 256 samples at the base pitch. Multiple bandlimited versions (mipmaps) are precomputed at octave intervals to prevent aliasing.

### Factory Wavetables

| # | Name | Character |
|---|---|---|
| 0 | Upper Waves | Classic PPG — saw-to-pulse morphing |
| 1 | Resonant | Filter-swept resonant peaks |
| 2 | Resonant 2 | More extreme resonant sweeps |
| 3 | Mallet Waves | Struck/plucked attack transients |
| 4 | Anharmonic | Bells, metallic, inharmonic partials |
| 5 | Electric Piano | EP-like tine/bark timbres |
| 6 | Bass Waves | Sub-heavy fundamentals |
| 7 | Organ Waves | Drawbar-like additive constructions |
| 8 | Vocal Waves | "Ah", "Oh", formant-like shapes |
| 9 | Pad Waves | Soft, evolving, chorused |
| 10 | Digital Harsh | Bit-crushed, aliased, aggressive |
| 11 | Sync Sweep | Hard-sync-like harmonics |
| 12 | Classic Analog | Sine → Saw → Square → Pulse |
| 13 | Additive | Pure overtone series, progressive |
| 14 | Noise Shapes | Noisy → tonal transition |
| 15 | User Table | Loadable from file (see User Wavetable Loading below) |

### Per-Oscillator Parameters

| Parameter | ID | Range | Notes |
|---|---|---|---|
| Wavetable Select | `ppg_waveA_table` / `ppg_waveB_table` | 0–15 | Which wavetable bank |
| Wave Position | `ppg_waveA_pos` / `ppg_waveB_pos` | 0–63 | Position within table |
| Wave Env Amount | `ppg_waveA_env_amt` / `ppg_waveB_env_amt` | ±63 | Env 3 → wave position depth |
| Wave LFO Amount | `ppg_waveA_lfo_amt` / `ppg_waveB_lfo_amt` | 0–63 | LFO → wave position depth |
| Range | `ppg_waveA_range` / `ppg_waveB_range` | 64', 32', 16', 8', 4', 2' | Foot register |
| Semitone | `ppg_waveA_semi` / `ppg_waveB_semi` | ±12 | Coarse pitch |
| Detune | `ppg_waveA_detune` / `ppg_waveB_detune` | ±50 cents | Fine pitch |
| Keytrack | `ppg_waveA_keytrack` / `ppg_waveB_keytrack` | On / Off | Fixed-pitch mode for effects |
| Level | `ppg_waveA_level` / `ppg_waveB_level` | 0–127 | Mix level |

### Wavetable Interpolation

Between adjacent wave positions, the oscillator crossfades using linear interpolation of the single-cycle waveforms. This ensures smooth timbral morphing when the wave position is modulated by an envelope or LFO. No discontinuous jumps.

### Boundary Behaviour

Wave position is **clamped** to [0, 63] — not wrapped. Position 63 holds the last waveform; overshooting via envelope or LFO modulation saturates at 63. Undershooting clamps at 0. This matches the original PPG hardware (no wraparound).

---

## Sub-Oscillator

A simple sub-oscillator adds low-end weight:

| Parameter | ID | Range | Notes |
|---|---|---|---|
| Waveform | `ppg_sub_wave` | Sine, Square | |
| Octave | `ppg_sub_octave` | -1, -2 | Relative to WaveA |
| Level | `ppg_sub_level` | 0–127 | |

---

## Noise Generator

| Parameter | ID | Range |
|---|---|---|
| Level | `ppg_noise_level` | 0–127 |
| Color | `ppg_noise_color` | White / Pink |

---

## Mixer

| Parameter | ID | Range |
|---|---|---|
| WaveA Level | `ppg_mix_waveA` | 0–127 |
| WaveB Level | `ppg_mix_waveB` | 0–127 |
| Sub Level | `ppg_mix_sub` | 0–127 |
| Noise Level | `ppg_mix_noise` | 0–127 |
| Ring Mod Level | `ppg_mix_ringmod` | 0–127 |

Ring modulation is WaveA × WaveB (bipolar multiplication). `ppg_mix_ringmod` is a **level control** — the ring mod output is mixed alongside WaveA/WaveB/Sub/Noise as an additional source (not a dry/wet blend). At 0 the ring mod contributes nothing; at 127 it's at full amplitude in the mix.

---

## Filter (VCF)

Primary model: **SSM 2044** (PPG Wave 2.2). This is the more characterful filter — a 4-pole 24 dB/oct low-pass with a distinctive "scooped" resonance that boosts a narrow peak without the bass-loss typical of Roland IR3109 filters. The CEM 3320 (Wave 2.3) is smoother and more polite; we're choosing the 2.2 behaviour for its personality.

| Parameter | ID | Range | Notes |
|---|---|---|---|
| Cutoff | `ppg_vcf_cutoff` | 20 Hz – 20 kHz | Logarithmic |
| Resonance | `ppg_vcf_resonance` | 0–127 | Self-oscillates at max |
| Envelope Amount | `ppg_vcf_env_amt` | ±127 | Bipolar, from Env 1 |
| LFO Amount | `ppg_vcf_lfo_amt` | 0–127 | |
| Keyboard Tracking | `ppg_vcf_keytrack` | 0%, 50%, 100% | |
| Filter Type | `ppg_vcf_type` | LP24, LP12, BP12, HP12 | Multimode — see note |

### Filter Type Note

The original PPG Wave 2.2 was LP24 only. The LP12/BP12/HP12 modes are an **extension beyond the original hardware** — a practical concession for modern use, since wavetable oscillators benefit from bandpass and highpass filtering for timbral variety. LP24 is the default and the only mode that authentically represents the SSM 2044 character. The other modes use the same filter topology reconfigured (state-variable filter with mode-selectable output taps).

### Filter Implementation

The SSM 2044 is modelled as a 4-pole state-variable filter with resonance compensation (no bass loss at high Q). Approximate the SSM's nonlinear saturation with a `tanh` soft-clip on each stage's input. Reference: Välimäki & Smith, "Virtual Analog Filter Design" (2012). Reuse the existing `Oversampler` for anti-aliasing at high resonance. Create `SSM2044Filter` as a new class using the same `processSample(float input)` interface pattern as `IR3109Filter` / `LadderFilter`.

---

## Amplifier (VCA)

| Parameter | ID | Range |
|---|---|---|
| Level | `ppg_vca_level` | 0–127 |
| Velocity Sensitivity | `ppg_vca_vel_sens` | 0–7 |

Driven by Env 2. Velocity sensitivity range 0–7 matches the original PPG's 3-bit hardware value (same convention as the DX7 operator velocity sensitivity). 0 = no velocity response (fixed level), 7 = full velocity scaling.

---

## Envelopes (× 3)

The PPG has three envelope generators — one more than typical subtractive synths. Env 3 is the wave-position modulator that gives the PPG its signature sound.

### Env 1 — Filter Envelope

| Stage | Range |
|---|---|
| Attack | 0.5 ms – 10 s |
| Decay | 0.5 ms – 10 s |
| Sustain | 0–127 |
| Release | 0.5 ms – 10 s |

### Env 2 — Amplitude Envelope

Same ranges as Env 1.

### Env 3 — Wave Envelope (wavetable position modulation)

This is the PPG's killer feature. Env 3 modulates the wave position of WaveA and/or WaveB, sweeping through the wavetable over the note's duration.

| Stage | Range |
|---|---|
| Attack | 0.5 ms – 10 s |
| Decay | 0.5 ms – 10 s |
| Sustain | 0–127 |
| Release | 0.5 ms – 10 s |

| Routing | ID | Range |
|---|---|---|
| Env 3 → WaveA depth | `ppg_waveA_env_amt` | ±63 |
| Env 3 → WaveB depth | `ppg_waveB_env_amt` | ±63 |

**Example:** WaveA position = 5, Env 3 amount = +40. On note-on, the attack sweeps from position 5 to 45, then decays back toward 5 × sustain level. The timbre evolves from a sine-like wave through increasingly complex harmonics and settles at a sustained point.

---

## LFO

| Parameter | ID | Range |
|---|---|---|
| Waveform | `ppg_lfo_wave` | Triangle, Sawtooth, Reverse Saw, Square, Sine, S&H |
| Rate | `ppg_lfo_rate` | 0.01 Hz – 50 Hz |
| Delay | `ppg_lfo_delay` | 0 – 5 s |
| → Pitch (WaveA+B) | `ppg_lfo_pitch_amt` | 0–127 |
| → Filter Cutoff | `ppg_lfo_cutoff_amt` | 0–127 |
| → Wave Position A | `ppg_waveA_lfo_amt` | 0–63 |
| → Wave Position B | `ppg_waveB_lfo_amt` | 0–63 |
| → Amplitude | `ppg_lfo_amp_amt` | 0–127 |
| Sync | `ppg_lfo_sync` | On / Off |

---

## Performance Controls

| Parameter | ID | Range | Notes |
|---|---|---|---|
| Pitch Bend Range | `ppg_bend_range` | ±1–12 semitones | |
| Mod Wheel | CC1 | → LFO pitch depth | Standard vibrato |
| Aftertouch | Channel AT | → Filter cutoff, wave position | Configurable |
| Portamento Time | `ppg_porta_time` | 0 – 5 s | |
| Portamento Mode | `ppg_porta_mode` | Off / Legato / Always | |
| Unison Detune | `ppg_unison_detune` | 0–50 cents | Spread in unison mode |

### MIDI CC Map

| CC | Default Assignment | Notes |
|---|---|---|
| CC 1 | LFO → Pitch depth | Mod wheel — vibrato |
| CC 2 | Wave Position A | Breath controller / knob |
| CC 4 | Wave Position B | Foot controller / knob |
| CC 7 | VCA Level | Volume (standard) |
| CC 10 | Pan | (handled by MasterMixer, not the module) |
| CC 11 | Expression | Scales output level |
| CC 71 | Filter Resonance | Sound Controller 2 (standard) |
| CC 74 | Filter Cutoff | Brightness (standard) |
| CC 91 | Wave Env Amount A | Repurposed effect depth |
| CC 93 | Wave Env Amount B | Repurposed chorus depth |

All CC assignments are MIDI-learnable via the global MIDI Learn system. The defaults above follow GM/GS conventions where applicable (CC 7, 10, 11, 71, 74) and use commonly available CCs for PPG-specific controls.

---

## Implementation Notes

### Wavetable Storage

```
Each wavetable = 64 waveforms × 256 samples × sizeof(float) = 64 KB per table
16 tables × 64 KB = 1 MB total (trivial memory footprint)
```

Store as a flat `std::array<std::array<std::array<float, 256>, 64>, 16>` or load from a binary resource embedded via CMake. Bandlimited mipmaps (8 octave levels) multiply storage by 8× = ~8 MB total.

### Wavetable Oscillator DSP

```cpp
// Per-sample oscillator output
float wavePos = basePosition + env3Value * envAmount + lfoValue * lfoAmount;
wavePos = std::clamp(wavePos, 0.0f, 63.0f);

int posLow = static_cast<int>(wavePos);
int posHigh = std::min(posLow + 1, 63);
float frac = wavePos - static_cast<float>(posLow);

// Select bandlimited mipmap level based on current frequency
int mipLevel = getMipLevel(currentFrequency, sampleRate);

float sampleLow  = readWaveform(tableIndex, posLow,  phase, mipLevel);
float sampleHigh = readWaveform(tableIndex, posHigh, phase, mipLevel);

float output = sampleLow + frac * (sampleHigh - sampleLow);  // crossfade
```

### Bandlimiting Strategy

Each single-cycle waveform is stored as bandlimited mipmaps at octave intervals (C1, C2, C3, ... C8). At runtime, select the mipmap whose Nyquist is just above the played frequency. This prevents aliasing without runtime oversampling (wavetable oscillators are naturally alias-free with proper mipmaps).

### User Wavetable Loading

Table slot 15 is reserved for user-loadable wavetables:

- **File format:** 16-bit or 32-bit float WAV, mono, containing exactly 64 × 256 = 16,384 samples. The file is interpreted as 64 consecutive single-cycle waveforms of 256 samples each.
- **Loading:** Via a "Load Wavetable" button in the PPG editor panel. Opens a native file browser filtered to `.wav` files.
- **Validation:** File must be mono. Sample count must be exactly 16,384 (or a power-of-two multiple, downsampled to 256 per frame). Files failing validation show an error toast; the slot retains its previous content.
- **Persistence:** The loaded wavetable's file path is stored in the preset JSON. On preset recall, the file is reloaded from disk. If the file is missing, table 15 falls back to the Classic Analog table (12) with a warning.
- **Mipmap generation:** Bandlimited mipmaps are computed on load (background thread, ~10ms for a single table). Playback uses the previous mipmap set until generation completes.

### File Layout

```
Source/Modules/PPGWave/
├── PPGWaveProcessor.h/cpp     # Module processor, APVTS, voice management
├── PPGWaveEditor.h/cpp        # UI panel
├── PPGWaveVoice.h/cpp         # Per-voice: 2 wavetable oscs + sub + filter + envs
├── PPGWaveOsc.h/cpp           # Wavetable oscillator with mipmap lookup
├── PPGWaveParams.h/cpp        # APVTS parameter layout
├── SSM2044Filter.h/cpp        # SSM 2044 filter model
└── WavetableData.h            # Factory wavetable definitions (constexpr or binary)
```

### Integration Points

| Integration | Action Required |
|---|---|
| `MidiRouter` | Add 6th output buffer; default channel 5 |
| `MasterMixer` | Add 6th channel strip |
| `PluginProcessor` | Instantiate `PPGWaveProcessor`, route MIDI, call `processBlock` |
| `PluginEditor` | Add PPG Wave tab |
| `PresetManager` | Register `ppg` module, add factory preset bank |
| `ModulationMatrix` | Expose `ppg_*` parameters as targets |
| `CMakeLists.txt` | Add PPGWave source files |
| `SceneManager` | Include `ppgwave` in scene save/restore |

---

## Default Patch: "Glass Pad"

| Parameter | Value |
|---|---|
| Voice mode | Poly (8 voices) |
| WaveA table | 0 (Upper Waves) |
| WaveA position | 8 |
| WaveA env amt | +30 |
| WaveB table | 4 (Anharmonic) |
| WaveB position | 12 |
| WaveB env amt | +20 |
| WaveB detune | +7 cents |
| Mix WaveA | 100 |
| Mix WaveB | 80 |
| Filter cutoff | ~60% |
| Filter resonance | 25% |
| Filter env amount | +40% |
| Env 1 (filter) | A: 50ms, D: 800ms, S: 40%, R: 500ms |
| Env 2 (amp) | A: 20ms, D: 0, S: 100%, R: 600ms |
| Env 3 (wave) | A: 300ms, D: 2s, S: 30%, R: 1s |
| LFO | Triangle, 4 Hz, delay 0.5s, pitch amt 5 |

This produces a shimmering, evolving glass pad that immediately demonstrates the PPG's unique character — something none of the other five modules can produce.

---

## Factory Preset Bank (suggested)

| # | Name | Character | Key Parameters |
|---|---|---|---|
| 1 | Glass Pad | Evolving shimmer | Upper Waves, slow wave env |
| 2 | Metal Stab | Aggressive hit | Anharmonic table, fast attack, ring mod |
| 3 | Vowel Sweep | "Aah" to "Ooh" | Vocal Waves, env 3 sweep |
| 4 | Digital Bass | Deep + gritty | Bass Waves, low position, sub osc |
| 5 | Bell Tree | Crystalline decay | Mallet Waves, fast env 3, no sustain |
| 6 | Alien Lead | Eerie mono | Unison, Resonant table, high LFO→wave |
| 7 | Warm Strings | Lush stereo pad | Classic Analog, slow env3, unison detune |
| 8 | Harsh Sync | Aggressive lead | Sync Sweep table, resonance, fast env1 |
| 9 | Glass Keys | E-piano variant | EP table, vel sens high, moderate env3 |
| 10 | Noise Sweep | FX/riser | Noise Shapes, full env3 range, long attack |
| 11 | PPG Brass | Brassy stab | Upper Waves pos 40+, fast env1, LP24 |
| 12 | Ice Choir | Vocal + shimmer | Vocal + Pad tables (A/B), LFO→wave |

---

## Open Questions

- **Arpeggiator:** The original PPG had a built-in arpeggiator. Include one, or rely on external MIDI arpeggiation? (Recommendation: skip for v1, add as enhancement.)
- **Waveform Editor:** A graphical wave editor for user tables would be powerful but is a significant UI effort. Defer to v2.

---

*PPG Wave Module Spec v1.1 — AxelF Synthesizer Workstation*
