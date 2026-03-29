#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Jupiter8DCO.h"
#include "../../Common/DSP/IR3109Filter.h"
#include "../../Common/DSP/ADSREnvelope.h"
#include "../../Common/DSP/LFOGenerator.h"

namespace axelf::jupiter8
{

class Jupiter8Sound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class Jupiter8Voice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         int startSample, int numSamples) override;

    void setVoiceIndex(int idx) { voiceIndex = idx; }
    void setUnisonDetune(float cents) { unisonDetuneCents = cents; }

    void setParameters(float dco1Wave, float dco1Range,
                       float dco2Wave, float dco2Range, float dco2Detune,
                       float mixDco1, float mixDco2,
                       float vcfCutoff, float vcfReso, float vcfEnvDepth,
                       float env1A, float env1D, float env1S, float env1R,
                       float env2A, float env2D, float env2S, float env2R,
                       float lfoRate, float lfoDepth, int lfoWaveform,
                       float pulseWidth, float subLevel, float noiseLevel,
                       int vcfKeyTrack, float vcfLfoAmount, float lfoDelay,
                       float portamento, float crossModDepth, int dcoSync,
                       int voiceMode, int hpfMode);

private:
    Jupiter8DCO dco1;
    Jupiter8DCO dco2;
    axelf::dsp::IR3109Filter vcf;
    axelf::dsp::ADSREnvelope env1;  // filter envelope
    axelf::dsp::ADSREnvelope env2;  // amplitude envelope
    axelf::dsp::LFOGenerator lfo;

    float velocity = 0.0f;
    float noteFrequency = 440.0f;
    int   midiNoteNum = 60;
    float dco2DetuneCents = 6.0f;
    int dco2Waveform = 0;
    float mixDco1Level = 0.7f;
    float mixDco2Level = 0.5f;
    float vcfEnvDepth = 0.3f;
    float vcfBaseCutoff = 13000.0f;
    float vcfResonance = 0.15f;
    float lfoDepthAmount = 0.0f;
    float pitchBendSemitones = 0.0f;
    float modWheelValue = 0.0f;       // CC1 mod wheel 0–1
    uint32_t noiseState = 12345;

    // Phase 2 additions
    float subLevel = 0.0f;
    float noiseLevel = 0.0f;
    int   vcfKeyTrackMode = 0;      // 0=Off, 1=50%, 2=100%
    float vcfLfoAmount = 0.0f;      // separate VCF LFO depth
    float lfoDelayTime = 0.0f;      // seconds
    float lfoDelayRamp = 1.0f;      // 0→1 ramp during delay
    double lfoDelaySamples = 0.0;   // counter
    double lfoDelayTarget = 0.0;    // target sample count
    float portaRate = 0.0f;         // portamento time (seconds)
    float portaFreq = 440.0f;       // current gliding freq
    float crossMod = 0.0f;          // cross-mod depth
    bool  syncEnabled = false;      // hard sync DCO-2→DCO-1
    float prevDco2Out = 0.0f;       // for cross-mod (1-sample delay)
    float subPhase = 0.0f;          // sub-oscillator phase accumulator (per-voice)
    uint32_t noiseState2 = 67890;   // separate PRNG for noise mixer path
    int   hpfMode = 0;             // 0=Off, 1=250Hz, 2=500Hz, 3=1kHz
    float hpfPrevIn = 0.0f;        // 1-pole HPF state
    float hpfPrevOut = 0.0f;
    int   voiceIndex = 0;          // 0–7 for unison spread
    float unisonDetuneCents = 0.0f; // per-voice unison detune
    int   antiClickCounter = -1;   // ≥0 = fade-in active (counts up to 32)

    // Parameter smoothing (Phase 8)
    juce::SmoothedValue<float> cutoffSmoothed;
    juce::SmoothedValue<float> resoSmoothed;
    juce::SmoothedValue<float> mix1Smoothed;
    juce::SmoothedValue<float> mix2Smoothed;
};

} // namespace axelf::jupiter8
