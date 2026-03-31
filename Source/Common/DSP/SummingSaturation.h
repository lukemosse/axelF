#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace axelf::dsp
{

/**
 *  Analog summing saturation applied to the stereo mix bus.
 *  Level-dependent soft saturation with even-order emphasis.
 *  2× oversampling, DC blocker.
 *
 *  Color modes:
 *    0 = Clean (bypass)
 *    1 = Neve (warm, even-order)
 *    2 = SSL (tighter, odd-order)
 *    3 = API (punchy, both)
 */
class SummingSaturation
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        oversampling.reset (new juce::dsp::Oversampling<float> (
            2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
        oversampling->initProcessing (static_cast<size_t> (maxBlockSize));

        const double os = sampleRate * 2.0;

        // RMS detector ~50ms
        rmsCoeff = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 20.0 / os));

        // DC blocker ~5 Hz
        dcBlockCoeff = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 5.0 / sampleRate));

        amountSmoothed.reset (sampleRate, 0.02);
        amountSmoothed.setCurrentAndTargetValue (0.0f);

        reset();
    }

    void reset()
    {
        rmsLevel = 0.0f;
        dcStateL = 0.0f;
        dcStateR = 0.0f;
        if (oversampling)
            oversampling->reset();
    }

    void setAmount (float a) { amountSmoothed.setTargetValue (a); }
    void setColorMode (int mode) { colorMode = mode; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
            return;

        const int numSamples = buffer.getNumSamples();
        auto* dataL = buffer.getWritePointer (0);
        auto* dataR = buffer.getWritePointer (1);

        for (int s = 0; s < numSamples; ++s)
        {
            const float amount = amountSmoothed.getNextValue();
            if (amount < 0.0001f || colorMode == 0)
            {
                dcStateL += dcBlockCoeff * (dataL[s] - dcStateL);
                dcStateR += dcBlockCoeff * (dataR[s] - dcStateR);
                continue;
            }

            // RMS envelope
            const float mono = (dataL[s] + dataR[s]) * 0.5f;
            rmsLevel += rmsCoeff * (mono * mono - rmsLevel);
            const float rms = std::sqrt (rmsLevel + 1e-12f);

            // Level-dependent drive
            const float drive = amount * (1.0f + rms * 3.0f);

            float outL = saturate (dataL[s], drive);
            float outR = saturate (dataR[s], drive);

            // DC blocker
            dcStateL += dcBlockCoeff * (outL - dcStateL);
            dcStateR += dcBlockCoeff * (outR - dcStateR);
            outL -= dcStateL;
            outR -= dcStateR;

            // Wet/dry blend
            dataL[s] = dataL[s] * (1.0f - amount) + outL * amount;
            dataR[s] = dataR[s] * (1.0f - amount) + outR * amount;
        }
    }

private:
    float saturate (float x, float drive) const
    {
        switch (colorMode)
        {
            case 1: // Neve — even-order asymmetric
            {
                const float biased = x + 0.1f * drive;
                return std::tanh (biased * (1.0f + drive)) - std::tanh (0.1f * drive);
            }
            case 2: // SSL — odd-order, tight
                return std::tanh (x * (1.0f + drive * 1.5f));

            case 3: // API — punchy, both harmonics
            {
                float odd = std::tanh (x * (1.0f + drive));
                float even = drive * 0.2f * x * x * (x > 0.0f ? 1.0f : -1.0f);
                return odd + even;
            }

            default:
                return x;
        }
    }

    double sr = 44100.0;
    int colorMode = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    float rmsCoeff = 0.0f;
    float rmsLevel = 0.0f;

    float dcBlockCoeff = 0.0f;
    float dcStateL = 0.0f;
    float dcStateR = 0.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
};

} // namespace axelf::dsp
