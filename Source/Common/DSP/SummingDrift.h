#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace axelf::dsp
{

/**
 *  Analog summing drift: ultra-slow random gain modulation on the master output.
 *  Models thermal drift in analog components.
 *  Independent L/R with slight decorrelation.
 *  Filtered random walk (~0.05–0.2 Hz), ±0.1 dB (±0.012 linear) at max amount.
 */
class SummingDrift
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        // LP at ~0.5 Hz for smoothing the random walk
        lpCoeff = 1.0f - std::exp (static_cast<float> (
            -2.0 * juce::MathConstants<double>::pi * 0.5 / sampleRate));

        // Different seeds for L/R decorrelation
        rngL = juce::Random (12345);
        rngR = juce::Random (67890);

        amountSmoothed.reset (sampleRate, 0.05);
        amountSmoothed.setCurrentAndTargetValue (0.0f);

        reset();
    }

    void reset()
    {
        driftL = 0.0f;
        driftR = 0.0f;
        walkL  = 0.0f;
        walkR  = 0.0f;
    }

    void setAmount (float a) { amountSmoothed.setTargetValue (a); }

    /** Process stereo buffer in-place. Applies subtle L/R gain drift. */
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
            if (amount < 0.0001f)
                continue;

            // Random walk step (white noise → LP filter)
            walkL += (rngL.nextFloat() * 2.0f - 1.0f) * 0.0001f;
            walkR += (rngR.nextFloat() * 2.0f - 1.0f) * 0.0001f;

            // Clamp walk to prevent unbounded drift
            walkL = std::clamp (walkL, -0.05f, 0.05f);
            walkR = std::clamp (walkR, -0.05f, 0.05f);

            // Smooth with LP filter
            driftL += lpCoeff * (walkL - driftL);
            driftR += lpCoeff * (walkR - driftR);

            // Scale: at amount=1, ±0.012 linear (~±0.1 dB)
            const float gainL = 1.0f + driftL * amount * 0.24f;
            const float gainR = 1.0f + driftR * amount * 0.24f;

            dataL[s] *= gainL;
            dataR[s] *= gainR;
        }
    }

private:
    double sr = 44100.0;
    float lpCoeff = 0.0f;

    juce::Random rngL, rngR;
    float walkL = 0.0f, walkR = 0.0f;
    float driftL = 0.0f, driftR = 0.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
};

} // namespace axelf::dsp
