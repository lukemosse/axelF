#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "JX3PDCO.h"
#include "JX3PChorus.h"
#include "../../Common/DSP/IR3109Filter.h"
#include "../../Common/DSP/ADSREnvelope.h"
#include "../../Common/DSP/LFOGenerator.h"

namespace axelf::jx3p
{

class JX3PSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class JX3PVoice : public juce::SynthesiserVoice
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

    void setParameters(int dco1Wave, int dco2Wave, float dco2Detune,
                       float cutoff, float reso, float envDepth,
                       float env1A, float env1D, float env1S, float env1R,
                       float env2A, float env2D, float env2S, float env2R,
                       float lfoRate, float lfoDepth, int lfoWaveform,
                       float dco1Range, float mixDco1, float mixDco2,
                       float vcfLfoAmount, int vcfKeyTrack,
                       float lfoDelay, float crossModDepth, int dcoSync,
                       float pulseWidth, float dco2PW,
                       float noiseLevel, float portamento);

private:
    JX3PDCO dco1, dco2;
    axelf::dsp::IR3109Filter vcf;
    axelf::dsp::ADSREnvelope env1; // VCF envelope
    axelf::dsp::ADSREnvelope env2; // VCA envelope
    axelf::dsp::LFOGenerator lfo;

    float velocity = 0.0f;
    float noteFrequency = 440.0f;
    int   midiNoteNum = 60;
    float dco2DetuneCents = 0.0f;
    float vcfBaseCutoff = 5000.0f;
    float vcfResonance = 0.2f;
    float vcfEnvDepth = 0.4f;
    float lfoDepthAmount = 0.0f;
    float pitchBendSemitones = 0.0f;
    float modWheelValue = 0.0f;       // CC1 mod wheel 0–1

    // Phase 2 additions
    float mixDco1Level = 0.5f;
    float mixDco2Level = 0.5f;
    float vcfLfoAmount = 0.0f;
    int   vcfKeyTrackMode = 0;
    float lfoDelayTime = 0.0f;
    float lfoDelayRamp = 1.0f;
    double lfoDelaySamples = 0.0;
    double lfoDelayTarget = 0.0;
    float crossMod = 0.0f;
    bool  syncEnabled = false;
    float prevDco2Out = 0.0f;
    float noiseLevel = 0.0f;
    float portaRate = 0.0f;         // portamento time (seconds)
    float portaFreq = 440.0f;       // current gliding freq
    uint32_t noiseState = 98765;
    int   antiClickCounter = -1;   // ≥0 = fade-in active (counts up to 32)

    // Parameter smoothing (Phase 8)
    juce::SmoothedValue<float> cutoffSmoothed;
    juce::SmoothedValue<float> resoSmoothed;
    juce::SmoothedValue<float> mix1Smoothed;
    juce::SmoothedValue<float> mix2Smoothed;
};

} // namespace axelf::jx3p
