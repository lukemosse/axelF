#pragma once

#include <cmath>

namespace axelf::ppgwave
{

// SSM 2044 4-pole low-pass filter model (PPG Wave 2.2)
// Also supports LP12, BP12, HP12 modes via state-variable reconfiguration
class SSM2044Filter
{
public:
    enum class Mode { LP24, LP12, BP12, HP12 };

    void setSampleRate(double sampleRate);
    void setParameters(float cutoffHz, float resonance, Mode filterMode);
    float getResonance() const { return reso; }
    Mode getMode() const { return mode; }
    void reset();
    float processSample(float input);

private:
    double currentSampleRate = 44100.0;
    float cutoff = 10000.0f;
    float reso = 0.0f;
    Mode mode = Mode::LP24;

    // 4 cascaded one-pole stages
    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
    float g = 0.0f;   // coefficient
    float k = 0.0f;   // resonance feedback

    void updateCoefficients();
};

} // namespace axelf::ppgwave
