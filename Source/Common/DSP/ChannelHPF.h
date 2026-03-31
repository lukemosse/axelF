#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>

namespace axelf::dsp
{

/**
 *  Per-channel high-pass filter (rumble filter).
 *  2nd-order Butterworth HPF with switchable fixed frequencies:
 *    0 = Off, 1 = 40 Hz, 2 = 80 Hz, 3 = 120 Hz
 *  Pre-computes coefficients for each fixed frequency at prepare() time.
 *  64-sample crossfade on mode change to prevent clicks.
 */
class ChannelHPF
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        // Pre-compute Butterworth HPF coefficients for each fixed frequency
        computeCoeffs (40.0,  coeffs[0]);
        computeCoeffs (80.0,  coeffs[1]);
        computeCoeffs (120.0, coeffs[2]);

        reset();
    }

    void reset()
    {
        for (auto& s : state) s = 0.0f;
        for (auto& s : prevState) s = 0.0f;
        crossfadeCount = 0;
        prevMode = 0;
        currentMode = 0;
    }

    /** Set HPF mode: 0=Off, 1=40Hz, 2=80Hz, 3=120Hz */
    void setMode (int mode)
    {
        if (mode != currentMode)
        {
            prevMode = currentMode;
            // Save current filter state for crossfade
            for (int i = 0; i < 4; ++i)
                prevState[i] = state[i];
            currentMode = mode;
            crossfadeCount = kCrossfadeSamples;
        }
    }

    float processSample (float input)
    {
        if (currentMode == 0 && prevMode == 0)
            return input;

        float current = (currentMode > 0) ? applyBiquad (input, coeffs[currentMode - 1], state) : input;

        if (crossfadeCount > 0)
        {
            float prev = (prevMode > 0) ? applyBiquad (input, coeffs[prevMode - 1], prevState) : input;
            float t = static_cast<float> (kCrossfadeSamples - crossfadeCount) / static_cast<float> (kCrossfadeSamples);
            --crossfadeCount;
            return prev + t * (current - prev);
        }

        return current;
    }

private:
    static constexpr int kCrossfadeSamples = 64;

    struct BiquadCoeffs
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    void computeCoeffs (double freq, BiquadCoeffs& c) const
    {
        // 2nd-order Butterworth HPF via bilinear transform
        const double w0  = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cosw = std::cos (w0);
        const double sinw = std::sin (w0);
        const double alpha = sinw / (2.0 * 0.7071067811865476); // Q = sqrt(2)/2 for Butterworth

        const double a0inv = 1.0 / (1.0 + alpha);
        c.b0 = static_cast<float> ((1.0 + cosw) * 0.5 * a0inv);
        c.b1 = static_cast<float> (-(1.0 + cosw) * a0inv);
        c.b2 = c.b0;
        c.a1 = static_cast<float> (-2.0 * cosw * a0inv);
        c.a2 = static_cast<float> ((1.0 - alpha) * a0inv);
    }

    float applyBiquad (float input, const BiquadCoeffs& c, float* s) const
    {
        // Transposed Direct Form II
        float output = c.b0 * input + s[0];
        s[0] = c.b1 * input - c.a1 * output + s[1];
        s[1] = c.b2 * input - c.a2 * output;
        return output;
    }

    double sr = 44100.0;
    int currentMode = 0;
    int prevMode = 0;
    int crossfadeCount = 0;

    std::array<BiquadCoeffs, 3> coeffs; // 40, 80, 120 Hz
    float state[4] = {};     // TDF-II state for current filter (2 per biquad, but 2 channels processed separately)
    float prevState[4] = {}; // TDF-II state for crossfade
};

} // namespace axelf::dsp
