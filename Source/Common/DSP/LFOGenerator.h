#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace axelf::dsp
{

enum class LFOWaveform { Sine, Triangle, Sawtooth, Square, SampleAndHold };

class LFOGenerator
{
public:
    void setSampleRate(double sampleRate);
    void setFrequency(float hz);
    void setWaveform(LFOWaveform waveform);
    void reset();

    float getNextSample();
    float getCurrentValue() const { return currentValue; }

private:
    double currentSampleRate = 44100.0;
    LFOWaveform currentWaveform = LFOWaveform::Sine;
    float frequency = 1.0f;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float currentValue = 0.0f;

    void updatePhaseIncrement();
};

} // namespace axelf::dsp
