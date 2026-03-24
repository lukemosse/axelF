# AxelF Synth Workstation — Comprehensive Technical Review

**Date**: June 2025
**Scope**: Every `.h` and `.cpp` file under `Source/`, plus `AxelF_Synth_Spec.md`
**Codebase**: ~10,500 lines across 60+ source files

---

## Table of Contents

1. [Synth Module Architecture](#1-synth-module-architecture)
2. [Pattern Engine & Sequencer](#2-pattern-engine--sequencer)
3. [Transport System](#3-transport-system)
4. [Master Mixer](#4-master-mixer)
5. [Mixer Side Panel](#5-mixer-side-panel)
6. [Effects Processing](#6-effects-processing)
7. [Preset System](#7-preset-system)
8. [UI Architecture](#8-ui-architecture)
9. [Scene Manager](#9-scene-manager)
10. [Audio Architecture & DSP](#10-audio-architecture--dsp)
11. [Code Quality & Issues](#11-code-quality--issues)

---

## 1. Synth Module Architecture

### 1.1 Overall Structure

All five modules follow a uniform pattern:

| Module | Namespace | Polyphony | Oscillator | Filter | Special |
|--------|-----------|-----------|------------|--------|---------|
| Jupiter-8 | `axelf::jupiter8` | 8-voice | 2× `Jupiter8DCO` (PolyBLEP) | `IR3109Filter` | — |
| Moog 15 | `axelf::moog15` | Monophonic (1 voice) | 3× `Moog15VCO` (PolyBLEP) | `LadderFilter` | Glide |
| JX-3P | `axelf::jx3p` | 6-voice | 2× `JX3PDCO` (PolyBLEP) | `IR3109Filter` | `JX3PChorus` |
| DX7 | `axelf::dx7` | 16-voice | 6× `DX7Operator` (FM sine) | — | 32 algorithms |
| LinnDrum | `axelf::linndrum` | 15-voice (one per drum) | Sample playback / synthetic | — | Step sequencer |

Each module has:
- **Processor** (`*Processor.h/.cpp`) — owns a `juce::Synthesiser`, creates parameter layout, delegates to voices
- **Voice** (`*Voice.h/.cpp`) — `juce::SynthesiserVoice` subclass, owns oscillators + filter + envelopes + LFO
- **Oscillator** (`*DCO.h/.cpp` / `*VCO.h/.cpp` / `DX7Operator.h/.cpp`) — waveform generation
- **Params** (`*Params.h/.cpp`) — `createParameterLayout()` returning `juce::AudioProcessorValueTreeState::ParameterLayout`
- **Editor** (`*Editor.h/.cpp`) — UI panel with knobs/combos/section boxes

Every module processor inherits from `ModuleProcessor` (abstract base in `Common/ModuleProcessor.h`) which provides `prepareToPlay()`, `processBlock()`, `releaseResources()`, and MIDI channel routing fields.

Each module owns its own `juce::AudioProcessorValueTreeState` (APVTS) backed by a `DummyProcessor` wrapper, because JUCE requires an `AudioProcessor` to own an APVTS. The main `AxelFProcessor` holds a single master APVTS but each module's APVTS is separate.

### 1.2 Jupiter-8 (31 APVTS Parameters)

**Architecture**: 2 DCOs → mixer → IR3109 filter → VCA. Two ADSR envelopes (env1→filter, env2→amp). One shared LFO.

**DCO**: `Jupiter8DCO` implements PolyBLEP sawtooth, pulse, and sub-oscillator (square -1 octave). Range: 2'/4'/8'/16' (multiplier = 8/range). DCO-2 adds: detune ±50 cents, noise waveform (XOR-shift PRNG at index 2).

**Filter**: `IR3109Filter` — Roland OTA 4-pole model with TPT structure. Env1 depth × 10000 Hz + LFO × 3000 Hz + keyboard tracking (offset from middle C). Resonance multiplied by 3.8 with `tanh` soft-clip feedback.

**Stub parameters**: `voice_mode`, `pulse_width`, `sub_level`, `noise_level`, `portamento`, `cross_mod`, `sync`, `HPF` — all appear as APVTS parameters and UI controls, but are **NOT wired** in `Jupiter8Voice::renderNextBlock()`. They exist in the parameter tree and the editor, but do nothing to the audio.

### 1.3 Moog 15 (32 APVTS Parameters)

**Architecture**: 3 VCOs → mixer → Ladder filter → VCA. Monophonic with glide. Two ADSR envelopes. One LFO.

**VCO**: `Moog15VCO` — PolyBLEP sawtooth, triangle, pulse, square. Extended range: 2'/4'/8'/16'/32' (multiplier = 16/range). VCO-2 and VCO-3 each have ±100 cent detune.

**Filter**: `LadderFilter` — Moog ladder model with 4-pole TPT structure. Drive parameter (1.0–5.0) feeds into `tanh(input × drive - resonance × 4 × feedback)`. Higher resonance multiplier (4.0 vs Jupiter's 3.8).

**Glide**: Frequency smoothing with rate-based approach (0–2 seconds). Implemented in the voice's `renderNextBlock`.

**Fully wired**: All 32 parameters are functional — no stubs.

### 1.4 JX-3P (29 APVTS Parameters)

**Architecture**: 2 DCOs → mixer → IR3109 filter → VCA → JX3P Chorus. Two ADSR envelopes. One LFO.

**Chorus**: `JX3PChorus` — stereo delay-line chorus with 3 modes:
- Off: bypass
- I: 7ms base delay, 0.5 wet mix
- II: 12ms base delay, 0.7 wet mix

LFO-modulated delay with linear-interpolation read, max delay 4096 samples, safety bounds checks. Applied as a post-processor after the synth voices.

**Stub parameters**: `dco_cross_mod`, `dco_sync`, `chorus_spread` — exposed in APVTS and UI but NOT wired in the voice or chorus processor.

### 1.5 DX7 (68 APVTS Parameters)

**Architecture**: 6 operators with 32 selectable algorithms. Phase modulation synthesis. No filter.

**Operators**: `DX7Operator` — pure sine oscillator with phase-modulation input. Level conversion: `pow(10, (level-99)*0.025)` (DX7 0–99 scale). Ratio: 0.5–31.0. Detune: -7 to +7 (`detuneHz = detune × 0.01 × baseFreq`).

**Algorithms**: `DX7Algorithm` — all 32 DX7 algorithms faithfully implemented via bitmask topology:
- `modulators[6]`: bitmask of which operators modulate each operator
- `carrierMask`: which operators output to audio
- `feedbackOp`: which operator has self-feedback

Feedback is a single-sample delay self-modulation loop.

**Per-operator EG**: 4-stage (Rate1→Rate2→Rate3=sustain→Rate4=release). Simplified exponential approach.

**Free-running**: Operator phase is NOT reset on noteOn — authentic DX7 behavior.

**LFO**: Speed 0–99 → 0.1–50 Hz, PMD/AMD 0–99, 5 waveforms.

**Missing from spec requirements**:
- No oversampling (spec requires 4× minimum for FM aliasing suppression)
- No pitch envelope generator
- No keyboard level scaling
- No velocity sensitivity per-operator
- No rate scaling

### 1.6 LinnDrum (63 APVTS Parameters: 15 drums × 4 + 3 global)

**Architecture**: 15 sample-based voices (one per drum sound). Step sequencer.

**Sample loading**: Attempts disk load from hardcoded path (`Lindrum From Mars` sample pack). Falls back to procedurally generated synthetic sounds:
- Kicks/toms: sine sweeps (frequency decay)
- Cymbals/hi-hats: filtered noise bursts
- Claps/snare: noise bursts
- Cowbell/ride: metallic sine combinations
- Cabasa: high-passed noise

**Per-drum**: Level (0–1), Tune (±12 semitones via playback rate), Decay (0–1), Pan (-1 to +1). Equal-power pan law.

**GM note mapping**: kick=36, snare=38, closedHH=42, openHH=46, ride=51, crash=49, etc.

**Built-in Sequencer** (`LinnDrumSequencer`): 16 steps × 15 tracks, tempo, swing (50–100%). **However, this sequencer is NOT used** — the global `PatternEngine`'s `DrumPattern` handles all step sequencing instead. The `LinnDrumSequencer` class is dead code.

---

## 2. Pattern Engine & Sequencer

### 2.1 PatternEngine (`Common/PatternEngine.h/.cpp`)

Central pattern record/playback system. Manages:
- **4 SynthPatterns** (indices 0–3 for Jupiter-8, Moog 15, JX-3P, DX7)
- **1 DrumPattern** (index 4, LinnDrum)

**Record modes**: Overdub (merge new notes) or Replace (clear on loop restart).
**Per-module record arm**: Each module can be independently armed for recording.
**Quantize grids**: Off, Quarter, Eighth, Sixteenth, ThirtySecond note.
**Per-module mute**: Pattern playback can be muted per module.

**ProcessBlock flow**:
1. If recording + armed, capture incoming MIDI events (note-on/off) to the appropriate pattern with beat-position timestamps
2. If playing, generate playback MIDI from all unmuted patterns
3. Live MIDI always passes through (merged with pattern output)
4. Flush pending note-offs on transport stop and loop reset

### 2.2 SynthPattern (`Common/Pattern.h`)

Event-based MIDI pattern. Stores `MidiEvent` structs: `{noteNumber, velocity, startBeat, duration}`. Bar count synced from transport. Playback generates note-on at `startBeat` and note-off at `startBeat + duration`, with pending note-off tracking across block boundaries. Handles wrap-around at loop boundary.

### 2.3 DrumPattern (`Common/Pattern.h`)

Step grid: 15 tracks × up to 256 steps (16 bars × 16 steps/bar). Each step has `active` (bool) and `velocity` (float). Independent loop length per pattern (1–16 bars). Playback fires note-on events at 16th-note resolution (0.25 beats per step) within the drum-specific loop. DrumPattern loops independently from the global transport bar count.

---

## 3. Transport System

### 3.1 GlobalTransport (`Common/GlobalTransport.h/.cpp`)

**BPM**: 20–300 (clamped). Stored as `float`.
**Bar count**: 1–16 (the loop length).
**Time signature**: Numerator 1–16, denominator assumed 4 (quarter note).
**States**: `Stopped`, `CountIn`, `Playing`, `Recording`.

**Advance logic** (`advance(int numSamples)`):
- Computes `beatsPerSample = bpm / (60 × sampleRate)`
- Advances `positionInBeats` by `numSamples × beatsPerSample`
- Fires metronome tick callback on beat boundaries (beat transition detection)
- Detects loop reset when `positionInBeats ≥ totalBeats` (bar count × time sig numerator)

**Count-in**: 1 bar of metronome before play/record. Count-in beat tracking uses the same metronome tick infrastructure.

**Host sync**: `syncToHost(hostBpm, hostPpq, isPlaying)` — overwrites BPM, position, and play state from DAW host.

**Loop reset flag**: `didLoopReset()` returns true once per loop boundary, consumed by PatternEngine and SceneManager.

**Swing**: Stored (0–100%) but not directly applied in transport advance — consumed by LinnDrumSequencer (which is dead code).

### 3.2 MetronomeClick (`Common/MetronomeClick.h`)

Simple sine-burst synthesizer:
- Downbeat: 1500 Hz, amplitude 0.7
- Other beats: 1000 Hz, amplitude 0.5
- Duration: 15ms with quadratic decay envelope
- Rendered additively into the output buffer
- Enable/disable via atomic toggle

---

## 4. Master Mixer

### 4.1 MasterMixer (`Common/MasterMixer.h/.cpp`)

5 channel strips (one per module) with:
- **Level**: 0.0–1.0 (fader range in UI extends to 1.5)
- **Pan**: -1.0 (left) to +1.0 (right)
- **Mute**: boolean
- **Solo**: boolean — if ANY channel is solo'd, all non-solo'd channels are silenced
- **Peak level**: computed per-block for metering (magnitude × level)

**Pan law**: Equal-power — `cos/sin` at `θ = (pan+1) × π/4`.

**Process flow** (`process(AudioBuffer&, int moduleIndex)`):
1. Check solo logic (if any solo active, silence non-solo'd modules)
2. Apply mute check
3. Apply equal-power pan to stereo buffer
4. Scale by channel level
5. Compute peak level for metering

**Not implemented**: Aux sends (spec mentions them, MixerEditor subtitle says "Aux Sends + Output Routing", but no aux code exists).

---

## 5. Mixer Side Panel

### 5.1 MixerSidePanel (`UI/MixerSidePanel.h`)

Compact always-visible panel, 140px wide on the right edge of the window:
- 5 vertical faders with level meters
- Mute (M) / Solo (S) buttons per channel
- Master fader at bottom
- Module-colored channel labels: JUP8, MOOG, JX3P, DX7, LINN
- Level meters with green/yellow/red coloring (threshold: 0.7 / 0.9)
- Timer-driven repaint at 30 Hz for meter updates

**Direct coupling**: Faders write directly to `MasterMixer::ChannelStrip.level` (no APVTS mediation). This means mixer state is NOT saved/loaded via the main preset system — only through scene snapshots.

### 5.2 MixerEditor (`UI/MixerEditor.h`)

Full-page mixer view (currently accessible as a 6th tab, based on LookAndFeel `tabAccents` having 6 entries):
- 5 `ChannelStripComp` sub-components with vertical level faders, pan knobs, mute/solo buttons, and level meters
- Master volume fader in its own section box
- Per-channel accent colors matching module theme
- 30 Hz timer-driven meter refresh

**Note**: The MixerEditor exists as a component class but is **NOT added as a tab** in `PluginEditor.cpp`. Only 5 tabs are created (one per synth module). The Mixer tab is referenced in the LookAndFeel (6 accent colors, 6 MIDI channel labels) but never instantiated. This is dead code / incomplete feature.

---

## 6. Effects Processing

### 6.1 Implemented Effects

| Effect | Module | Type | Location |
|--------|--------|------|----------|
| JX-3P Chorus | JX-3P only | Stereo delay-line chorus | `JX3PChorus.h/.cpp` |
| Moog 15 Drive | Moog 15 only | Filter saturation (`tanh`) | Inside `LadderFilter` |
| Metronome | Global | Sine burst | `MetronomeClick.h` |

**JX-3P Chorus details**: Two modes with different base delays (7ms / 12ms), LFO-swept delay with linear interpolation, stereo spread. Post-synth in the JX-3P processor's processBlock.

### 6.2 Missing from Spec

The specification document calls for several effects that are **NOT implemented**:
- **Global effects bus** (reverb, delay, chorus as sends) — not present
- **Per-module insert effects** — not present (except JX-3P chorus)
- **Oversampling** — `Oversampler` class exists in `Common/DSP/Oversampler.h` wrapping `juce::dsp::Oversampling<float>` with configurable factor, but **NO module uses it**. The DX7 spec requirement for 4× minimum oversampling is unmet.

---

## 7. Preset System

### 7.1 Global Presets — PresetManager (`Common/PresetManager.h/.cpp`)

**Format**: XML files (`.axpreset`).
**Factory presets**: Scanned from `<exe_dir>/Presets/` directory. **No factory presets are shipped** — the directory scan runs but finds nothing.
**Save/Load**: Writes/reads all 5 module APVTS states into a single XML wrapper document.

### 7.2 Factory Preset Installation — FactoryPresets (`Common/FactoryPresets.h`)

`FactoryPresets::installIfNeeded()` writes hardcoded XML preset files to `%APPDATA%/AxelF/Voices/<Module>/` on first run. Only writes if files don't already exist (idempotent).

**Presets shipped**:
| Module | Preset Count | Names |
|--------|-------------|-------|
| Jupiter-8 | 5 | Classic Pad, Brass Lead, PWM Strings, Sub Bass, Sync Lead |
| Moog 15 | 3+ | Fat Bass, Acid Squelch, Warm Lead (+ possibly more, file was truncated) |
| JX-3P | (assumed similar count) | |
| DX7 | (assumed similar count) | |
| LinnDrum | (assumed similar count) | |

### 7.3 Per-Module Voice Presets — VoicePresetBar (`UI/VoicePresetBar.h`)

Each module editor has a `VoicePresetBar` component providing:
- Save / Load buttons (file chooser dialogs)
- Prev / Next navigation through discovered presets
- Preset name display
- Storage directory: `%APPDATA%/AxelF/Voices/<ModuleName>/`
- Format: Raw APVTS state XML (ValueTree)

The VoicePresetBar scans *.xml files in the module's voice directory and provides sequential prev/next navigation. Save creates new XML files. Load replaces the APVTS state.

---

## 8. UI Architecture

### 8.1 Window Layout

**Window size**: 1500 × 950 pixels (fixed, no resizing).

```
┌──────────────────────────────────────────┐
│ TopBar (44px)                            │
├──────────────────────────────────────────┤
│ PatternBankBar (28px)                    │
├──────────────────────────────┬───────────┤
│ TabbedComponent              │ MixerSide │
│ (5 tabs: Jupiter-8,         │ Panel     │
│  Moog 15, JX-3P, DX7,      │ (140px)   │
│  LinnDrum)                   │           │
│                              │           │
└──────────────────────────────┴───────────┘
```

### 8.2 Color System — AxelFColours (`UI/AxelFColours.h`)

Dark theme with module-specific accent colors:

| Token | Hex | Usage |
|-------|-----|-------|
| `bgDark` | `#1a1a2e` | Window/panel background |
| `bgPanel` | `#232340` | Recessed panel areas |
| `bgSection` | `#2d2d4a` | Section box fill |
| `bgControl` | `#3a3a5c` | Button/control background |
| `accentGold` | `#d4a843` | Logo, position display, active bank |
| `accentBlue` | `#5b8bd4` | Selection highlight |
| `accentRed` | `#d45b5b` | Mute, record, error |
| `accentGreen` | `#5bd47a` | Solo, play, success |
| `jupiter` | `#cc4444` | Jupiter-8 tab/section accent |
| `moog` | `#8b6914` | Moog 15 accent |
| `jx3p` | `#2b7a9e` | JX-3P accent |
| `dx7` | `#6b8e23` | DX7 accent |
| `linn` | `#8b4513` | LinnDrum accent |
| `mixer` | `#6a5acd` | Mixer accent |

### 8.3 LookAndFeel — AxelFLookAndFeel (`UI/AxelFLookAndFeel.h/.cpp`)

Custom `LookAndFeel_V4` subclass with overrides for:
- **Rotary slider**: Radial gradient knob body with highlight shine, indicator line, drop shadow
- **Linear slider**: Vertical track with rounded thumb, or horizontal with circle thumb
- **ComboBox**: Rounded rectangle with triangle arrow
- **Button**: Rounded rectangle with subtle shadow, brighter on hover/down
- **Tab buttons**: Module-colored bottom accent bar (4px), MIDI channel sub-labels, active/inactive coloring, module-specific active text colors

Tab bar has 6 accent color entries (5 modules + mixer), suggesting a 6th mixer tab was planned/removed.

### 8.4 Reusable UI Widgets

| Widget | File | Description |
|--------|------|-------------|
| `SectionBox` | `UI/SectionBox.h` | Rounded panel with titled header + bottom separator + content area |
| `ADSRDisplay` | `UI/ADSRDisplay.h` | Reactive ADSR curve visualization, attaches to 4 sliders |
| `AlgorithmDisplay` | `UI/AlgorithmDisplay.h` | DX7 algorithm topology diagram, carrier/modulator circles + connections |
| `DX7EGDisplay` | `UI/DX7EGDisplay.h` | 4-rate/4-level DX7 EG visualization with rate-proportional widths |
| `DrumStepGrid` | `UI/DrumStepGrid.h` | Multi-bar step sequencer grid with click-to-toggle, velocity brightness, playback cursor |
| `PatternStrip` | `UI/PatternStrip.h` | Per-synth-module pattern bar with record arm, mini piano roll, clear, quantize selector |
| `VoicePresetBar` | `UI/VoicePresetBar.h` | Per-module preset navigation (prev/next/save/load) |
| `TopBar` | `UI/TopBar.h` | Transport controls, BPM, position display, bar count, panic, preset name |
| `PatternBankBar` | `UI/PatternBankBar.h` | Scene bank buttons (A–D) with save |
| `MixerSidePanel` | `UI/MixerSidePanel.h` | Compact vertical mixer with faders + meters |
| `MixerEditor` | `UI/MixerEditor.h` | Full mixer view — **not instantiated** |

### 8.5 Module Editor Layout Pattern

All synth module editors (Jupiter-8, Moog 15, JX-3P, DX7) follow the same pattern:
1. Header area (22pt bold module name + 13pt italic description)
2. VoicePresetBar (32px strip)
3. Section boxes arranged in 2–3 rows (DCO/VCO, VCF, ENV1, ENV2, LFO, plus module-specific sections)
4. ADSR display views per envelope
5. PatternStrip at the bottom (50px)

Control positioning uses absolute coordinates within section boxes via `posKnob`/`posCombo` lambda helpers. Knobs are 60×60 or 48×48 (DX7) with 10pt labels.

The DX7 editor is the most complex: 72 sliders (2 global + 66 per-operator + 4 LFO), 1 combo, 6 mini EG displays, 1 algorithm diagram, packed into 6 operator rows.

The LinnDrum editor is unique: 15 drum "pads" in a 5×3 grid with 4 knobs each (level/tune/decay/pan), plus a full DrumStepGrid below.

---

## 9. Scene Manager

### 9.1 SceneManager (`Common/SceneManager.h/.cpp`)

**16 scenes** (labeled A through P), each storing:
- 4 synth patterns (copy of SynthPattern data)
- 1 drum pattern (copy of DrumPattern data)
- Mixer snapshot (level, pan, mute, solo for all 5 channels)

**Scene operations**:
- `saveToScene(index, patternEngine, mixerSnapshot)` — copies current patterns + mixer state into scene slot
- `loadScene(index, patternEngine, mixerSnapshot)` — restores scene data into active engine + mixer

### 9.2 Chain Mode

Sequence of scene indices with per-scene repeat counts:
- `setChain(vector<ChainEntry>)` where `ChainEntry = {sceneIndex, repeatCount}`
- `advanceChain(patternEngine, mixerSnapshot)` — called on loop boundary (when transport loop-resets)
- Decrements repeat counter; when exhausted, advances to next chain entry
- Wraps to beginning of chain when complete

### 9.3 PatternBankBar Integration

The `PatternBankBar` component only exposes 4 bank buttons (A–D), mapping to scene indices 0–3. The remaining 12 scenes (E–P) are accessible only programmatically — no UI for them. The save button saves current patterns + mixer state to the active bank.

---

## 10. Audio Architecture & DSP

### 10.1 Signal Flow

```
MIDI Input (live)
    ↓
PluginProcessor::processBlock
    ↓
┌─ GlobalTransport.advance(numSamples)
│   ├─ Metronome tick detection
│   └─ Loop reset detection
├─ MidiRouter (currently unused — active-tab routing used instead)
├─ PatternEngine.processBlock
│   ├─ Record incoming MIDI (if armed + recording)
│   └─ Generate playback MIDI (per-module patterns)
├─ All-notes-off injection (on module switch)
├─ ModulationMatrix.process
│   └─ 16 slots: target += source × amount (additive)
├─ For each module [0..4]:
│   └─ moduleProcessor.processBlock(tempBuffer, moduleMidi)
│       ├─ Synth render (voices)
│       └─ Module-specific post-processing (JX-3P chorus)
├─ MasterMixer.process (per module)
│   ├─ Solo/mute logic
│   ├─ Equal-power pan
│   ├─ Level scaling
│   └─ Peak metering
├─ MetronomeClick.renderBlock (additive on output)
└─ SceneManager chain advance (on loop reset)
```

### 10.2 Module Processing Order

Modules are processed **sequentially** (not parallel):
1. Jupiter-8, 2. Moog 15, 3. JX-3P, 4. DX7, 5. LinnDrum

Each module renders into its own temporary buffer, which is then mixed into the output via MasterMixer.

### 10.3 MIDI Routing

**Current**: "Active tab" model — all live MIDI is sent to `activeModuleIndex` only. PatternEngine plays back per-module MIDI to the correct module.

**MidiRouter class exists** with channel-based routing (per-module MIDI channel assignment), but is **NOT used** in processBlock. It's dead infrastructure.

### 10.4 DSP Components

#### ADSREnvelope (`Common/DSP/ADSREnvelope.h/.cpp`)
- States: Idle → Attack → Decay → Sustain → Release
- Attack: linear ramp (`rate = 1/(time × sr)`)
- Decay: exponential (multiplicative approach to sustain level)
- Release: exponential (multiplicative approach to 0)
- Minimum time: 1ms clamp

#### IR3109Filter (`Common/DSP/IR3109Filter.h/.cpp`)
- Roland OTA-based 4-pole lowpass
- TPT (topology-preserving transform) structure
- Bilinear transform: `g = tan(π × fc / sr)`
- One-pole sections: `G = g / (1 + g)`
- Feedback: `tanh(feedback) × resonance × 3.8`

#### LadderFilter (`Common/DSP/LadderFilter.h/.cpp`)
- Moog ladder 4-pole lowpass
- Similar TPT structure to IR3109
- Drive/saturation: `tanh(input × drive - resonance × 4 × feedback)`
- Higher resonance multiplier (4.0 vs 3.8)
- Drive parameter range: 1.0–5.0

#### LFOGenerator (`Common/DSP/LFOGenerator.h/.cpp`)
- 5 waveforms: Sine, Triangle, Sawtooth, Square, Sample&Hold
- Phase-increment based, wraps at 1.0
- S&H uses `std::rand()` — **not ideal for audio** (non-deterministic, platform-dependent)

#### Oversampler (`Common/DSP/Oversampler.h/.cpp`)
- Wraps `juce::dsp::Oversampling<float>` with half-band polyphase IIR
- Configurable factor (power of 2)
- **EXISTS BUT UNUSED** — no module calls it

### 10.5 Sample Rate & Block Size

- Default: 44100 Hz / 512 samples
- Prepared via `prepareToPlay()` propagation from `PluginProcessor` to all modules
- No explicit aliasing protection on subtractive synths (PolyBLEP handles fundamental aliasing but not FM)
- DX7 FM synthesis has **no anti-aliasing measures** — significant quality concern

### 10.6 Output Configuration

- Single stereo output bus (2 channels)
- No sidechain input
- No aux sends / multi-output

---

## 11. Code Quality & Issues

### 11.1 Critical Issues

| # | Severity | Area | Description |
|---|----------|------|-------------|
| 1 | **High** | DX7 | **No oversampling on FM synthesis**. The DX7 module generates significant aliasing at high modulation indices. The `Oversampler` class exists but is never used. Spec requires 4× minimum. |
| 2 | **High** | ModulationMatrix | **Direct atomic store to parameter values** bypasses APVTS notification system. Can cause parameter drift and inconsistent UI state. Should use `setValueNotifyingHost()` or similar. |
| 3 | **Medium** | Jupiter-8 | **10 stub parameters** (`voice_mode`, `pulse_width`, `sub_level`, `noise_level`, `portamento`, `cross_mod`, `sync`, `HPF`, `vcf_key_track`, `vcf_lfo_amount`) are exposed in UI but do nothing. User can tweak knobs that have no audio effect. |
| 4 | **Medium** | JX-3P | **3 stub parameters** (`dco_cross_mod`, `dco_sync`, `chorus_spread`) — same issue as Jupiter-8. |
| 5 | **Medium** | LinnDrum | **Hardcoded sample path** — looks for `Lindrum From Mars` sample pack at a fixed Windows path. No user-configurable sample directory. Falls back to synthetic generation. |
| 6 | **Medium** | MidiRouter | **Dead infrastructure** — `MidiRouter` class is fully implemented but never called. MIDI routing uses active-tab model instead. |
| 7 | **Medium** | LinnDrumSequencer | **Dead code** — `LinnDrumSequencer` class exists but is never used. PatternEngine handles all sequencing. |
| 8 | **Medium** | MixerEditor | **Dead code** — Full mixer page component exists but is never added as a tab. |

### 11.2 Design Concerns

| # | Area | Description |
|---|------|-------------|
| 1 | Mixer state | Mixer faders/mute/solo write directly to `ChannelStrip` fields — **not APVTS backed**. Mixer state is only preserved via scene snapshots, not in the main preset save/load. |
| 2 | LFO S&H | `LFOGenerator` uses `std::rand()` for Sample & Hold — non-deterministic, platform-dependent, not seedable. Should use a proper PRNG. |
| 3 | Header-only editors | All 5 module editors are defined entirely in header files (~200–300 lines each). The `.cpp` files are empty stubs. This increases compilation time but is not a functional issue. |
| 4 | Thread safety | `MetronomeClick.enabled` uses `std::atomic<bool>`, but `MasterMixer::ChannelStrip` members (level, pan, mute, solo) are plain `float`/`bool` written from UI thread and read from audio thread — potential data race. |
| 5 | PatternBankBar | Only exposes 4 of 16 available scenes (A–D). Scenes E–P are inaccessible from UI. |
| 6 | DX7 voice count | 16-voice polyphony is unusually high for FM synthesis CPU cost, especially without oversampling. Could cause CPU spikes. |
| 7 | Sequential processing | All 5 modules process sequentially — no parallelism. With a full arrangement playing, all 5 synths render on the audio thread back-to-back. |

### 11.3 Positive Observations

| # | Area | Observation |
|---|------|-------------|
| 1 | Architecture | Clean namespace hierarchy (`axelf::jupiter8`, `axelf::moog15`, etc.) with consistent module structure. |
| 2 | JUCE usage | Correct APVTS pattern with `SliderAttachment`/`ComboBoxAttachment` for all parameter bindings. |
| 3 | DSP quality | IR3109 and Ladder filter implementations are well-structured TPT models with proper `tanh` saturation. |
| 4 | DX7 algorithms | All 32 algorithm topologies faithfully implemented with bitmask routing — impressive completeness. |
| 5 | UI design | Cohesive dark theme with module-specific accent colors. Custom LookAndFeel with gradient knobs, meters, and section boxes. |
| 6 | Pattern engine | Solid event-based and step-based dual pattern system with quantize, record modes, and loop management. |
| 7 | Scene system | Well-structured scene/chain system with full pattern + mixer state snapshots. |
| 8 | PolyBLEP | All subtractive oscillators use PolyBLEP anti-aliasing — correct approach for virtual analog. |
| 9 | Undo-safe presets | `VoicePresetBar` uses `replaceState()` which is APVTS-compatible. Factory preset installation is idempotent. |
| 10 | Transport | Clean transport state machine with count-in, host sync, and loop-reset detection feeding into scene chain. |

### 11.4 File Inventory Summary

| Category | Files | Lines (est.) |
|----------|-------|-------------|
| Module Processors | 5 × (h + cpp) = 10 | ~1200 |
| Module Voices | 5 × (h + cpp) = 10 | ~1400 |
| Module Oscillators | 5 × (h + cpp) = 10 | ~800 |
| Module Params | 5 × (h + cpp) = 10 | ~1200 |
| Module Editors | 5 × (h + cpp) = 10 | ~2400 |
| Common DSP | 5 × (h + cpp) = 10 | ~600 |
| Common Infrastructure | 8 × (h + cpp) = 16 | ~1800 |
| UI Components | 12 headers + 1 cpp = 13 | ~1500 |
| Plugin Core | 4 (h + cpp each) | ~600 |
| Total | ~93 files | ~11,500 |

---

*End of technical review.*
