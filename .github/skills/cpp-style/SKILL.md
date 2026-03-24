---
name: cpp-style
description: "C++ code style and conventions for the AxelF project. Use when: writing new C++ source files, headers, classes, or functions for the AxelF workstation. Covers naming conventions, header/implementation structure, auto usage, const correctness, comment style, and include ordering."
---

# C++ Style — AxelF Workstation

## Naming Conventions

### Classes & Structs

- **PascalCase**: `Jupiter8Processor`, `LadderFilter`, `ADSREnvelope`, `MasterMixer`
- Module-specific classes prefixed with module name: `Jupiter8Voice`, `DX7Operator`, `LinnDrumSequencer`
- Shared DSP classes use descriptive names without module prefix: `LadderFilter`, `IR3109Filter`, `LFOGenerator`

### Functions & Methods

- **camelCase**: `processBlock`, `getNextSample`, `setParameters`, `noteOn`, `noteOff`
- Getters: `getLevel()`, `getCutoff()`, `isActive()`, `isPlaying()`
- Setters: `setLevel()`, `setCutoff()`, `setFrequency()`
- Process methods: `processSample()`, `processBlock()`, `renderNextBlock()`

### Variables

- **camelCase** for locals and members: `sampleRate`, `phaseIncrement`, `cutoffSmoothed`
- **No prefixes** — no `m_`, no `_` prefix for members
- Private members are simply `camelCase` (disambiguated by `this->` only when necessary, which is rare)
- Constants: `constexpr float kMaxFrequency = 20000.0f;` (k-prefix + PascalCase) or `static constexpr`

### Parameters & APVTS IDs

- **snake_case** with module prefix: `jup8_dco1_waveform`, `moog_vcf_cutoff`, `dx7_op1_level`
- This is the only place snake_case is used — it's a JUCE/DAW convention for parameter IDs.

### Enums

```cpp
enum class Waveform { Sawtooth, Pulse, Square, Triangle, Sine, Noise };
enum class VoiceMode { Poly, Unison, SoloLastNote, SoloLowNote, SoloHighNote };
enum class FilterType { LowPass, HighPass };
```

- **enum class** always (never unscoped enums).
- PascalCase for both the enum name and its values.

### Namespaces

```cpp
namespace axelf { }                // Top-level
namespace axelf::jupiter8 { }     // Module-specific
namespace axelf::dsp { }           // Shared DSP utilities
```

- **lowercase** with `::` nesting.
- Don't use `using namespace` in headers.

### Files

- **PascalCase** matching the primary class: `Jupiter8Processor.h`, `LadderFilter.cpp`, `ADSREnvelope.h`
- Params headers: `<Module>Params.h` (e.g., `Jupiter8Params.h`)
- One primary class per file pair (`.h` / `.cpp`)

## Header vs Implementation

### Headers (`.h`)

```cpp
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf::jupiter8
{

class Jupiter8Voice : public juce::SynthesiserVoice
{
public:
    // Public interface — declarations only (no inline bodies for non-trivial methods)
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         int startSample, int numSamples) override;

private:
    Jupiter8DCO dco1;
    Jupiter8DCO dco2;
    axelf::dsp::IR3109Filter vcf;
    axelf::dsp::ADSREnvelope env1, env2;
};

} // namespace axelf::jupiter8
```

### Rules

1. **`#pragma once`** — always (no include guards).
2. **Minimal includes in headers** — forward-declare where possible, include in `.cpp`.
3. **No function bodies in headers** except trivial getters/setters and `constexpr` functions.
4. **Closing brace comment** for namespace: `} // namespace axelf::jupiter8`

### Implementation (`.cpp`)

```cpp
#include "Jupiter8Voice.h"
#include "Jupiter8DCO.h"
#include "../Common/DSP/IR3109Filter.h"
#include "../Common/DSP/ADSREnvelope.h"

namespace axelf::jupiter8
{

bool Jupiter8Voice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<Jupiter8Sound*>(sound) != nullptr;
}

void Jupiter8Voice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                     int startSample, int numSamples)
{
    // implementation here
}

} // namespace axelf::jupiter8
```

## `auto` Usage

**Use `auto` sparingly and intentionally:**

```cpp
// YES — type is obvious from RHS
auto slider = std::make_unique<juce::Slider>();
auto* param = apvts.getRawParameterValue("jup8_vcf_cutoff");

// YES — iterators
for (auto it = midiBuffer.begin(); it != midiBuffer.end(); ++it)

// YES — range-based for with complex container types
for (const auto& [key, value] : parameterMap)

// NO — type not obvious; spell it out
float cutoff = calculateCutoff(frequency, sampleRate);  // not: auto cutoff = ...
int voiceCount = synth.getNumVoices();                   // not: auto voiceCount = ...
juce::AudioBuffer<float> tempBuffer(2, numSamples);      // not: auto tempBuffer = ...
```

**Rule of thumb**: Use `auto` when the type is visible on the same line (factory functions, `make_unique`, casts). Spell out the type when it aids readability.

## Const Correctness

```cpp
// Const ref parameters for non-trivial types
void process(const juce::AudioBuffer<float>& input);

// Const methods that don't modify state
bool isActive() const { return state != State::Idle; }
float getLevel() const { return currentLevel; }
int getMidiChannel() const { return midiChannel; }

// Const local variables when value won't change
const float frequency = juce::MidiMessage::getMidiNoteInHertz(noteNumber);
const int numSamples = buffer.getNumSamples();
```

**Mark everything `const` that can be `const`** — parameters, locals, methods, pointers.

## Braces & Formatting

```cpp
// Allman-style braces (opening brace on new line for functions/classes)
class Jupiter8Processor : public axelf::ModuleProcessor
{
public:
    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        // ...
    }

    // Short single-statement if: still use braces
    if (phase >= 1.0f)
    {
        phase -= 1.0f;
    }
};
```

- **4-space indentation** (no tabs).
- **Always use braces** for `if`, `for`, `while` — even single-line bodies.
- **Allman-style** (opening brace on its own line) for class and function definitions.
- **K&R-style acceptable** for control flow (`if`, `for`) if the team prefers — just be consistent.

## Include Ordering

```cpp
// 1. Corresponding header (for .cpp files)
#include "Jupiter8Voice.h"

// 2. Project headers (relative paths)
#include "Jupiter8DCO.h"
#include "../Common/DSP/IR3109Filter.h"

// 3. JUCE module headers
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// 4. Standard library
#include <cmath>
#include <array>
#include <memory>
```

Blank line between each group.

## Comments

```cpp
// Single-line comments for brief explanations
float output = 2.0f * phase - 1.0f;  // naive sawtooth, PolyBLEP applied below

// Block comments for section headers or design rationale
// ─────────────────────────────────────────────
// Moog Ladder Filter — 4-pole with nonlinear saturation
// Based on Huovilainen's improved model (2004)
// Each stage applies tanh() for analog warmth
// ─────────────────────────────────────────────
```

- **No Doxygen/Javadoc** unless writing public API documentation — keep comments lightweight.
- **Comment *why*, not *what*** — the code should be self-explanatory for *what*.
- **No commented-out code** — use version control instead.
- **No TODO without a plan** — if you write `// TODO:`, include what needs doing and why.

## Smart Pointers & Ownership

```cpp
// Use unique_ptr for owned resources
std::unique_ptr<juce::AudioFormatReader> reader;

// Raw pointers for non-owning references (JUCE convention)
juce::AudioProcessorValueTreeState& apvts;  // reference to parent-owned APVTS
std::atomic<float>* cutoffParam = nullptr;   // APVTS-owned atomic; we just observe

// No raw new/delete — always wrap in smart pointers or JUCE containers
auto voice = std::make_unique<Jupiter8Voice>(apvts);
synth.addVoice(voice.release());  // Synthesiser takes ownership
```

## Error Handling

- **`jassert()`** for debug-only assertions (DSP invariants, parameter bounds).
- **No exceptions in audio code** — JUCE audio path is exception-free.
- **Return early** on invalid state rather than nesting deeply.
- **`DBG()`** for debug logging (stripped in release builds).
