#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Moog15VCO.h"
#include "MoogContour.h"
#include "MoogLadder.h"

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

    void setParameters(int vco1Wave, int vco1Range, float vco1Level, float vco1PW,
                       int vco2Wave, int vco2Range, float vco2Detune, float vco2Level, float vco2PW,
                       int vco3Wave, int vco3Range, float vco3Detune, float vco3Level, float vco3PW,
                       int vco3Ctrl,
                       float noiseLevel, int noiseColor,
                       float feedback,
                       float cutoff, float reso, float drive, float envDepth, int keyTrack,
                       float e1A, float e1D, float e1S,
                       float e2A, float e2D, float e2S,
                       float glideTime,
                       float lfoPitchAmt, float vcfLfoAmt,
                       float lfoRate, int lfoWaveform,
                       float masterVol);

private:
    // Three free-running VCOs (never reset)
    Moog15VCO vco1, vco2, vco3;

    // Dedicated Moog ladder filter (never reset — continuous analog behavior)
    MoogLadder vcf;

    // Model D contour generators (NOT ADSRs — retrigger from current level)
    MoogContour filterContour;   // env1 → VCF cutoff
    MoogContour loudnessContour; // env2 → VCA amplitude

    // Voice state
    float velocity = 0.0f;
    float glideTarget = 440.0f;
    float currentFreq = 440.0f;
    int   midiNoteNum = 60;
    float pitchBendSemitones = 0.0f;
    float modWheelValue = 0.0f;

    // Oscillator parameters
    float vco2DetuneCents = 0.0f;
    float vco3DetuneCents = 0.0f;
    int   vco3CtrlMode = 0;             // 0=normal, 1=LFO mode (ignores keyboard)

    // Range multipliers per VCO (indexed 0–5: 32'/16'/8'/4'/2'/Lo)
    static constexpr float kRangeMultipliers[] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
    int vco1RangeIdx = 1;
    int vco2RangeIdx = 1;
    int vco3RangeIdx = 1;

    // Feedback path (output → mixer)
    float feedbackSample = 0.0f;
    float feedbackGain = 0.0f;

    // Glide
    float glideTime = 0.05f;

    // Filter modulation
    float vcfBaseCutoff = 2000.0f;
    float vcfResonance = 0.2f;
    float vcfDrive = 1.5f;
    float vcfEnvDepth = 0.3f;
    int   vcfKeyTrackMode = 0;          // 0=off, 1=33%, 2=67%, 3=100%
    float vcfLfoAmount = 0.0f;

    // LFO (separate from Osc 3 as LFO)
    float lfoPhase = 0.0f;
    float lfoPhaseInc = 0.0f;
    int   lfoWaveform = 0;
    float lfoPitchAmount = 0.0f;

    // Noise
    float noiseLevelParam = 0.0f;
    int   noiseColorParam = 0;           // 0=white, 1=pink
    uint32_t noiseState = 54321;
    float pinkState[3] = {};             // Paul Kellet pink filter state
    float shValue = 0.0f;                // S&H LFO current hold value

    // Master
    float masterVolume = 0.8f;

    // DC blocker state (per synth-click-prevention skill)
    float dcBlockX1 = 0.0f;
    float dcBlockY1 = 0.0f;

    // Parameter smoothing (per DSP conventions — all continuous params)
    juce::SmoothedValue<float> cutoffSmoothed;
    juce::SmoothedValue<float> resoSmoothed;
    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> mixVco1Smoothed;
    juce::SmoothedValue<float> mixVco2Smoothed;
    juce::SmoothedValue<float> mixVco3Smoothed;
    juce::SmoothedValue<float> noiseLevelSmoothed;
    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float> envDepthSmoothed;
    juce::SmoothedValue<float> masterVolSmoothed;

    // Fast 2^x approximation for per-sample pitch/filter calculations
    static inline float fastPow2(float x);
    // LFO tick
    float getLfoSample();
    // Noise
    float getWhiteNoise();
    float getPinkNoise();
    // DC blocker
    float dcBlock(float x);
};

} // namespace axelf::moog15
