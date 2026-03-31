#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <array>

namespace axelf::dsp
{

/**
 *  Analog noise floor generator.
 *  Per-channel decorrelated noise shaped with LP ~12 kHz and HP ~60 Hz
 *  to match console hiss spectral profile.
 *  Level controllable from ~-100 dBFS to ~-60 dBFS.
 */
class ConsoleNoise
{
public:
    static constexpr int kNumChannels = 6;

    void prepare (double sampleRate)
    {
        // LP at ~12 kHz
        lpCoeff = 1.0f - std::exp (static_cast<float> (
            -2.0 * juce::MathConstants<double>::pi * 12000.0 / sampleRate));

        // HP at ~60 Hz (DC blocker)  
        hpCoeff = 1.0f - std::exp (static_cast<float> (
            -2.0 * juce::MathConstants<double>::pi * 60.0 / sampleRate));

        amountSmoothed.reset (sampleRate, 0.05); // 50ms
        amountSmoothed.setCurrentAndTargetValue (0.0f);

        // Initialize PRNGs with different seeds per channel
        for (int i = 0; i < kNumChannels * 2; ++i)
            rng[i] = juce::Random (static_cast<juce::int64> (i * 31337 + 42));

        reset();
    }

    void reset()
    {
        for (auto& s : lpState) s = 0.0f;
        for (auto& s : hpState) s = 0.0f;
    }

    /** Set noise amount: 0 = off, 1 = max (~-60 dBFS) */
    void setAmount (float a) { amountSmoothed.setTargetValue (a); }

    /** Generate a noise sample for a specific channel + stereo index.
     *  channelIdx: 0-5, stereoIdx: 0=L, 1=R */
    float generateSample (int channelIdx, int stereoIdx)
    {
        const float amount = amountSmoothed.getNextValue();
        if (amount < 0.0001f)
            return 0.0f;

        // Map amount 0-1 to gain: 0 to ~-60 dBFS (0.001)
        // Using exponential mapping for perceptual linearity
        const float gain = amount * 0.001f;

        const int idx = channelIdx * 2 + stereoIdx;

        // Generate white noise (-1 to 1)
        float noise = rng[idx].nextFloat() * 2.0f - 1.0f;

        // Shape: LP at ~12 kHz
        lpState[idx] += lpCoeff * (noise - lpState[idx]);
        float shaped = lpState[idx];

        // Shape: HP at ~60 Hz
        hpState[idx] += hpCoeff * (shaped - hpState[idx]);
        shaped -= hpState[idx];

        return shaped * gain;
    }

private:
    float lpCoeff = 0.0f;
    float hpCoeff = 0.0f;

    std::array<float, kNumChannels * 2> lpState = {};
    std::array<float, kNumChannels * 2> hpState = {};
    std::array<juce::Random, kNumChannels * 2> rng;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
};

} // namespace axelf::dsp
