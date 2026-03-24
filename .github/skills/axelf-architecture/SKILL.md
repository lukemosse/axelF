---
name: axelf-architecture
description: "AxelF synthesizer workstation architecture and module layout. Use when: deciding where new code belongs, understanding module relationships, working with the mixer/MIDI routing layer, adding output bus routing, creating or modifying preset/scene management, or understanding the overall signal flow. Covers the five instrument modules, their interconnection, and the shared infrastructure."
---

# AxelF Architecture — Project Map

## System Overview

AxelF is a JUCE C++ synthesizer workstation that emulates the five instruments used to record *Axel F* (Harold Faltermeyer, 1984). It runs as a standalone app or VST3/AU plugin.

```
┌──────────────────────────────────────────────────────────────┐
│                     PluginProcessor                          │
│  ┌─────────────────────────────────────────────────────┐     │
│  │                   MidiRouter                        │     │
│  │   Routes MIDI by channel to modules (1-16 or Omni)  │     │
│  └─────┬───────┬───────┬───────┬───────┬───────────────┘     │
│        │       │       │       │       │                     │
│  ┌─────▼──┐┌───▼───┐┌──▼──┐┌──▼──┐┌───▼────┐               │
│  │Jupiter8││Moog15 ││JX3P ││ DX7 ││LinnDrum│               │
│  │Processor││Process.││Proc.││Proc.││Processor│              │
│  └─────┬──┘└───┬───┘└──┬──┘└──┬──┘└───┬────┘               │
│        │       │       │       │       │                     │
│  ┌─────▼───────▼───────▼───────▼───────▼───────────────┐     │
│  │                  MasterMixer                        │     │
│  │  Per-module: fader, pan, mute, solo, aux sends      │     │
│  │  Master output: stereo sum → host DAW               │     │
│  └─────────────────────┬───────────────────────────────┘     │
│                        │                                     │
│  ┌─────────────────────▼───────────────────────────────┐     │
│  │              ModulationMatrix (16 slots)            │     │
│  │  Sources: LFO, ENV, MIDI CC, velocity, aftertouch   │     │
│  │  Targets: any exposed parameter across all modules   │     │
│  └─────────────────────────────────────────────────────┘     │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐     │
│  │              PresetManager                          │     │
│  │  Per-module presets (128 slots each)                 │     │
│  │  Global "Scene" = snapshot of all 5 modules          │     │
│  │  JSON format, factory + user banks                   │     │
│  └─────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────┘
```

## The Five Modules

| # | Module | Class Name | Prefix | Role in Track | Voice Count | Synth Type |
|---|---|---|---|---|---|---|
| 1 | Roland Jupiter-8 | `Jupiter8Processor` | `jup8_` | Iconic saw lead melody | 8 poly (unison collapses to mono) | Subtractive (2 DCO) |
| 2 | Moog Modular 15 | `Moog15Processor` | `moog_` | Deep sub-bass lines | Mono | Subtractive (3 VCO) |
| 3 | Roland JX-3P | `JX3PProcessor` | `jx3p_` | Punchy chord stabs | 6 poly | Subtractive (2 DCO) + chorus |
| 4 | Yamaha DX7 | `DX7Processor` | `dx7_` | Bright marimba/bell | 16 poly | FM (6 operators, 32 algorithms) |
| 5 | LinnDrum | `LinnDrumProcessor` | `linn_` | All percussion | 15 simultaneous | Sample-based + step sequencer |

## Module Relationships

### What's Shared

- **MidiRouter** — single instance in `PluginProcessor`; dispatches MIDI events by channel.
- **MasterMixer** — sums module stereo outputs with per-channel fader/pan/mute/solo.
- **ModulationMatrix** — 16 slots; cross-module modulation possible (e.g., LinnDrum trigger → Jupiter-8 filter).
- **PresetManager** — handles per-module and global scene load/save.
- **Common DSP classes** — `ADSREnvelope`, `LFOGenerator`, `IR3109Filter`, `LadderFilter`, `Oversampler`.
- **Transport/Clock** — host BPM sync shared across LinnDrum sequencer and module arpeggiators.

### What's Independent

- Each module owns its own `AudioProcessorValueTreeState` (APVTS).
- Each module has its own voice allocator and note management.
- Each module has a dedicated editor/UI component.
- Module process blocks run sequentially (not in parallel) within `PluginProcessor::processBlock`.

## Signal Flow

```
MIDI In → MidiRouter → [per-module MIDI buffer]
                           │
         ┌─────────────────┼─────────────────┐
         ▼                 ▼                  ▼
    Jupiter8          Moog15 ...          LinnDrum
    processBlock      processBlock        processBlock
    (stereo out)      (stereo out)        (stereo out)
         │                 │                  │
         └────────┬────────┴──────────────────┘
                  ▼
             MasterMixer
          ┌────────────┐
          │ Per-module: │
          │  Level      │
          │  Pan L/R    │
          │  Mute/Solo  │
          │  Aux 1/2    │
          └──────┬─────┘
                 ▼
          Master Stereo Out → Host DAW
```

## Output Bus Conventions

- **Main stereo out** — always bus 0, indices [0, 1].
- **Aux Send A** — bus 1 (for effects routing in DAW).
- **Aux Send B** — bus 2.
- LinnDrum voices can individually route to Main, Aux A, or Aux B via per-voice `output_bus` parameter.
- Other modules output to Main only (aux sends are post-fader from MasterMixer).

## MIDI Routing Layer

```cpp
// MidiRouter.h
namespace axelf
{
    class MidiRouter
    {
    public:
        // Call from PluginProcessor::processBlock
        void routeMidi(const juce::MidiBuffer& input,
                       juce::MidiBuffer& jupiter8Midi,
                       juce::MidiBuffer& moog15Midi,
                       juce::MidiBuffer& jx3pMidi,
                       juce::MidiBuffer& dx7Midi,
                       juce::MidiBuffer& linnDrumMidi);

        void setModuleChannel(int moduleIndex, int channel);  // 0 = omni
        void sendPanic();  // CC123 all-notes-off to all modules

    private:
        std::array<int, 5> moduleChannels = {1, 2, 3, 4, 10};
        // Default: Jup8=1, Moog=2, JX3P=3, DX7=4, LinnDrum=10 (GM drums)
    };
}
```

### MIDI Channel Defaults

| Module | Default Channel | Rationale |
|---|---|---|
| Jupiter-8 | 1 | Lead — primary MIDI channel |
| Moog 15 | 2 | Bass |
| JX-3P | 3 | Chords |
| DX7 | 4 | Melody/bell |
| LinnDrum | 10 | GM drum convention |

## Master Mixer

```cpp
// MasterMixer.h
namespace axelf
{
    class MasterMixer
    {
    public:
        struct ChannelStrip
        {
            float level = 1.0f;    // 0–1
            float pan   = 0.0f;    // -1 (L) to +1 (R)
            bool  mute  = false;
            bool  solo  = false;
            float auxSend1 = 0.0f;
            float auxSend2 = 0.0f;
        };

        void process(const std::array<juce::AudioBuffer<float>, 5>& moduleOutputs,
                     juce::AudioBuffer<float>& mainOut,
                     juce::AudioBuffer<float>& aux1Out,
                     juce::AudioBuffer<float>& aux2Out);

    private:
        std::array<ChannelStrip, 5> strips;
        // Solo logic: if any channel is solo'd, mute all non-solo channels
    };
}
```

## Preset / Scene Management

### Per-Module Presets

- Stored as JSON files: `Resources/Presets/<Module>/<PatchName>.json`
- Each module has 128 slots minimum.
- Factory presets include historically accurate patches from the original recordings.
- The Jupiter-8 ships with "Axel Lead" as the default init patch.

### Global Scenes

A Scene captures the complete state of all five modules plus mixer settings:

```json
{
    "scene_name": "Axel F - Full Arrangement",
    "version": 1,
    "modules": {
        "jupiter8": { /* full APVTS state */ },
        "moog15":   { /* full APVTS state */ },
        "jx3p":     { /* full APVTS state */ },
        "dx7":      { /* full APVTS state */ },
        "linndrum": { /* full APVTS state */ }
    },
    "mixer": {
        "jupiter8": { "level": 0.8, "pan": 0.0, "mute": false },
        "moog15":   { "level": 0.7, "pan": 0.0, "mute": false },
        "jx3p":     { "level": 0.6, "pan": -0.2, "mute": false },
        "dx7":      { "level": 0.5, "pan": 0.2, "mute": false },
        "linndrum": { "level": 0.75, "pan": 0.0, "mute": false }
    },
    "midi_routing": {
        "jupiter8": 1, "moog15": 2, "jx3p": 3, "dx7": 4, "linndrum": 10
    }
}
```

## Where New Code Belongs

| Task | Location |
|---|---|
| New oscillator type / waveform | `Source/Modules/<Module>/<Module>DCO.h` or `VCO.h` |
| New filter model | `Source/Common/DSP/` (shared) or module-specific if unique |
| New modulation source | `Source/Common/ModulationMatrix.h/cpp` |
| New MIDI feature | `Source/Common/MidiRouter.h/cpp` |
| New UI panel | `Source/Modules/<Module>/<Module>Editor.h/cpp` |
| New parameter | `Source/Modules/<Module>/<Module>Params.h` + APVTS layout |
| New LinnDrum sample set | `Resources/Samples/` |
| New factory preset | `Resources/Presets/<Module>/` |
| New DSP utility (envelope, LFO, etc.) | `Source/Common/DSP/` |
| Build configuration | `CMakeLists.txt` (root) |

## PluginProcessor Lifecycle

```cpp
// PluginProcessor.cpp — processBlock
void AxelFProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages)
{
    // 1. Route MIDI to per-module buffers
    midiRouter.routeMidi(midiMessages, jup8Midi, moogMidi, jx3pMidi, dx7Midi, linnMidi);

    // 2. Clear per-module audio buffers
    for (auto& buf : moduleBuffers)
        buf.clear();

    // 3. Process each module into its own buffer
    jupiter8.processBlock(moduleBuffers[0], jup8Midi);
    moog15.processBlock(moduleBuffers[1], moogMidi);
    jx3p.processBlock(moduleBuffers[2], jx3pMidi);
    dx7.processBlock(moduleBuffers[3], dx7Midi);
    linnDrum.processBlock(moduleBuffers[4], linnMidi);

    // 4. Apply modulation matrix
    modMatrix.process();

    // 5. Mix to master output
    masterMixer.process(moduleBuffers, buffer, aux1Buffer, aux2Buffer);
}
```

## Threading Model

- **Audio thread**: `processBlock` — no allocations, no locks, no blocking I/O.
- **Message thread**: UI updates, preset loading, parameter changes via APVTS (lock-free).
- **Background thread**: Sample loading (LinnDrum), preset file I/O.
- Communication between threads uses lock-free mechanisms: `std::atomic`, APVTS, `juce::AbstractFifo`.
