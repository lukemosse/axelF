#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "DX7Operator.h"
#include "DX7Algorithm.h"
#include "../../Common/DSP/LFOGenerator.h"
#include <array>

namespace axelf::dx7
{

class DX7Sound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class DX7Voice : public juce::SynthesiserVoice
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

    void setAlgorithm(int alg) { algorithm.setAlgorithm(alg); }
    void setFeedback(int fb) { feedbackAmount = fb; }
    void setLFOParameters(float rate, float pmd, float amd, int waveform, float delay = 0.0f);
    DX7Operator& getOperator(int index) { return operators[static_cast<size_t>(index)]; }

private:
    std::array<DX7Operator, 6> operators;
    DX7Algorithm algorithm;
    axelf::dsp::LFOGenerator lfo;
    int feedbackAmount = 0;
    float lastFeedbackSample = 0.0f;
    float prevFeedbackSample = 0.0f;
    float velocity = 0.0f;
    float noteFrequency = 440.0f;
    float pitchBendSemitones = 0.0f;
    float modWheelValue = 0.0f;       // CC1 mod wheel 0–1
    float lfoPMD = 0.0f;
    float lfoAMD = 0.0f;
    int lfoDelaySamples = 0;
    int samplesSinceNoteOn = 0;
};

} // namespace axelf::dx7
