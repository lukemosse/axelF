#include "SSM2044Filter.h"
#include <algorithm>

namespace axelf::ppgwave
{

static constexpr float kPi = 3.14159265358979323846f;

void SSM2044Filter::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    updateCoefficients();
}

void SSM2044Filter::setParameters(float cutoffHz, float resonance, Mode filterMode)
{
    cutoff = std::clamp(cutoffHz, 20.0f, static_cast<float>(currentSampleRate * 0.49));
    reso = std::clamp(resonance, 0.0f, 1.0f);
    mode = filterMode;
    updateCoefficients();
}

void SSM2044Filter::reset()
{
    s1 = s2 = s3 = s4 = 0.0f;
}

float SSM2044Filter::processSample(float input)
{
    // SSM 2044 nonlinear saturation on input
    float saturated = std::tanh(input - k * s4);

    // 4-pole cascade (each pole: y = y + g * (tanh(x) - y))
    float y1 = s1 + g * (std::tanh(saturated) - s1);
    float y2 = s2 + g * (std::tanh(y1) - s2);
    float y3 = s3 + g * (std::tanh(y2) - s3);
    float y4 = s4 + g * (std::tanh(y3) - s4);

    // Flush denormals to prevent CPU spikes in filter tail
    auto flush = [](float x) { return (std::abs(x) < 1.0e-15f) ? 0.0f : x; };
    s1 = flush(y1);
    s2 = flush(y2);
    s3 = flush(y3);
    s4 = flush(y4);

    switch (mode)
    {
        case Mode::LP24: return y4;
        case Mode::LP12: return y2;
        case Mode::BP12: return y2 - y4;
        case Mode::HP12: return input - y2 - k * y4;
    }

    return y4;
}

void SSM2044Filter::updateCoefficients()
{
    float sr = static_cast<float>(currentSampleRate);
    g = 1.0f - std::exp(-2.0f * kPi * cutoff / sr);
    k = reso * 4.0f;  // Scale 0-1 resonance to 0-4 feedback
}

} // namespace axelf::ppgwave
