#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Moog15VCO.h"
#include "../../Common/DSP/LadderFilter.h"
#include "../../Common/DSP/ADSREnvelope.h"
#include "../../Common/DSP/LFOGenerator.h"

namespace axelf::moog15
{

class Moog15Sound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class Moog15Voice : public juce::SynthesiserVoice
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

    void setParameters(int vco1Wave, float vco1Range,
                       int vco2Wave, float vco2Detune,
                       int vco3Wave, float vco3Detune,
                       float mixV1, float mixV2, float mixV3,
                       float cutoff, float reso, float drive, float envDepth,
                       float e1A, float e1D, float e1S, float e1R,
                       float e2A, float e2D, float e2S, float e2R,
                       float glideTime,
                       float lfoRate, float lfoDepth, int lfoWaveform,
                       float vco1PW, float vco2PW, float vco3PW,
                       float noiseLevel, int vcfKeyTrack,
                       float vcfLfoAmount, float lfoPitchAmount);

private:
    Moog15VCO vco1, vco2, vco3;
    axelf::dsp::LadderFilter vcf;
    axelf::dsp::ADSREnvelope env1;  // filter
    axelf::dsp::ADSREnvelope env2;  // amplitude
    axelf::dsp::LFOGenerator lfo;

    float velocity = 0.0f;
    float glideTarget = 440.0f;
    float currentFreq = 440.0f;
    float noteFrequency = 440.0f;
    int   midiNoteNum = 60;
    float vco2DetuneCents = 0.0f;
    float vco3DetuneCents = 0.0f;
    float mixVco1Level = 0.8f;
    float mixVco2Level = 0.6f;
    float mixVco3Level = 0.4f;
    float vcfBaseCutoff = 400.0f;
    float vcfResonance = 0.4f;
    float vcfDrive = 1.5f;
    float vcfEnvDepth = 0.6f;
    float glideRate = 0.0f;
    float lfoDepthAmount = 0.0f;
    float pitchBendSemitones = 0.0f;
    float modWheelValue = 0.0f;       // CC1 mod wheel 0–1
    bool  smoothingInitialised = false;

    // Phase 2 additions
    float noiseLevel = 0.0f;
    int   vcfKeyTrackMode = 0;
    float vcfLfoAmount = 0.0f;
    float lfoPitchAmt = 0.0f;
    uint32_t noiseState = 54321;

    // Parameter smoothing (Phase 8)
    juce::SmoothedValue<float> cutoffSmoothed;
    juce::SmoothedValue<float> resoSmoothed;
    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> mixVco1Smoothed;
    juce::SmoothedValue<float> mixVco2Smoothed;
    juce::SmoothedValue<float> mixVco3Smoothed;
};

} // namespace axelf::moog15
