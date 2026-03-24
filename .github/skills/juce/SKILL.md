---
name: juce
description: "JUCE C++ audio plugin development for the AxelF synthesizer workstation. Use when: writing or editing AudioProcessor subclasses, APVTS parameter trees, DSP process blocks, voice/oscillator/filter code, JUCE GUI components, or any code under Source/Modules/. Covers project layout, module conventions, CMake integration, and JUCE-idiomatic patterns."
---

# JUCE Development — AxelF Workstation

## Project Structure

```
AxelF/
├── CMakeLists.txt                    # Root CMake — juce_add_plugin, module registration
├── Source/
│   ├── PluginProcessor.h/cpp         # Top-level AudioProcessor — hosts all 5 modules
│   ├── PluginEditor.h/cpp            # Top-level editor — tab UI switching between modules
│   ├── Common/
│   │   ├── MidiRouter.h/cpp          # MIDI channel routing to modules
│   │   ├── MasterMixer.h/cpp         # Per-module fader/pan/mute/solo → stereo sum
│   │   ├── ModulationMatrix.h/cpp    # 16-slot global mod matrix
│   │   ├── PresetManager.h/cpp       # JSON preset load/save, factory presets
│   │   └── DSP/
│   │       ├── LadderFilter.h/cpp    # Shared Moog-style 4-pole ladder
│   │       ├── IR3109Filter.h/cpp    # Roland IR3109 filter model
│   │       ├── ADSREnvelope.h/cpp    # Shared ADSR with configurable ranges
│   │       ├── LFOGenerator.h/cpp    # Multi-waveform LFO
│   │       └── Oversampler.h/cpp     # Wrapper around juce::dsp::Oversampling
│   └── Modules/
│       ├── Jupiter8/                 # Module 1 — lead synth
│       │   ├── Jupiter8Processor.h/cpp
│       │   ├── Jupiter8Editor.h/cpp
│       │   ├── Jupiter8Voice.h/cpp
│       │   ├── Jupiter8DCO.h/cpp
│       │   └── Jupiter8Params.h      # APVTS parameter layout for this module
│       ├── Moog15/                   # Module 2 — bass
│       │   ├── Moog15Processor.h/cpp
│       │   ├── Moog15Editor.h/cpp
│       │   ├── Moog15Voice.h/cpp
│       │   ├── Moog15VCO.h/cpp
│       │   └── Moog15Params.h
│       ├── JX3P/                     # Module 3 — chord stabs
│       │   ├── JX3PProcessor.h/cpp
│       │   ├── JX3PEditor.h/cpp
│       │   ├── JX3PVoice.h/cpp
│       │   ├── JX3PDCO.h/cpp
│       │   ├── JX3PChorus.h/cpp      # Built-in stereo chorus
│       │   └── JX3PParams.h
│       ├── DX7/                      # Module 4 — FM marimba/bell
│       │   ├── DX7Processor.h/cpp
│       │   ├── DX7Editor.h/cpp
│       │   ├── DX7Voice.h/cpp
│       │   ├── DX7Operator.h/cpp     # Single FM operator (×6 per voice)
│       │   ├── DX7Algorithm.h/cpp    # 32-algorithm carrier/modulator topology
│       │   └── DX7Params.h
│       └── LinnDrum/                 # Module 5 — drum machine
│           ├── LinnDrumProcessor.h/cpp
│           ├── LinnDrumEditor.h/cpp
│           ├── LinnDrumVoice.h/cpp   # Per-sample voice with tune/decay/pan
│           ├── LinnDrumSequencer.h/cpp
│           └── LinnDrumParams.h
└── Resources/
    ├── Presets/                       # Factory JSON presets per module
    └── Samples/                       # LinnDrum 12-bit WAV samples
```

## APVTS Parameter Conventions

### Parameter ID Format

All parameter IDs follow: `<module>_<section>_<param>`

```
Module prefixes:
  jup8_   — Jupiter-8
  moog_   — Moog Modular 15
  jx3p_   — JX-3P
  dx7_    — DX7
  linn_   — LinnDrum
  mix_    — Master mixer
  global_ — Global settings (tempo, mod matrix)
```

### Examples

```
jup8_dco1_waveform      jup8_dco2_detune       jup8_vcf_cutoff
jup8_vcf_resonance      jup8_env1_attack       jup8_env1_decay
jup8_lfo_rate           jup8_lfo_waveform      jup8_voice_mode
moog_vco1_waveform      moog_vco1_range        moog_vcf_cutoff
moog_vcf_drive          moog_env1_attack       moog_glide_time
jx3p_dco1_waveform      jx3p_chorus_mode       jx3p_chorus_depth
dx7_op1_level           dx7_op1_ratio          dx7_op1_eg_r1
dx7_algorithm           dx7_feedback           dx7_lfo_speed
linn_kick_level         linn_kick_tune         linn_kick_decay
linn_kick_pan           linn_swing             linn_master_tune
mix_jup8_level          mix_jup8_pan           mix_jup8_mute
```

### APVTS Layout Pattern

Each module defines its parameters in a dedicated `*Params.h` file that returns a `juce::AudioProcessorValueTreeState::ParameterLayout`:

```cpp
// Jupiter8Params.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf::jupiter8
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
```

```cpp
// Jupiter8Params.cpp
juce::AudioProcessorValueTreeState::ParameterLayout
axelf::jupiter8::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Oscillators
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"jup8_dco1_waveform", 1},
        "DCO-1 Waveform",
        juce::StringArray{"Sawtooth", "Pulse", "Sub-Osc"},
        0));  // default: Sawtooth

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"jup8_dco2_detune", 1},
        "DCO-2 Detune",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f),
        6.0f));  // default: +6 cents (Axel F lead patch)

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"jup8_vcf_cutoff", 1},
        "VCF Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f),  // skew for log
        13000.0f));

    // ... remaining parameters follow same pattern
    return { params.begin(), params.end() };
}
```

### Key APVTS Rules

1. **Always use `juce::ParameterID{id, versionHint}`** — the version hint (integer) enables host-side parameter versioning.
2. **Use `NormalisableRange` skew factor** for frequency parameters (cutoff, rate): `skew = 0.3f` gives logarithmic feel.
3. **Choice parameters** for waveform selectors, voice modes, algorithm numbers.
4. **Bool parameters** for switches (sync on/off, mute, solo).
5. **Float parameters** for continuous knobs (level, cutoff, resonance, envelope times).
6. **Never create raw `std::atomic<float>*` pointers** — always go through APVTS `getRawParameterValue()` or `getParameter()`.

## Module Processor Pattern

Each module is a self-contained processor inheriting from a shared base:

```cpp
// Common/ModuleProcessor.h
namespace axelf
{
    class ModuleProcessor
    {
    public:
        virtual ~ModuleProcessor() = default;

        virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
        virtual void processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& midiMessages) = 0;
        virtual void releaseResources() = 0;

        void setMidiChannel(int channel) { midiChannel = channel; }
        int getMidiChannel() const { return midiChannel; }

    protected:
        double currentSampleRate = 44100.0;
        int currentBlockSize = 512;
        int midiChannel = 0;  // 0 = omni
    };
}
```

Each concrete module (e.g., `Jupiter8Processor`) owns its APVTS and voice pool:

```cpp
class Jupiter8Processor : public axelf::ModuleProcessor
{
public:
    Jupiter8Processor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;
    void releaseResources() override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::Synthesiser synth;   // JUCE voice allocator
    // voices added in constructor: synth.addVoice(new Jupiter8Voice(...))
    // sound added: synth.addSound(new Jupiter8Sound())
};
```

## Voice Architecture

Polyphonic modules (Jupiter-8, JX-3P, DX7) use `juce::SynthesiserVoice`:

```cpp
class Jupiter8Voice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override;

private:
    Jupiter8DCO dco1, dco2;
    axelf::dsp::IR3109Filter vcf;
    axelf::dsp::ADSREnvelope env1, env2;  // env1 → filter, env2 → VCA
    axelf::dsp::LFOGenerator lfo;
};
```

Monophonic modules (Moog 15) handle note priority internally without `juce::Synthesiser`.

LinnDrum uses a non-stealing voice pool — all 15 drum voices can sound simultaneously.

## GUI Pattern

Module editors inherit `juce::Component` and receive a reference to the module's APVTS:

```cpp
class Jupiter8Editor : public juce::Component
{
public:
    explicit Jupiter8Editor(juce::AudioProcessorValueTreeState& apvts);

private:
    // Use APVTS attachments for all controls
    juce::Slider cutoffSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    // ...
};
```

**Always use APVTS attachments** (`SliderAttachment`, `ComboBoxAttachment`, `ButtonAttachment`) — never manually wire `Slider::onValueChange` to parameter updates.

## JUCE Module Dependencies

```
juce_audio_processors    — AudioProcessor, APVTS, plugin hosting
juce_dsp                 — Oversampling, IIR/FIR, LadderFilter (baseline), waveshaper
juce_audio_basics        — AudioBuffer, MidiBuffer, MidiMessage
juce_audio_formats       — AudioFormatManager for LinnDrum sample loading
juce_graphics            — LookAndFeel, Path drawing for UI panels
juce_gui_basics          — Component, Slider, ComboBox
juce_audio_plugin_client — VST3/AU/Standalone export
```

## Common Mistakes to Avoid

1. **Don't call `getParameter()` in the audio thread** — cache `std::atomic<float>*` from `getRawParameterValue()` in `prepareToPlay` or constructor.
2. **Don't allocate memory in `processBlock`** — no `new`, no `std::vector::push_back`, no string operations.
3. **Don't use `juce::dsp::LadderFilter` as-is for the Moog** — it's a starting point only; the spec calls for input drive/saturation and vintage resonance behaviour that requires a custom implementation.
4. **Don't forget `NormalisableRange` skew** for frequency knobs — linear frequency sliders are unusable.
5. **Don't mix parameter naming conventions** — always `<module>_<section>_<param>`, lowercase, underscores.
6. **Don't use `juce::Synthesiser` for LinnDrum** — it steals voices; LinnDrum needs all 15 voices simultaneous with mute-group logic.
