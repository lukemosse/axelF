# AxelF — Roadmap to 10/10

*A phased plan to bring every Sound on Sound review criteria to a perfect score.*

---

## Current Scores → Target

| Criteria               | Now | Target | Key Gaps |
|------------------------|:---:|:------:|----------|
| Sound Quality          | 7   | 10     | 26 stub params, no oversampling, no effects colouring |
| Feature Completeness   | 5   | 10     | Stub params, missing effects, incomplete presets |
| User Interface         | 7   | 10     | No preset browser, limited scene UI, no visual timeline |
| Workflow / Jamming     | 6   | 10     | Dry output, no performance effects, limited scene switching |
| Sequencer / Arrangement| 6   | 10     | No pattern copy/paste, no visual timeline, limited chain UI |
| Mixer / Effects        | 4   | 10     | Zero effects, no APVTS backing, thread-unsafe mixer |
| Preset Library         | 5   | 10     | Only 19 presets, no browser, no DX7 ROM voices |
| Stability / Polish     | 7   | 10     | Dead code, thread safety, no latency reporting |
| Value & Potential      | 7   | 10     | All the above combined |

---

## Phase 1 — Foundation & Thread Safety

*Fix structural issues that everything else depends on.*

### 1.1 APVTS-Back the Mixer

**Problem:** MasterMixer uses plain `float`/`bool` fields accessed from both audio and UI threads — no atomics, no serialization, no automation.

**Work:**
- Add 5 × 4 APVTS parameters to PluginProcessor: `mix_jup8_level`, `mix_jup8_pan`, `mix_jup8_mute`, `mix_jup8_solo` (and same for all 5 modules = 20 params)
- Add `mix_master_level` parameter
- Rewrite `MasterMixer::process()` to read from `std::atomic<float>*` cached pointers
- Wire `MixerSidePanel` faders/buttons via APVTS SliderAttachment and ButtonAttachment
- Remove the raw struct field read/write pattern
- Mixer state now auto-serializes with `getStateInformation()`/`setStateInformation()`

**Files:** `MasterMixer.h`, `MasterMixer.cpp`, `PluginProcessor.h`, `PluginProcessor.cpp`, `MixerSidePanel.h`

### 1.2 Thread-Safe Peak Metering

**Problem:** `peakLevel` is a plain float written by audio thread, read by UI timer.

**Work:**
- Change `peakLevel` to `std::atomic<float>`
- Write from `process()` with `store(val, std::memory_order_relaxed)`
- Read from UI timer with `load(std::memory_order_relaxed)`

**Files:** `MasterMixer.h`

### 1.3 Dead Code Removal

**Problem:** Three dead code files: `LinnDrumSequencer` (never called), `MidiRouter` (inline routing used instead), `MixerEditor` (replaced by MixerSidePanel).

**Work:**
- Delete `LinnDrumSequencer.h/cpp` (or wrap in `#if 0` if keeping for reference)
- Delete `MidiRouter.h/cpp` (routing is inline in PluginProcessor)
- Delete `MixerEditor.h/cpp` (replaced by MixerSidePanel)
- Remove from `CMakeLists.txt` `target_sources` and any `#include` references
- Clean up `PluginProcessor.h` member declaration for `MidiRouter`

**Files:** `CMakeLists.txt`, `PluginProcessor.h`, `PluginProcessor.cpp`, dead files

### 1.4 Latency Reporting

**Problem:** When oversampling is added (Phase 2), host needs latency info.

**Work:**
- In `PluginProcessor::prepareToPlay()`, sum latency from all oversampled modules
- Call `setLatencySamples()` with the total

**Files:** `PluginProcessor.cpp`

---

## Phase 2 — Sound Quality: Wire All Stub Parameters

*Every declared parameter must reach the DSP.*

### 2.1 Jupiter-8 — Wire 10 Stub Parameters

| Parameter | DSP Target |
|-----------|-----------|
| `jup8_voice_mode` | Voice allocator: Poly/Unison/Solo-Last/Solo-Low/Solo-High. Unison stacks all 8 voices on one note with detune spread. Solo modes set `juce::Synthesiser` to retrigger mono with note priority. |
| `jup8_pulse_width` | `Jupiter8DCO::setPulseWidth()` — modulate the PW threshold in the pulse waveform |
| `jup8_sub_level` | Mix square-wave one octave below DCO-1 into the voice output pre-filter |
| `jup8_noise_level` | Mix white noise source pre-filter |
| `jup8_vcf_key_track` | Scale filter cutoff by `(noteNumber - 60) / 60.0 * keyTrackAmount` |
| `jup8_vcf_lfo_amount` | Modulate VCF cutoff by `lfo.getOutput() * vcfLfoAmount` |
| `jup8_lfo_delay` | Delay LFO onset: ramp LFO depth from 0→1 over `lfoDelay` seconds after note-on |
| `jup8_portamento` | Smooth frequency glide between notes (per-sample exponential slew) |
| `jup8_dco_cross_mod` | FM DCO-1 by DCO-2 output: `dco1Freq += dco2Output * crossModDepth * dco1Freq` |
| `jup8_dco_sync` | Hard-sync DCO-2 to DCO-1: reset DCO-2 phase when DCO-1 wraps |

**Work:**
- Expand `Jupiter8Voice::setParameters()` signature to accept all 10 new values
- Add DSP code for each in `renderNextBlock()`
- Add sub-oscillator generator (square at half frequency, trivial)
- Add white noise source (`juce::Random` seeded per-voice)
- Implement portamento as frequency target smoothing
- Implement hard sync as phase-reset detection
- Update `Jupiter8Processor::updateVoiceParameters()` to load all 10 from APVTS

**Files:** `Jupiter8Voice.h/cpp`, `Jupiter8DCO.h/cpp`, `Jupiter8Processor.cpp`

### 2.2 Moog 15 — Wire 7 Stub Parameters

| Parameter | DSP Target |
|-----------|-----------|
| `moog_vco1_pw`, `moog_vco2_pw`, `moog_vco3_pw` | Per-VCO pulse width in pulse waveform mode |
| `moog_noise_level` | Mix white/pink noise pre-filter |
| `moog_vcf_key_track` | Scale cutoff by note number |
| `moog_vcf_lfo_amount` | Modulate cutoff by LFO |
| `moog_lfo_pitch_amount` | Modulate VCO pitch by LFO |

**Work:**
- Expand `Moog15Voice::setParameters()` to accept 7 new values
- Wire PW into each `Moog15VCO` 
- Add noise generator and mix pre-filter
- Add keyboard tracking and LFO modulation in `renderNextBlock()`

**Files:** `Moog15Voice.h/cpp`, `Moog15VCO.h/cpp`, `Moog15Processor.cpp`

### 2.3 JX-3P — Wire 9 Stub Parameters

| Parameter | DSP Target |
|-----------|-----------|
| `jx3p_dco1_range` | Octave transposition for DCO-1 (4', 8', 16' foot register) |
| `jx3p_mix_dco1`, `jx3p_mix_dco2` | Per-oscillator level into filter |
| `jx3p_vcf_lfo_amount` | Modulate cutoff by LFO |
| `jx3p_vcf_key_track` | Scale cutoff by note number |
| `jx3p_lfo_delay` | Delayed LFO onset after note-on |
| `jx3p_dco_cross_mod` | DCO-1 pitch modulated by DCO-2 output |
| `jx3p_dco_sync` | Hard-sync DCO-2 to DCO-1 |
| `jx3p_chorus_spread` | Stereo width of chorus effect |

**Work:**
- Expand voice `setParameters()` to accept all 9
- Wire octave transposition as frequency multiplier (0.5, 1.0, 2.0)
- Add oscillator mix levels, LFO modulation, keyboard tracking
- Add sync/cross-mod DSP (similar to Jupiter-8)
- Pass chorus spread to `JX3PChorus`

**Files:** `JX3PVoice.h/cpp`, `JX3PDCO.h/cpp`, `JX3PChorus.h/cpp`, `JX3PProcessor.cpp`

---

## Phase 3 — Oversampling

*Eliminate aliasing across all synthesis engines.*

### 3.1 DX7 — 4× Oversampling

**Problem:** FM synthesis generates extreme harmonic content. No oversampling = audible aliasing at moderate modulation depths.

**Work:**
- Instantiate `juce::dsp::Oversampling<float>` with order 2 (4×) and `filterHalfBandPolyphaseIIR`
- In `DX7Processor::prepareToPlay()`, call `oversampler.initProcessing()`; pass oversampled sample rate to all voices
- In `processBlock()`: upsample → `synth.renderNextBlock()` at 4× rate → downsample
- Report latency via `oversampler.getLatencyInSamples()`

**Files:** `DX7Processor.h/cpp`, `DX7Voice.h/cpp`

### 3.2 Jupiter-8 — 2× Oversampling

**Problem:** Pulse-width modulation and cross-mod create harmonics above Nyquist.

**Work:**
- Add `juce::dsp::Oversampling<float>` order 1 (2×) to Jupiter8Processor
- Wire into processBlock similar to DX7
- All oscillator phase increments recalculated at oversampled rate

**Files:** `Jupiter8Processor.h/cpp`

### 3.3 Moog 15 — 2× Oversampling

**Problem:** Ladder filter saturation + high resonance produce nonlinear harmonics.

**Work:**
- Same pattern as Jupiter-8: 2× oversampling in processBlock

**Files:** `Moog15Processor.h/cpp`

### 3.4 JX-3P — 2× Oversampling

**Problem:** Oscillator sync and cross-mod.

**Work:**
- Same pattern as Jupiter-8: 2× oversampling in processBlock

**Files:** `JX3PProcessor.h/cpp`

### 3.5 LinnDrum — No Oversampling

The 12-bit / 27.5 kHz character is intentional. Leave as-is.

---

## Phase 4 — Global Effects Bus

*The single biggest gap for a composition workstation.*

### 4.1 Effects Architecture

**Design:** Two global send effects + a master bus chain.

```
Module outputs → MasterMixer (per-channel fader/pan/mute/solo)
                      │
                 ┌────┴────┐
                 │  Dry Sum │
                 └────┬────┘
                      │
              ┌───────┼───────┐
              ▼       ▼       ▼
          Aux Send 1  Aux Send 2  (per-channel send levels already in MasterMixer spec)
              │       │
         ┌────▼────┐ ┌▼─────────┐
         │ Reverb  │ │  Delay   │
         │ (stereo)│ │ (stereo) │
         └────┬────┘ └────┬─────┘
              │           │
              └─────┬─────┘
                    ▼
            ┌───────────────┐
            │  Master Bus   │
            │  EQ → Comp →  │
            │  Limiter      │
            └───────┬───────┘
                    ▼
              Stereo Output
```

### 4.2 Reverb (Send Effect 1)

**Implementation:** Algorithmic plate/hall reverb.

**Parameters (APVTS-backed):**
- `fx_reverb_size` (0–1) — room size / decay time
- `fx_reverb_damping` (0–1) — high-frequency absorption
- `fx_reverb_width` (0–1) — stereo spread
- `fx_reverb_mix` (0–1) — wet/dry on the send return
- `fx_reverb_predelay` (0–100ms)

**Work:**
- Create `Source/Common/DSP/PlateReverb.h/cpp`
- Implement using Schroeder/Moorer allpass + comb structure or Dattorro plate topology
- Alternatively use `juce::dsp::Reverb` as a quality starting point, then customise

### 4.3 Stereo Delay (Send Effect 2)

**Parameters (APVTS-backed):**
- `fx_delay_time_l`, `fx_delay_time_r` (1ms–2000ms, tempo-syncable)
- `fx_delay_feedback` (0–0.95)
- `fx_delay_mix` (0–1)
- `fx_delay_sync` (off, 1/4, 1/8, dotted 1/8, 1/16, triplet)
- `fx_delay_highcut` (1kHz–20kHz) — darken repeats
- `fx_delay_ping_pong` (on/off)

**Work:**
- Create `Source/Common/DSP/StereoDelay.h/cpp`
- Circular buffer with interpolated read, HP/LP in feedback path
- Tempo sync via `GlobalTransport` BPM

### 4.4 Master Bus Processing

**Parameters:**
- `fx_master_eq_low` (±12 dB, shelf at 100 Hz)
- `fx_master_eq_mid` (±12 dB, parametric at 1 kHz, Q adjustable)
- `fx_master_eq_high` (±12 dB, shelf at 8 kHz)
- `fx_master_comp_threshold` (-40 to 0 dB)
- `fx_master_comp_ratio` (1:1 to 20:1)
- `fx_master_comp_attack` (0.1–100 ms)
- `fx_master_comp_release` (10–1000 ms)
- `fx_master_limiter` (on/off, ceiling at -0.3 dBFS)

**Work:**
- Create `Source/Common/DSP/MasterBus.h/cpp`
- 3-band EQ using `juce::dsp::IIR::Filter` (shelves + parametric)
- Compressor: envelope follower + gain computer
- Brickwall limiter: lookahead with `juce::dsp::Limiter` or custom

### 4.5 Per-Channel Sends

**Work:**
- Add `mix_jup8_send1`, `mix_jup8_send2` etc. APVTS params (10 total)
- Update `MasterMixer::process()` to tap post-fader signal into send buffers
- Sum send buffers → reverb/delay → add returns to master bus

### 4.6 Effects UI

**Work:**
- Add effects controls to MixerSidePanel (per-channel send knobs) or create a dedicated EffectsPanel below the mixer
- Add master bus controls accessible from the TopBar or a collapsible panel
- Per-channel send amount: small knob per strip in MixerSidePanel

**Files to create:**
- `Source/Common/DSP/PlateReverb.h/cpp`
- `Source/Common/DSP/StereoDelay.h/cpp`
- `Source/Common/DSP/MasterBus.h/cpp`
- `Source/UI/EffectsPanel.h` (optional dedicated panel)

**Files to modify:**
- `CMakeLists.txt` (add new .cpp files)
- `PluginProcessor.h/cpp` (instantiate effects, wire into processBlock)
- `MasterMixer.h/cpp` (aux send routing)
- `MixerSidePanel.h` (send knobs)

---

## Phase 5 — Preset Library & Browser

### 5.1 Expand Factory Presets

**Target:** Minimum 128 presets total (spec says 128 per module).

| Module | Current | Target | Priority additions |
|--------|:-------:|:------:|-------------------|
| Jupiter-8 | 5 | 25 | Axel Lead (canonical default), PWM strings, sync lead, polysaw pad, unison brass, filter sweep, S&H wobble, detuned fifth, bright stab, bell pad, warm keys, solo whistle, portamento lead, etc. |
| Moog 15 | 4 | 25 | Classic sub, reese bass, acid 303, filter bass, pluck bass, growl, wobble, FM bass, octave bass, legato lead, perc bass, etc. |
| JX-3P | 4 | 25 | Chorus poly, stab brass, lush pad, PWM ensemble, filter pluck, dark strings, bright poly, detune wash, etc. |
| DX7 | 4 | 32 | All 32 ROM-1A factory voices per spec: BRASS 1, BRASS 2, STRINGS 1, E.PIANO 1, MARIMBA, logs, etc. These are well-documented. |
| LinnDrum | 2 | 10 | Standard Kit, 808 Style, Gated Verb, Lo-Fi, Tight, Heavy, Synthetic, etc. |
| **Total** | **19** | **117** | |

**Work:**
- Create parameter value sets for each preset in `FactoryPresets.h` or as JSON files in `Resources/Presets/`
- DX7 ROM voices: well-documented parameter dumps exist (public domain)

### 5.2 Preset Browser UI

**Problem:** No visual preset browser. Just a label showing current preset name.

**Work:**
- Create `Source/UI/PresetBrowser.h/cpp` — modal or docked panel
- Category filtering (Bass, Lead, Pad, Keys, Percussion, FX)
- Search/filter by name
- Save/rename/delete user presets
- Factory vs User bank tabs
- Accessible from clicking the preset name label in TopBar

**Design:**
```
┌──────────────────────────────────┐
│ PRESET BROWSER            [X]   │
│ ┌──────────┐ ┌─────────────────┐│
│ │ Factory  │ │ Axel Lead     ★ ││
│ │ User     │ │ Brass Lead      ││
│ │          │ │ PWM Strings     ││
│ │ ──────── │ │ Sub Bass        ││
│ │ Bass     │ │ Sync Lead       ││
│ │ Lead     │ │ Classic Pad     ││
│ │ Pad      │ │ ...             ││
│ │ Keys     │ │                 ││
│ │ Perc     │ │                 ││
│ │ FX       │ │                 ││
│ └──────────┘ └─────────────────┘│
│ [Save] [Save As] [Delete]       │
└──────────────────────────────────┘
```

**Files:** `Source/UI/PresetBrowser.h/cpp`, `TopBar.h` (wire click handler)

---

## Phase 6 — Sequencer & Arrangement Upgrades

### 6.1 Pattern Copy/Paste/Clear

**Work:**
- Add `copyPattern(moduleIdx)`, `pastePattern(moduleIdx)`, `clearPattern(moduleIdx)` to PatternEngine
- Add Copy/Paste/Clear buttons to each module editor's pattern section or a global bar

### 6.2 Expanded Scene UI (16 scenes)

**Problem:** PatternBankBar only shows 4 scenes (A–D) of 16.

**Work:**
- Expand PatternBankBar to show banks A–P (or use pages: Bank 1–4 page selector × 4 banks per page)
- Alternative: scrollable scene strip with 16 slots
- Add drag-to-reorder for chain mode
- Show chain playback position indicator

### 6.3 Visual Pattern Overview / Timeline

**Work:**
- Create `Source/UI/ArrangementView.h` — horizontal timeline showing:
  - Lane per module (colour-coded)
  - Rectangular blocks for each pattern's bar range
  - Chain sequence visualised as consecutive blocks
  - Current playback position cursor
- Accessible as an additional tab or collapsible bottom panel

### 6.4 Swing Per Module

**Work:**
- The LinnDrum already has swing. Add per-module swing to each synth pattern in PatternEngine.
- `SynthPattern::swing` field (50–75%), applied during playback quantisation.

---

## Phase 7 — UI Polish & Performance Features

### 7.1 Keyboard / MIDI Monitor

**Work:**
- Add a small virtual keyboard at the bottom of the editor (or per-module) for mouse-click input
- MIDI activity LED per module in the TabBar or MixerSidePanel

### 7.2 Arpeggiator (Jupiter-8)

**Problem:** Spec calls for arpeggiator (Up, Down, Up/Down, Random, 1–3 octaves) on Jupiter-8.

**Work:**
- Create `Source/Modules/Jupiter8/Jupiter8Arpeggiator.h/cpp`
- Modes: Up, Down, Up/Down, Random
- Range: 1–3 octaves
- Tempo-synced to GlobalTransport
- APVTS params: `jup8_arp_mode`, `jup8_arp_range`, `jup8_arp_on`

### 7.3 Module Performance Features

Per the spec, additional performance features needed:

| Feature | Module | Status | Action |
|---------|--------|--------|--------|
| Pitch bend | All | Likely partial | Verify all modules respond to pitchWheelMoved() |
| Mod wheel (CC1) → vibrato | All | Needs verification | Wire CC1 to LFO depth |
| Aftertouch routing | Jupiter-8, DX7 | Missing | Route channel aftertouch to filter/pitch/level |
| Velocity sensitivity | DX7 | Per-operator in spec | Verify op-level velocity scaling works |
| Portamento | Jupiter-8, Moog | Jupiter needs wiring | Wire portamento param (Phase 2 handles this) |

### 7.4 Tooltips & Parameter Readout

**Work:**
- Add value readout to sliders on hover/drag (current numeric value)
- Add tooltips to all buttons and knobs showing parameter name and range
- Use JUCE's `setTooltip()` + `TooltipWindow`

### 7.5 Resizable Window

**Work:**
- Add `setResizable(true, true)` with aspect ratio constraint
- Set minimum size 1200×700, maximum 2400×1400
- All components use proportional layout in `resized()`

---

## Phase 8 — Stability & Code Quality

### 8.1 ModulationMatrix Verification

**Status:** Framework exists with 16 slots. Verify it's actually called in processBlock and that routing works end-to-end.

**Work:**
- Confirm `modMatrix.process(numSamples)` is called in PluginProcessor::processBlock
- Add at least one default modulation (e.g., mod wheel → filter cutoff) as a smoke test
- Wire mod matrix UI (assign source, assign target, set amount)

### 8.2 Sample-Accurate Parameter Smoothing Audit

**Work:**
- Audit every module voice's `renderNextBlock()` to ensure:
  - No raw `std::atomic` reads inside the sample loop
  - All audible params use `SmoothedValue` per the DSP conventions skill
  - Discrete params (waveform, algorithm) switch immediately without smoothing

### 8.3 Edge Case Hardening

| Issue | Fix |
|-------|-----|
| PatternEngine beat wrapping uses `if` not `while` | Change to `while` for extreme tempo/buffer size combos |
| Zero-pattern-length edge case | Guard `if (barCount <= 0) return;` |
| DX7 feedback at max (7) can produce DC offset | Add DC blocker after DX7 output: single-pole HPF at 5 Hz |

### 8.4 Unit Tests

**Work:**
- Add catch2 or JUCE's built-in test framework
- Test cases for: oscillator frequency accuracy, filter self-oscillation, envelope timing, preset serialisation round-trip, pattern record/playback, scene save/load
- At minimum: one DSP test per module, one UI state test per scene operation

---

## Phase 9 — Final Polish

### 9.1 About / Splash Screen

- Version display, credits, JUCE logo attribution

### 9.2 MIDI Learn

**Spec requires:** Full MIDI learn on all automatable parameters.

**Work:**
- Right-click any knob → "MIDI Learn" → move a CC → mapped
- Store CC→parameterID map in JSON, save with scenes
- Create `Source/Common/MidiLearn.h/cpp`

### 9.3 CPU Metering

- Add a small CPU % readout in the TopBar (using `juce::AudioProcessor::cpuUsage`)

### 9.4 Undo/Redo for Pattern Editing

- Store pattern snapshots before mutations
- Ctrl+Z / Ctrl+Y keybindings

---

## Execution Order (Dependency-Aware)

```
Phase 1 ─── Foundation & Thread Safety
  │          (1.1 APVTS mixer, 1.2 peak meters, 1.3 dead code, 1.4 latency)
  │
Phase 2 ─── Wire Stub Parameters
  │          (2.1 Jupiter-8 ×10, 2.2 Moog ×7, 2.3 JX-3P ×9)
  │          Can run in parallel with Phase 1
  │
Phase 3 ─── Oversampling
  │          (3.1 DX7 4×, 3.2–3.4 subtractive 2×)
  │          Depends on: Phase 1.4 (latency reporting)
  │
Phase 4 ─── Global Effects Bus
  │          (4.1–4.6 reverb, delay, master bus, send routing, UI)
  │          Depends on: Phase 1.1 (APVTS mixer with sends)
  │
Phase 5 ─── Preset Library & Browser
  │          (5.1 expand to 117+ presets, 5.2 browser UI)
  │          Depends on: Phase 2 (all params wired = presets can use them)
  │
Phase 6 ─── Sequencer & Arrangement
  │          (6.1 copy/paste, 6.2 16-scene UI, 6.3 timeline, 6.4 swing)
  │          Independent — can run in parallel with Phase 3–5
  │
Phase 7 ─── UI Polish & Performance
  │          (7.1 keyboard, 7.2 arpeggiator, 7.3 MIDI perf, 7.4–7.5)
  │          Depends on: Phase 2 (params wired for perf features)
  │
Phase 8 ─── Stability & Code Quality
  │          (8.1 mod matrix, 8.2 smoothing audit, 8.3 edge cases, 8.4 tests)
  │          Run after Phases 2–4 (audit what was just built)
  │
Phase 9 ─── Final Polish
             (9.1 about, 9.2 MIDI learn, 9.3 CPU meter, 9.4 undo/redo)
             Last — requires all other phases complete
```

---

## Summary: What Gets Us to 10

| Criteria | Key Moves |
|----------|-----------|
| **Sound Quality → 10** | Wire all 26 stub params + 4× DX7 oversampling + 2× subtractive oversampling |
| **Feature Completeness → 10** | All params functional, effects bus, arpeggiator, MIDI learn, mod matrix verified |
| **User Interface → 10** | Preset browser, 16-scene bar, visual timeline, tooltips, resizable window |
| **Workflow / Jamming → 10** | Reverb + delay make jams sound finished; arpeggiator for live play; MIDI learn for hardware control |
| **Sequencer / Arrangement → 10** | Pattern copy/paste/clear, chain visualisation, per-module swing, arrangement view |
| **Mixer / Effects → 10** | APVTS-backed mixer, reverb send, delay send, master EQ + comp + limiter, per-channel sends |
| **Preset Library → 10** | 117+ factory presets, DX7 ROM voices, browser with categories, save/rename/delete |
| **Stability / Polish → 10** | Thread safety fixed, dead code removed, edge cases hardened, smoothing audit, unit tests |
| **Value & Potential → 10** | All the above transforms this from "promising" to "complete creative workstation" |

---

## Estimated Scope

| Phase | New Files | Modified Files | New APVTS Params | New DSP Code |
|-------|:---------:|:--------------:|:----------------:|:------------:|
| 1     | 0         | ~8             | 21               | Minimal      |
| 2     | 0         | ~12            | 0 (already declared) | Heavy     |
| 3     | 0         | ~8             | 0                | Medium       |
| 4     | 3–4       | ~6             | ~20              | Heavy        |
| 5     | 1–2       | ~3             | 0                | None (UI)    |
| 6     | 1         | ~4             | ~4               | Light        |
| 7     | 1–2       | ~8             | ~6               | Medium       |
| 8     | 1 (tests) | ~10            | 0                | Light        |
| 9     | 2         | ~4             | ~6               | Light        |
| **Total** | **~10** | **~60**       | **~57**          |              |
