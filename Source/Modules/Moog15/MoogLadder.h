#pragma once

namespace axelf::moog15
{

/// Huovilainen improved Moog transistor ladder filter (24 dB/oct lowpass).
/// Never reset between notes — continuous analog behavior.
class MoogLadder
{
public:
    void setSampleRate(double sampleRate);
    void setParameters(float cutoffHz, float resonance, float drive);

    float processSample(float input);

    // Intentionally no reset() — the filter runs continuously

private:
    double currentSampleRate = 44100.0;
    float cutoff = 1000.0f;
    float resonance = 0.0f;
    float drive = 1.0f;

    // 4 cascade stages
    float stage[4] = {};
    float stageZ1[4] = {};
    float delay[4] = {};

    // Cached coefficients
    float G = 0.0f;
    float k = 0.0f;

    void updateCoefficients();
};

} // namespace axelf::moog15
