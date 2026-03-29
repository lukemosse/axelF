#include "MoogLadder.h"
#include <cmath>
#include <algorithm>

namespace axelf::moog15
{

static constexpr float kVt = 1.22f;  // Thermal voltage scaling

static inline float fastTanh(float x)
{
    // Padé approximant — fast and smooth
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void MoogLadder::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    updateCoefficients();
}

void MoogLadder::setParameters(float cutoffHz, float reso, float driveAmount)
{
    cutoff = std::clamp(cutoffHz, 20.0f, 20000.0f);
    resonance = std::clamp(reso, 0.0f, 1.0f);
    drive = std::clamp(driveAmount, 1.0f, 5.0f);
    updateCoefficients();
}

void MoogLadder::updateCoefficients()
{
    float fc = cutoff / static_cast<float>(currentSampleRate);
    // Huovilainen: frequency warping
    G = 1.0f - std::exp(-2.0f * 3.14159265f * fc);
    // Resonance: 0–1 maps to k = 0–4 (self-oscillation at 4)
    k = resonance * 4.0f;
}

float MoogLadder::processSample(float input)
{
    // Input drive + feedback (delay-free compensation factor 0.5)
    float x = fastTanh(input * drive - k * 0.5f * delay[3]);

    // 4 cascaded one-pole stages with tanh nonlinearity
    for (int i = 0; i < 4; ++i)
    {
        float prevStage = (i == 0) ? x : delay[i - 1];
        stage[i] = delay[i] + G * (fastTanh(prevStage / kVt) - fastTanh(delay[i] / kVt));
        // Prevent denormal CPU stall in filter state
        stage[i] += 1e-20f;
        stage[i] -= 1e-20f;
        delay[i] = stage[i];
    }

    return stage[3];
}

} // namespace axelf::moog15
