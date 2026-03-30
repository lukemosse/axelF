#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace axelf::dsp
{

/// Per-channel stereo delay line for reverb send depth positioning.
/// Smooths delay time changes to prevent clicks.
class SendPreDelay
{
public:
    void prepare (double sampleRate, float maxDelayMs = 100.0f)
    {
        sr = sampleRate;
        const int maxSamples = static_cast<int> (std::ceil (sampleRate * maxDelayMs * 0.001)) + 1;
        bufL.assign (static_cast<size_t> (maxSamples), 0.0f);
        bufR.assign (static_cast<size_t> (maxSamples), 0.0f);
        bufSize = maxSamples;
        writePos = 0;
        delaySmoothed.reset (sampleRate, 0.02);  // 20 ms ramp
        delaySmoothed.setCurrentAndTargetValue (0.0f);
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        writePos = 0;
    }

    void setDelayMs (float ms)
    {
        float delaySamples = static_cast<float> (sr * ms * 0.001);
        if (delaySamples >= static_cast<float> (bufSize - 1))
            delaySamples = static_cast<float> (bufSize - 2);
        delaySmoothed.setTargetValue (delaySamples);
    }

    /// Process one stereo sample pair through the delay line.
    /// Returns the delayed output as {L, R}.
    std::pair<float, float> processSample (float inL, float inR)
    {
        bufL[static_cast<size_t> (writePos)] = inL;
        bufR[static_cast<size_t> (writePos)] = inR;

        const float delay = delaySmoothed.getNextValue();

        // Integer + fractional split for linear interpolation
        const float readPosF = static_cast<float> (writePos) - delay;
        const int readFloor = static_cast<int> (std::floor (readPosF));
        const float frac = readPosF - static_cast<float> (readFloor);

        // Wrap to valid ring-buffer indices
        const int i0 = ((readFloor % bufSize) + bufSize) % bufSize;
        const int i1 = (i0 + 1) % bufSize;

        const float outL = bufL[static_cast<size_t> (i0)]
                         + frac * (bufL[static_cast<size_t> (i1)] - bufL[static_cast<size_t> (i0)]);
        const float outR = bufR[static_cast<size_t> (i0)]
                         + frac * (bufR[static_cast<size_t> (i1)] - bufR[static_cast<size_t> (i0)]);

        writePos = (writePos + 1) % bufSize;
        return { outL, outR };
    }

private:
    std::vector<float> bufL, bufR;
    int bufSize = 1;
    int writePos = 0;
    double sr = 44100.0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySmoothed;
};

} // namespace axelf::dsp
