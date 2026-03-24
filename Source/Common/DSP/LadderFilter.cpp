#include "LadderFilter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

namespace axelf::dsp
{

void LadderFilter::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
}

void LadderFilter::setParameters(float cutoffHz, float res, float driveAmount)
{
    cutoff    = std::clamp(cutoffHz, 20.0f, static_cast<float>(currentSampleRate * 0.49));
    resonance = std::clamp(res, 0.0f, 1.0f);
    drive     = std::max(driveAmount, 0.1f);
}

void LadderFilter::reset()
{
    for (int i = 0; i < 4; ++i)
    {
        stage[i] = 0.0f;
        stageZ1[i] = 0.0f;
    }
    feedback = 0.0f;
}

float LadderFilter::processSample(float input)
{
    // Simplified Moog ladder model
    const float fc = static_cast<float>(2.0 * currentSampleRate) *
        std::tan(juce::MathConstants<float>::pi * cutoff / static_cast<float>(currentSampleRate));
    const float g = fc / (2.0f * static_cast<float>(currentSampleRate));
    const float G = g / (1.0f + g);

    // Drive / saturation
    float x = std::tanh(input * drive - resonance * 4.0f * feedback);

    // Four cascaded one-pole filters
    for (int i = 0; i < 4; ++i)
    {
        float v = G * (x - stageZ1[i]);
        stage[i] = v + stageZ1[i];
        stageZ1[i] = stage[i] + v;
        x = stage[i];
    }

    feedback = stage[3];
    return stage[3];
}

} // namespace axelf::dsp
