#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace axelf::dsp
{

class ADSREnvelope
{
public:
    enum class State { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate(double sampleRate);
    void setParameters(float attack, float decay, float sustain, float release);

    void noteOn();
    void noteOff();
    void reset();

    float getNextSample();
    bool isActive() const { return state != State::Idle; }
    State getState() const { return state; }

private:
    double currentSampleRate = 44100.0;
    State state = State::Idle;
    float output = 0.0f;

    float attackRate  = 0.0f;
    float decayRate   = 0.0f;
    float sustainLevel = 0.5f;
    float releaseRate = 0.0f;
};

} // namespace axelf::dsp
