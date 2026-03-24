---
name: dsp-conventions
description: "DSP implementation patterns for the AxelF synthesizer. Use when: writing oscillator code, filter implementations, envelope generators, LFOs, oversampling, FM synthesis operators, sample-accurate parameter smoothing, or any audio processing code. Covers process block structure, SmoothedValue usage, filter base classes, and anti-aliasing strategies."
---

# DSP Conventions — AxelF Workstation

## Process Block Structure

Every DSP module follows this render pattern:

```cpp
void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                     int startSample, int numSamples)
{
    // 1. Update smoothed parameters (once per block or per sample)
    // 2. Process sample-by-sample in a tight loop
    // 3. Write to outputBuffer at correct offset

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Advance smoothed values
        float cutoff = cutoffSmoothed.getNextValue();
        float resonance = resonanceSmoothed.getNextValue();

        // Update filter coefficients only when parameters are still smoothing
        if (cutoffSmoothed.isSmoothing() || resonanceSmoothed.isSmoothing())
            vcf.setParameters(cutoff, resonance);

        // Generate oscillator output
        float osc1 = dco1.processSample();
        float osc2 = dco2.processSample();
        float mixed = osc1 * dco1Level + osc2 * dco2Level + noise * noiseLevel;

        // Filter
        float filtered = vcf.processSample(mixed);

        // Envelope (VCA)
        float envValue = env2.getNextSample();
        float output = filtered * envValue;

        // Write to buffer (stereo)
        outputBuffer.addSample(0, startSample + sample, output);
        outputBuffer.addSample(1, startSample + sample, output);
    }
}
```

### Key Rules

- **Sample-accurate iteration**: Always loop `for (int sample = 0; sample < numSamples; ++sample)`.
- **Use `addSample`**, not `setSample`, when mixing voices into a shared buffer.
- **Offset by `startSample`** — JUCE may call `renderNextBlock` for sub-ranges within a block.
- **Never branch on MIDI inside the sample loop** — MIDI events are pre-dispatched by `juce::Synthesiser` before calling `renderNextBlock`.

## Parameter Smoothing

All audible parameters must be smoothed to avoid zipper noise. Use `juce::SmoothedValue`:

```cpp
// In voice or processor class
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> cutoffSmoothed;
juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> levelSmoothed;

// In prepareToPlay
cutoffSmoothed.reset(sampleRate, 0.02);  // 20ms ramp for filter cutoff
levelSmoothed.reset(sampleRate, 0.005);  // 5ms ramp for level changes

// Before each block — read from APVTS atomic and set target
cutoffSmoothed.setTargetValue(*apvts.getRawParameterValue("jup8_vcf_cutoff"));

// In sample loop — call getNextValue() once per sample
float cutoff = cutoffSmoothed.getNextValue();
```

### Smoothing Times by Parameter Type

| Parameter Type | Smoothing Time | Smoothing Type |
|---|---|---|
| Filter cutoff | 20 ms | Linear |
| Filter resonance | 20 ms | Linear |
| Oscillator level/mix | 5 ms | Multiplicative |
| Pan | 10 ms | Linear |
| LFO rate | 50 ms | Linear |
| Master volume | 10 ms | Multiplicative |
| Envelope times (A/D/R) | No smoothing | Direct (applied at note-on/off) |
| Waveform select | No smoothing | Direct (discrete choice) |
| Pitch/detune | 5 ms | Linear |

### Rules

1. **Never read from `std::atomic<float>*` inside the sample loop** — read once before the loop, pass to `setTargetValue`.
2. **Use multiplicative smoothing for gain/level** — sounds more natural for amplitude changes.
3. **Don't smooth discrete parameters** (waveform, algorithm, voice mode) — switch immediately.
4. **Envelope A/D/S/R values** are read at note-on/off, not smoothed continuously.

## Oversampling Strategy

### When Required

| Module | Oversampling | Reason |
|---|---|---|
| DX7 | **4× minimum** | FM synthesis generates extreme high-frequency content; aliasing is audible |
| Jupiter-8 | **2×** | Pulse-width modulation and cross-mod create harmonics above Nyquist |
| Moog 15 | **2×** | Ladder filter saturation + high resonance produce nonlinear harmonics |
| JX-3P | **2×** | Oscillator sync and cross-mod |
| LinnDrum | **None** | Sample playback only; original 12-bit character is desirable |

### Implementation Pattern

```cpp
// In processor header
juce::dsp::Oversampling<float> oversampler{
    2,                                           // numChannels
    2,                                           // oversampling order (2^2 = 4× for DX7)
    juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR  // quality
};

// In prepareToPlay
oversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
double oversampledRate = sampleRate * oversampler.getOversamplingFactor();
// Pass oversampledRate to oscillators, filters, envelopes

// In processBlock
juce::dsp::AudioBlock<float> block(buffer);
auto oversampledBlock = oversampler.processSamplesUp(block);

// Process at oversampled rate
processAtOversampledRate(oversampledBlock);

// Downsample back
oversampler.processSamplesDown(block);
```

### Rules

1. **All internal DSP uses the oversampled sample rate** — oscillator phase increments, filter coefficients, envelope times.
2. **Don't oversample the LinnDrum** — the 12-bit / 27.5 kHz character is intentional.
3. **Use `filterHalfBandPolyphaseIIR`** for best quality/performance trade-off.
4. **Latency reporting**: call `oversampler.getLatencyInSamples()` and report via `setLatencySamples()`.

## Oscillator Implementation

### Phase Accumulator Pattern

All oscillators use a normalised phase accumulator (0.0 to 1.0):

```cpp
class DCO
{
public:
    float processSample()
    {
        float output = 0.0f;

        switch (waveform)
        {
            case Waveform::Sawtooth:
                output = 2.0f * phase - 1.0f;       // naive; apply PolyBLEP
                output -= polyBLEP(phase, phaseIncrement);
                break;
            case Waveform::Pulse:
                output = (phase < pulseWidth) ? 1.0f : -1.0f;
                output += polyBLEP(phase, phaseIncrement);
                output -= polyBLEP(std::fmod(phase + (1.0f - pulseWidth), 1.0f), phaseIncrement);
                break;
            case Waveform::Square:
                output = (phase < 0.5f) ? 1.0f : -1.0f;
                // apply PolyBLEP at discontinuities
                break;
        }

        // Advance phase
        phase += phaseIncrement;
        if (phase >= 1.0f)
            phase -= 1.0f;

        return output;
    }

    void setFrequency(float freqHz, float sampleRate)
    {
        phaseIncrement = freqHz / sampleRate;
    }

private:
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    float pulseWidth = 0.5f;
    Waveform waveform = Waveform::Sawtooth;
};
```

### Anti-Aliasing

- **PolyBLEP** (Polynomial Band-Limited Step) for sawtooth, pulse, square waveforms.
- **Sine/Triangle** don't need anti-aliasing (no discontinuities).
- **FM operators (DX7)** rely on oversampling (4×) rather than PolyBLEP since FM modulation creates unpredictable partials.

## Filter Base Classes

### IR3109 (Jupiter-8 and JX-3P)

4-pole (24 dB/oct) low-pass, modelled on Roland's IR3109 OTA filter chip.

```cpp
namespace axelf::dsp
{
    class IR3109Filter
    {
    public:
        void prepare(double sampleRate);
        void setParameters(float cutoffHz, float resonance);  // resonance 0–1
        float processSample(float input);
        void reset();

    private:
        double sampleRate = 44100.0;
        float state[4] = {};  // 4 cascaded 1-pole stages
        float cutoff = 1000.0f;
        float reso = 0.0f;
    };
}
```

### Moog Ladder (Moog 15)

Custom ladder filter with input drive and self-oscillation:

```cpp
namespace axelf::dsp
{
    class LadderFilter
    {
    public:
        void prepare(double sampleRate);
        void setParameters(float cutoffHz, float resonance, float drive);
        float processSample(float input);
        void reset();

    private:
        double sampleRate = 44100.0;
        float stage[4] = {};
        float delay[4] = {};
        float cutoff = 1000.0f;
        float reso = 0.0f;
        float drive = 0.0f;

        // Nonlinear saturation applied per stage
        static float tanhApprox(float x);
    };
}
```

### Key Filter Rules

1. **Don't use `juce::dsp::IIR::Filter` for the analog-modelled filters** — the spec requires nonlinear, self-oscillating behaviour.
2. **Use `juce::dsp::LadderFilter` only as a reference baseline**, then replace with custom implementation.
3. **Apply tanh saturation** in each stage of the ladder filter for Moog warmth.
4. **Cutoff coefficient calculation** must use the bilinear transform or a matched analog prototype — never just `cutoff / sampleRate`.
5. **Resonance at max must self-oscillate** (produce a sine tone at the cutoff frequency with zero input).

## Envelope Generator

Shared ADSR with configurable time ranges:

```cpp
namespace axelf::dsp
{
    class ADSREnvelope
    {
    public:
        struct Parameters
        {
            float attack  = 0.005f;  // seconds
            float decay   = 0.4f;
            float sustain = 0.7f;    // level 0–1
            float release = 0.2f;
        };

        void prepare(double sampleRate);
        void setParameters(const Parameters& params);
        void noteOn();
        void noteOff();
        float getNextSample();
        bool isActive() const;
        void reset();

    private:
        enum class State { Idle, Attack, Decay, Sustain, Release };
        State state = State::Idle;
        float currentLevel = 0.0f;
        // Exponential curves for natural feel
        float attackRate = 0.0f, decayRate = 0.0f, releaseRate = 0.0f;
        float sustainLevel = 0.0f;
        double sampleRate = 44100.0;
    };
}
```

### Envelope Rules

1. **Exponential curves** for decay and release (not linear) — `currentLevel *= decayFactor`.
2. **Linear attack** (or optionally switchable) — most analog synths use linear-ish attack.
3. **Set parameters at note-on**, not per-sample (unless the envelope is being modulated, which is uncommon).
4. **`isActive()` returns false only in Idle state** — the voice manager uses this to know when to steal/free voices.

## FM Synthesis (DX7-Specific)

### Operator Processing

```cpp
class DX7Operator
{
public:
    float processSample(float modulationInput)
    {
        float phase = basePhase + modulationInput;  // FM: add modulator output to phase
        float output = std::sin(phase * juce::MathConstants<float>::twoPi) * level;

        // Advance base phase
        basePhase += phaseIncrement;
        if (basePhase >= 1.0f)
            basePhase -= 1.0f;

        return output;
    }

private:
    float basePhase = 0.0f;
    float phaseIncrement = 0.0f;  // frequency / sampleRate
    float level = 0.0f;           // 0–1, from operator EG
};
```

### Feedback

Operator 6 (or whichever the algorithm designates) can self-modulate:

```cpp
// Single-sample delay feedback
float feedbackSample = previousOutput * feedbackAmount;
float output = op.processSample(feedbackSample);
previousOutput = output;
```

### DX7 Rules

1. **Feedback is a single-sample delay** — store previous output, feed back on next sample.
2. **Operator EGs use DX7's 4-rate/4-level model**, not standard ADSR.
3. **Output levels use exponential scaling** matching the DX7's characteristic curve.
4. **Always oversample 4×** — FM with high modulation indices produces extreme aliasing.
5. **Phase modulation is additive** — modulator output is *added* to carrier phase, not multiplied with frequency.

## LinnDrum Sample Playback

```cpp
class LinnDrumVoice
{
public:
    void trigger(float velocity)
    {
        playbackPosition = 0.0;
        isPlaying = true;
        currentVelocity = velocity;
    }

    float processSample()
    {
        if (!isPlaying) return 0.0f;

        // Linear interpolation for pitch-shifted playback
        int pos0 = static_cast<int>(playbackPosition);
        int pos1 = pos0 + 1;
        float frac = static_cast<float>(playbackPosition - pos0);

        if (pos1 >= sampleLength)
        {
            isPlaying = false;
            return 0.0f;
        }

        float s0 = sampleData[pos0];
        float s1 = sampleData[pos1];
        float output = s0 + frac * (s1 - s0);

        // Apply velocity and decay envelope
        output *= currentVelocity * decayEnvelope.getNextSample();

        playbackPosition += playbackRate;  // 1.0 = original pitch
        return output;
    }

private:
    const float* sampleData = nullptr;
    int sampleLength = 0;
    double playbackPosition = 0.0;
    double playbackRate = 1.0;  // modified by tune parameter
    float currentVelocity = 1.0f;
    bool isPlaying = false;
    axelf::dsp::ADSREnvelope decayEnvelope;
};
```

### LinnDrum Rules

1. **No oversampling** — preserve the original 12-bit / 27.5 kHz character.
2. **Mute groups**: voices in the same group call `stop()` on siblings when triggered (e.g., open/closed hi-hat).
3. **No voice stealing** — all 15 voices play simultaneously.
4. **Linear interpolation** for tune-shifted playback (not sinc — keeps the gritty character).
