#include "LFOGenerator.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace axelf::dsp
{

void LFOGenerator::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    updatePhaseIncrement();
}

void LFOGenerator::setFrequency(float hz)
{
    frequency = hz;
    updatePhaseIncrement();
}

void LFOGenerator::setWaveform(LFOWaveform waveform)
{
    currentWaveform = waveform;
}

void LFOGenerator::reset()
{
    phase = 0.0;
    currentValue = 0.0f;
}

float LFOGenerator::getNextSample()
{
    float out = 0.0f;

    switch (currentWaveform)
    {
        case LFOWaveform::Sine:
            out = static_cast<float>(std::sin(phase * 2.0 * juce::MathConstants<double>::pi));
            break;

        case LFOWaveform::Triangle:
            out = static_cast<float>(2.0 * std::abs(2.0 * (phase - std::floor(phase + 0.5))) - 1.0);
            break;

        case LFOWaveform::Sawtooth:
            out = static_cast<float>(2.0 * phase - 1.0);
            break;

        case LFOWaveform::Square:
            out = phase < 0.5 ? 1.0f : -1.0f;
            break;

        case LFOWaveform::SampleAndHold:
        {
            double prevPhase = phase - phaseIncrement;
            if (prevPhase < 0.0)
                prevPhase += 1.0;
            if (phase < prevPhase)  // wrapped around
                currentValue = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f;
            out = currentValue;
            break;
        }
    }

    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    currentValue = out;
    return out;
}

void LFOGenerator::updatePhaseIncrement()
{
    phaseIncrement = static_cast<double>(frequency) / currentSampleRate;
}

} // namespace axelf::dsp
