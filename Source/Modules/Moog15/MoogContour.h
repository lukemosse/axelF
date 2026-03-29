#pragma once

namespace axelf::moog15
{

class MoogContour
{
public:
    void setSampleRate(double sampleRate);
    void setParameters(float attackTime, float decayTime, float sustainLevel);

    void noteOn();
    void noteOff();

    float getNextSample();
    bool isActive() const { return state != Idle; }

private:
    enum State { Idle, Attack, Decay, Sustain, Release };

    State state = Idle;
    double currentSampleRate = 44100.0;
    float output = 0.0f;
    float attackRate = 0.0f;
    float decayCoeff = 0.0f;
    float sustainLevel = 0.5f;
};

} // namespace axelf::moog15
