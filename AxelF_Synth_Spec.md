# AxelF — Synthesizer Workstation
## Technical Specification: Instrument Emulation Modules

**Version:** 1.0 Draft  
**Platform:** JUCE C++ Application  
**Instruments:** 5 Modules (Roland Jupiter-8, Moog Modular 15, Roland JX-3P, Yamaha DX7, LinnDrum)

---

## Overview

The AxelF workstation emulates the five instruments used to record *Axel F* (Harold Faltermeyer, 1984). Each instrument is implemented as a self-contained JUCE AudioProcessor module, loadable as individual tabs within the host workstation UI. All modules share a common MIDI routing layer, master clock/tempo, and a unified output mixer.

---

## Module 1 — Roland Jupiter-8 (Lead Synth)

**Role in track:** Iconic saw-wave lead melody.

### Architecture

- **Voice Count:** 8 voices, polyphonic (with unison mode collapsing all 8 voices to mono)
- **Oscillators per voice:** 2 (DCO-1 and DCO-2)
- **Voice Assign Modes:** Poly, Unison, Solo (last-note priority), Solo (low-note), Solo (high-note)

### Oscillators (DCO-1 / DCO-2)

| Parameter | Range | Notes |
|---|---|---|
| Waveform | Sawtooth, Pulse, Sub-Osc (square, -1 oct) | DCO-1 and DCO-2 independently selectable |
| Pulse Width | 0–100% | PW modulation available from LFO and Env |
| Range | 16', 8', 4', 2' | Foot register |
| Detune (DCO-2) | ±50 cents | Relative to DCO-1 |
| Cross-Mod | 0–127 | DCO-1 FM'd by DCO-2, hard-sync option |
| Sub-Oscillator Level | 0–127 | Square wave, one octave below DCO-1 |
| Noise Level | 0–127 | White noise source mixed pre-filter |

The canonical Axel F patch uses DCO-1 in **sawtooth** mode at 8', DCO-2 detuned ~6 cents, both in unison mode. A default init patch approximating this should be provided.

### Mixer

- DCO-1 Level
- DCO-2 Level
- Sub Level
- Noise Level

### Filter (VCF)

- **Type:** 4-pole (24 dB/oct) low-pass, modelled on the IR3109 filter chip; switchable to High-Pass
- **Cutoff Frequency:** 20 Hz – 20 kHz (logarithmic)
- **Resonance:** 0–127 (self-oscillates at maximum)
- **Keyboard Tracking:** 0%, 50%, 100%
- **Envelope Amount:** Bipolar, ±127
- **LFO Amount:** 0–127

### Amplifier (VCA)

- **Level:** 0–127
- **Envelope/Gate Switch:** VCA can be driven by Env 2 or held open by Gate

### Envelopes (ADSR × 2)

Both envelopes are identical in architecture:

| Stage | Range |
|---|---|
| Attack | 0.5 ms – 10 s |
| Decay | 0.5 ms – 10 s |
| Sustain | 0–127 |
| Release | 0.5 ms – 10 s |

- **Env 1** → Filter cutoff (amount: bipolar)
- **Env 2** → VCA (standard), optionally routable to pitch or PW

### LFO

| Parameter | Range |
|---|---|
| Waveform | Triangle, Sawtooth, Reverse Saw, Square, Sample & Hold |
| Rate | 0.01 Hz – 100 Hz |
| Delay | 0 – 5 s (onset delay for mod-wheel-triggered vibrato) |
| Destinations | DCO-1 pitch, DCO-2 pitch, PW, VCF cutoff |
| Amount | 0–127 per destination |

### Modulation

- **Mod Wheel (CC1):** Routes to LFO-to-pitch depth (vibrato)
- **Pitch Bend:** ±2 semitones (configurable ±1–12)
- **Aftertouch (Poly):** Routable to VCF, LFO depth, or pitch
- **Portamento:** 0 – 5 s (active in Solo modes)

### Arpeggiator

- **Modes:** Up, Down, Up/Down, Random
- **Range:** 1–3 octaves
- **Clock:** Internal (synced to host BPM) or MIDI clock

---

## Module 2 — Moog Modular 15 (Bass)

**Role in track:** Deep, warm sub-bass lines.

### Architecture

The Model 15 is a semi-fixed modular system. This emulation implements the canonical patching of the Model 15 cabinet as a performance-ready instrument, exposing the core signal path as panel controls, with a simplified patchbay for the most common routings.

- **Voice Count:** Monophonic
- **Oscillators:** 3 × VCO (901A master + 2 × 901B slaves)

### VCOs (901A/B)

| Parameter | Range | Notes |
|---|---|---|
| Waveform | Sine, Triangle, Sawtooth, Square, Pulse | 901B per oscillator |
| Range | 32', 16', 8', 4', 2' | Switched per oscillator |
| Frequency Fine | ±50 cents | Per oscillator |
| Pulse Width | 0–100% | Per oscillator |
| Oscillator Mix Level | 0–127 | Per oscillator into mixer |

The bass patch typically uses all three VCOs: VCO 1 and 2 as thick sawtooth unison, VCO 3 as a square wave one octave below, mixed heavily.

### Filter (904A Ladder)

The centrepiece of the Moog sound. A true 4-pole 24 dB/oct ladder filter model is required, capturing the characteristic saturation and resonance behaviour.

| Parameter | Range | Notes |
|---|---|---|
| Cutoff | 20 Hz – 20 kHz | |
| Resonance | 0–127 | Self-oscillates; at high levels exhibits vintage "ping" |
| Filter Drive | 0–127 | Input saturation before the ladder stages |
| Keyboard Tracking | 0%, 33%, 67%, 100% | Four-position switch |
| Envelope Amount | 0–127 | Unipolar |
| LFO Amount | 0–127 | |

### Envelopes (ADSR × 2, 911 modules)

- **Env 1** → Filter (Contour)
- **Env 2** → Amplifier (VCA)

| Stage | Range |
|---|---|
| Attack | 1 ms – 10 s |
| Decay | 1 ms – 10 s |
| Sustain | 0–100% |
| Release | 1 ms – 10 s |

### VCA (902)

- Level: 0–127
- Driven by Env 2

### LFO (921 used as slow oscillator)

| Parameter | Range |
|---|---|
| Waveform | Triangle, Sawtooth, Square, Sine |
| Rate | 0.01 Hz – 30 Hz |
| Amount to VCO pitch | 0–127 |
| Amount to VCF | 0–127 |

### S&H / Noise

- White and Pink noise sources available in mixer
- Sample & Hold module (984) clocked by LFO or external trigger, normalled to filter FM

### Pitch & Performance

- **Portamento/Glide:** 0 – 5 s (transistor-ladder slew emulation)
- **Pitch Bend:** ±2 semitones default (configurable)
- **Mod Wheel:** LFO to VCO depth (vibrato)
- **Velocity Sensitivity:** Maps to VCA level and/or Filter amount

### Patchbay (Simplified)

A visual patchbay panel with a subset of the Model 15's connections, allowing non-destructive routing changes:

- Env 1 → VCF amount (normalled)
- Env 2 → VCA (normalled)
- LFO → VCO pitch (normalled)
- LFO → VCF cutoff (switchable)
- S&H → VCF cutoff (switchable)
- Noise → VCO FM (switchable)

---

## Module 3 — Roland JX-3P (Chord Stabs)

**Role in track:** Punchy, pad-like chord stabs; polyphonic harmonic filler.

### Architecture

- **Voice Count:** 6 voices, polyphonic
- **Oscillators per voice:** 2 DCOs
- **Voice Assign Modes:** Poly, Unison (6-voice stack), Dual (split keyboard)

### DCOs

| Parameter | Range | Notes |
|---|---|---|
| DCO-1 Waveform | Sawtooth, Pulse, Square | |
| DCO-2 Waveform | Sawtooth, Pulse, Square, Noise | |
| DCO-1 Range | 4', 8', 16' | |
| DCO-2 Range | 2', 4', 8', 16' | Wider range than DCO-1 |
| Tune (DCO-2) | ±50 cents | |
| Cross-Mod | 0–127 | DCO-1 pitch modulated by DCO-2 |
| Sync | On/Off | DCO-2 synced to DCO-1 |

### Mixer

- DCO-1 Level (0–127)
- DCO-2 Level (0–127)

### Filter (IR3109 — same chip family as Jupiter-8)

| Parameter | Range |
|---|---|
| Type | 4-pole Low-Pass (24 dB/oct) |
| Cutoff | 20 Hz – 20 kHz |
| Resonance | 0–127 |
| Envelope Amount | 0–127 (unipolar) |
| LFO Amount | 0–127 |
| Keyboard Tracking | Off, Half, Full |

### Envelopes (ADSR × 2)

- **Env 1** → VCF
- **Env 2** → VCA

Identical architecture to Jupiter-8 envelopes.

### LFO

| Parameter | Range |
|---|---|
| Waveform | Triangle, Sawtooth, Square, Random |
| Rate | 0.05 Hz – 50 Hz |
| Delay | 0 – 3 s |
| Destinations | DCO-1 pitch, DCO-2 pitch, VCF cutoff, VCA level |

### Chorus (Built-in)

The JX-3P's onboard stereo chorus is a defining timbral characteristic.

- **Mode:** Off, Chorus I (mild), Chorus II (deep)
- **Rate:** 0.1 – 10 Hz (when in manual mode)
- **Depth:** 0–127
- **Stereo Spread:** 0–127

### PG-200 Programmer (Emulated)

The JX-3P was famously programmable only via the external PG-200 controller. All parameters exposed above are displayed and editable in the AxelF UI (no physical programmer required).

---

## Module 4 — Yamaha DX7 (Marimba/Bell Sound)

**Role in track:** The bright, percussive marimba-type melodic texture.

### Architecture

- **Synthesis:** 6-operator FM (Frequency Modulation synthesis)
- **Voice Count:** 16 voices, polyphonic
- **Algorithms:** All 32 DX7 algorithms available, selectable by number with a visual diagram showing carrier/modulator topology

### Operators (× 6, labelled 1–6)

Per operator:

| Parameter | Range | Notes |
|---|---|---|
| Frequency Ratio | 0.50 – 31.00 (ratio mode) | Standard DX7 coarse + fine |
| Frequency Fixed | 1 Hz – 9999 Hz | Fixed-frequency mode |
| Output Level | 0–99 | |
| Velocity Sensitivity | 0–7 | Scales output level with note velocity |
| Rate Scaling | 0–7 | Key tracking for EG speed |
| EG Rate 1–4 | 0–99 | Attack, Decay-1, Decay-2, Release rates |
| EG Level 1–4 | 0–99 | Level at each stage |
| Detune | -7 to +7 | Fine Hz detune |
| Oscillator Mode | Ratio / Fixed | Per operator |
| Keyboard Level Scaling | 0–99, curve shape | Left and right depth + breakpoint |
| Feedback | 0–7 | Applies to operator 1 (self-feedback loop) |

### Pitch Envelope Generator (PEG)

- 4 rate/level stages (Rate 1–4, Level 1–4)
- Global pitch contour, same model as operator EGs but applied to overall pitch

### LFO

| Parameter | Range |
|---|---|
| Waveform | Triangle, Sawtooth Down, Sawtooth Up, Square, Sine, S&H |
| Speed | 0–99 |
| Delay | 0–99 |
| PMD (Pitch Mod Depth) | 0–99 |
| AMD (Amp Mod Depth) | 0–99 |
| Sync | On/Off (LFO restarts on new note) |

### Performance

- **Pitch Bend Range:** 0–12 semitones (independent up/down configurable)
- **Mod Wheel:** Routes to LFO PMD and/or AMD
- **Aftertouch:** Routable to pitch, brightness, volume
- **Foot Controller:** Volume or expression
- **Portamento:** 0–99 rate, mono/poly

### Preset System

- Ships with the 32 factory cartridge voices (ROM-1A) as bundled presets
- The marimba/vibraphone patch should be accessible as a named default ("MARIMBA 1")
- User preset bank (32 slots) with load/save to file

### Note on FM Implementation

Operator EGs and output levels should follow the DX7's characteristic exponential scaling. Feedback must be implemented as a single-sample delay self-modulation loop. Aliasing suppression via oversampling (minimum 4× internal) is required due to FM's tendency to produce high-frequency energy.

---

## Module 5 — LinnDrum (Drums)

**Role in track:** All percussion: kick, snare, hi-hats, toms, claps, cowbell, cabasa.

### Architecture

- **Type:** Sample-based drum machine with per-voice analog-modelled processing
- **Voice Count:** 15 independent voices (matching LinnDrum's stock voice complement)
- **Polyphony:** All voices simultaneously (fully polyphonic / non-stealing, as per hardware)

### Voice Inventory

| # | Voice | Notes |
|---|---|---|
| 1 | Bass Drum | |
| 2 | Snare Drum | |
| 3 | Hi-Hat (Closed) | |
| 4 | Hi-Hat (Open) | Triggered open HH closes when CHH is triggered |
| 5 | Ride Cymbal | |
| 6 | Crash Cymbal | |
| 7 | High Tom | |
| 8 | Mid Tom | |
| 9 | Low Tom | |
| 10 | High Conga | |
| 11 | Low Conga | |
| 12 | Handclap | |
| 13 | Cowbell | |
| 14 | Tambourine | |
| 15 | Cabasa | |

### Per-Voice Controls

| Parameter | Range | Notes |
|---|---|---|
| Level | 0–127 | |
| Pan | L–C–R | |
| Tune | ±12 semitones | Pitch-shift of sample |
| Decay | 0–127 | Envelope release time applied to voice |
| Output Bus | Mix / Bus A / Bus B | Separate output routing |
| Mute Group | Off / Group 1–8 | Voices in same group mute each other (open/closed HH) |
| Solo | On/Off | |

### Global Controls

- **Swing:** 50%–75% (16th-note swing, equivalent to LinnDrum's "Groove" control)
- **Master Tune:** ±100 cents (scales all voice pitches simultaneously)
- **Click/Metronome:** On/Off (sent to separate output bus)

### Sequencer (Step Programmer)

- **Steps:** 16 or 32 per pattern
- **Patterns:** 99 user patterns + 8 factory example patterns
- **Resolution:** 16th notes (with swing applied)
- **Accent:** Per step, per voice (±6 dB hit variation)
- **Flam:** Per step, per voice (double-hit with configurable offset: 0–50 ms)
- **Copy/Paste/Clear:** Pattern-level operations
- **Chain:** Patterns can be chained into songs up to 999 bars

### MIDI

- Receives standard GM-mapped note-on events for real-time triggering
- Transmits step-sequencer output as MIDI notes (for recording into DAW)
- MIDI clock sync: internal master, external slave, or JUCE host transport

### Samples

The AxelF ships with a set of factory-accurate LinnDrum samples (12-bit, 27.5 kHz source fidelity, matching the original DAC characteristics). A sample-replacement mechanism allows each voice to load a custom WAV/AIFF file for non-vintage applications.

---

## Common Infrastructure (All Modules)

### MIDI

- All modules respond to independent MIDI channels (configurable 1–16 per module, or Omni)
- Global MIDI panic (all-notes-off, CC123) broadcast to all modules simultaneously
- Full MIDI learn on all automatable parameters

### Modulation Matrix (Global)

A global 16-slot modulation matrix is available across all modules, allowing sources (LFO, ENV, MIDI CC, velocity, aftertouch, mod wheel, key number) to be routed to any exposed parameter with configurable depth and polarity.

### Preset/Patch Management

- Each module maintains its own preset bank (minimum 128 patches)
- Global "Scene" saves a snapshot of all five modules simultaneously
- Preset files are human-readable JSON for portability
- Factory presets for each module include historically accurate patches from the original recordings

### Output Routing

- Each module exposes stereo main out + up to 2 auxiliary sends
- Master mixer section with per-module fader, pan, mute, solo
- Master output: stereo sum to host DAW

### Automation

All panel controls should be fully automatable via JUCE AudioProcessorValueTreeState (APVTS). Parameter IDs should follow the convention `<module>_<parameter>` (e.g., `jup8_dco1_waveform`, `dx7_op1_level`).

---

## Appendix A — Default Patch: "Axel Lead" (Jupiter-8)

| Parameter | Value |
|---|---|
| Voice mode | Unison (8 voices) |
| DCO-1 waveform | Sawtooth |
| DCO-2 waveform | Sawtooth |
| DCO-2 detune | +6 cents |
| Filter type | Low-pass |
| Filter cutoff | ~65% |
| Filter resonance | 15% |
| Filter env amount | +30% |
| Env 1 Attack | 5 ms |
| Env 1 Decay | 400 ms |
| Env 1 Sustain | 70% |
| Env 1 Release | 200 ms |
| Env 2 Attack | 5 ms |
| Env 2 Decay | 0 |
| Env 2 Sustain | 100% |
| Env 2 Release | 300 ms |
| Chorus | Off (Jupiter-8 dry) |
| Portamento | Off |

---

## Appendix B — Recommended JUCE Modules

| JUCE Module | Purpose |
|---|---|
| `juce_audio_processors` | Plugin/standalone host wrapper |
| `juce_dsp` | Filter, oversampling, IIR/FIR, waveshapers |
| `juce_audio_basics` | AudioBuffer, MIDI, sample-accurate timing |
| `juce_audio_formats` | WAV/AIFF sample loading (LinnDrum) |
| `juce_graphics` + `juce_gui_basics` | All UI panels |
| `juce_audio_plugin_client` | Optional VST3/AU export |

Third-party DSP libraries worth evaluating for filter accuracy: **Moog Ladder Filter** (reference implementations by Will Pirkle), **Korg 35 model** (for comparative benchmarking), **JUCE's own dsp::LadderFilter** as a starting baseline with the option to replace.

---

*AxelF Workstation — Spec v1.0. Subject to revision during implementation.*
