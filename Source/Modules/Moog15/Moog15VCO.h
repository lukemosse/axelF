#pragma once

namespace axelf::moog15
{

class Moog15VCO
{
public:
    void setSampleRate(double sampleRate);
    void setFrequency(float hz);
    void setWaveform(int waveformIndex);
    void setRange(float range);
    void setPulseWidth(float pw);
    void reset();

    float getNextSample();

private:
    double currentSampleRate = 44100.0;
    int waveform = 0;
    float frequency = 440.0f;
    float range = 16.0f;
    float pulseWidth = 0.5f;
    double phase = 0.0;
    double phaseIncrement = 0.0;

    void updatePhaseIncrement();
    float polyBlepResidual(double t) const;
};

} // namespace axelf::moog15
