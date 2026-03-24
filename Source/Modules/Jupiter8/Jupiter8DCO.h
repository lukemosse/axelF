#pragma once

namespace axelf::jupiter8
{

class Jupiter8DCO
{
public:
    void setSampleRate(double sampleRate);
    void setFrequency(float hz);
    void setWaveform(int waveformIndex);
    void setRange(float range);
    void setPulseWidth(float pw);
    void resetPhase();
    bool didPhaseWrap() const { return phaseWrapped; }
    void reset();

    float getNextSample();

private:
    double currentSampleRate = 44100.0;
    int waveform = 0;
    float frequency = 440.0f;
    float range = 8.0f;
    float pulseWidth = 0.5f;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    bool phaseWrapped = false;

    void updatePhaseIncrement();
    float polyBlepResidual(double t) const;
};

} // namespace axelf::jupiter8
