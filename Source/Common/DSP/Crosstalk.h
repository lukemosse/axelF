#pragma once

#include <cmath>
#include <array>
#include <juce_core/juce_core.h>

namespace axelf::dsp
{

/**
 *  Console crosstalk: adjacent-channel bleed simulation.
 *  Each channel picks up a tiny fraction of its neighbors,
 *  filtered through a one-pole LP at ~8 kHz (capacitive coupling model).
 *  Applied to pre-fader signals in a non-recursive pass using a temp buffer.
 */
class Crosstalk
{
public:
    static constexpr int kNumChannels = 6;

    void prepare (double sampleRate)
    {
        // LP at ~8 kHz for bleed signal (capacitive coupling)
        lpCoeff = 1.0f - std::exp (static_cast<float> (
            -2.0 * juce::MathConstants<double>::pi * 8000.0 / sampleRate));

        amountSmoothed.reset (sampleRate, 0.05); // 50ms
        amountSmoothed.setCurrentAndTargetValue (0.0f);

        reset();
    }

    void reset()
    {
        for (auto& s : lpState) s = 0.0f;
    }

    /** Set crosstalk amount: 0 = off, 1 = max (maps to ~-60 dB bleed) */
    void setAmount (float a) { amountSmoothed.setTargetValue (a); }

    /** Process one sample for all 6 channels (stereo interleaved as L).
     *  Call with arrays of 6 channel values. Modifies channelSamples in-place.
     *  This processes one channel of stereo (call once for L, once for R).
     */
    void processSamples (float* channelSamples, int channelIndex)
    {
        const float amount = amountSmoothed.getNextValue();
        if (amount < 0.0001f)
            return;

        // Bleed level: map 0-1 to 0 to ~-60 dB (0.001)
        const float bleed = amount * 0.001f;

        // Non-recursive: use stored pre-bleed values
        float temp[kNumChannels];
        for (int i = 0; i < kNumChannels; ++i)
            temp[i] = channelSamples[i];

        for (int i = 0; i < kNumChannels; ++i)
        {
            float crosstalkSum = 0.0f;

            // Bleed from adjacent channels
            if (i > 0)
                crosstalkSum += temp[i - 1];
            if (i < kNumChannels - 1)
                crosstalkSum += temp[i + 1];

            // LP filter the bleed
            const int stateIdx = i * 2 + channelIndex;
            lpState[stateIdx] += lpCoeff * (crosstalkSum * bleed - lpState[stateIdx]);

            channelSamples[i] += lpState[stateIdx];
        }
    }

private:
    float lpCoeff = 0.0f;
    float lpState[kNumChannels * 2] = {}; // per-channel × L/R

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
};

} // namespace axelf::dsp
