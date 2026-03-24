#include "IR3109Filter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

namespace axelf::dsp
{

void IR3109Filter::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
}

void IR3109Filter::setParameters(float cutoffHz, float res)
{
    cutoff    = std::clamp(cutoffHz, 20.0f, static_cast<float>(currentSampleRate * 0.49));
    resonance = std::clamp(res, 0.0f, 1.0f);
}

void IR3109Filter::reset()
{
    for (int i = 0; i < 4; ++i)
    {
        stage[i] = 0.0f;
        stageZ1[i] = 0.0f;
    }
    feedback = 0.0f;
}

float IR3109Filter::processSample(float input)
{
    // Roland IR3109 OTA-based model (similar structure to ladder, different character)
    const float g = static_cast<float>(
        std::tan(juce::MathConstants<double>::pi * static_cast<double>(cutoff) / currentSampleRate));
    const float G = g / (1.0f + g);

    // Soft-clip feedback for Roland character
    float x = input - resonance * 3.8f * std::tanh(feedback);

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
