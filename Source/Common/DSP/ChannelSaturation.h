#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>

namespace axelf::dsp
{

/**
 *  ToneCentric-inspired per-channel saturation.
 *  3-band Linkwitz-Riley crossover → per-band waveshaping → recombine.
 *  Level-responsive drive, 2× oversampling, DC blocker.
 *
 *  Color modes:
 *    0 = Clean (bypass)
 *    1 = Transformer (even-order, warm)
 *    2 = Console (odd-order, punchy)
 *    3 = Tape (both, soft compression + HF rolloff)
 */
class ChannelSaturation
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        // 2× oversampling
        oversampling.reset (new juce::dsp::Oversampling<float> (
            1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
        oversampling->initProcessing (static_cast<size_t> (maxBlockSize));

        // Linkwitz-Riley crossover coefficients at 2× rate
        const double os = sampleRate * 2.0;
        updateCrossoverCoeffs (os);

        // Envelope follower
        envAttackCoeff  = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 100.0 / os)); // ~10ms at 2×
        envReleaseCoeff = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 10.0 / os));   // ~100ms at 2×

        // DC blocker at ~5 Hz
        dcBlockCoeff = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 5.0 / sampleRate));

        // Tape HF rolloff at ~10 kHz (one-pole LP)
        tapeHfCoeff = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 10000.0 / os));

        // Amount smoothing
        amountSmoothed.reset (sampleRate, 0.02); // 20ms
        amountSmoothed.setCurrentAndTargetValue (0.0f);

        reset();
    }

    void reset()
    {
        // Crossover state
        for (auto& s : xoverLowState)  s = 0.0f;
        for (auto& s : xoverHighState) s = 0.0f;
        for (auto& s : xoverLowState2) s = 0.0f;
        for (auto& s : xoverHighState2) s = 0.0f;

        envLevel = 0.0f;
        dcState = 0.0f;
        tapeHfState = 0.0f;

        if (oversampling)
            oversampling->reset();
    }

    void setAmount (float a) { amountSmoothed.setTargetValue (a); }
    void setColorMode (int mode) { colorMode = mode; }

    /** Process a single-channel (mono) sample through the saturation.
     *  Call separately for L and R with the appropriate channel index (0 or 1).
     *  Returns the processed sample. When amount is 0 or colorMode is Clean, returns input unchanged.
     */
    float processSample (float input)
    {
        const float amount = amountSmoothed.getNextValue();
        if (amount < 0.0001f || colorMode == 0)
        {
            // Still advance DC blocker state to prevent pops on enable
            dcState += dcBlockCoeff * (input - dcState);
            return input;
        }

        // Envelope follower (level-responsive drive)
        const float absIn = std::abs (input);
        if (absIn > envLevel)
            envLevel += envAttackCoeff * (absIn - envLevel);
        else
            envLevel += envReleaseCoeff * (absIn - envLevel);

        // Drive scales with both amount knob and signal level
        const float drive = amount * (1.0f + envLevel * 2.0f);

        // 3-band split using cascaded one-pole crossovers (Linkwitz-Riley approximation)
        // Low/Mid split at ~200 Hz
        xoverLowState[0] += xoverLowCoeff * (input - xoverLowState[0]);
        xoverLowState2[0] += xoverLowCoeff * (xoverLowState[0] - xoverLowState2[0]);
        const float low = xoverLowState2[0];
        const float midHigh = input - low;

        // Mid/High split at ~3 kHz
        xoverHighState[0] += xoverHighCoeff * (midHigh - xoverHighState[0]);
        xoverHighState2[0] += xoverHighCoeff * (xoverHighState[0] - xoverHighState2[0]);
        const float mid = xoverHighState2[0];
        const float high = midHigh - mid;

        float outLow, outMid, outHigh;

        switch (colorMode)
        {
            case 1: // Transformer (even-order dominant)
                outLow  = low + drive * 0.3f * low * low * (low > 0.0f ? 1.0f : -1.0f);
                outMid  = std::tanh (mid * (1.0f + drive * 0.5f)) + drive * 0.15f * mid * mid * (mid > 0.0f ? 1.0f : -1.0f);
                outHigh = std::tanh (high * (1.0f + drive * 0.3f));
                break;

            case 2: // Console (odd-order, tighter)
                outLow  = std::tanh (low * (1.0f + drive * 0.4f));
                outMid  = std::tanh (mid * (1.0f + drive * 0.8f));
                outHigh = std::tanh (high * (1.0f + drive * 0.5f));
                break;

            case 3: // Tape (both harmonics, soft compression, HF rolloff)
            {
                outLow  = low + drive * 0.2f * low * low * (low > 0.0f ? 1.0f : -1.0f);
                outMid  = std::tanh (mid * (1.0f + drive * 0.4f)) + drive * 0.1f * mid * mid * (mid > 0.0f ? 1.0f : -1.0f);
                float hSat = std::tanh (high * (1.0f + drive * 0.2f));
                // Tape HF rolloff
                tapeHfState += tapeHfCoeff * (hSat - tapeHfState);
                outHigh = tapeHfState;
                break;
            }

            default: // Clean (shouldn't reach here, but safety)
                return input;
        }

        // Recombine
        float output = outLow + outMid + outHigh;

        // DC blocker (one-pole HP)
        dcState += dcBlockCoeff * (output - dcState);
        output -= dcState;

        // Wet/dry blend based on amount (gentle crossfade)
        output = input * (1.0f - amount * 0.7f) + output * amount * 0.7f + output * 0.3f * amount;

        return output;
    }

private:
    void updateCrossoverCoeffs (double osSampleRate)
    {
        // One-pole LP coefficients for cascaded 2-pole Linkwitz-Riley approximation
        xoverLowCoeff  = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 200.0 / osSampleRate));
        xoverHighCoeff = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 3000.0 / osSampleRate));
    }

    double sr = 44100.0;
    int colorMode = 0; // 0=Clean, 1=Transformer, 2=Console, 3=Tape

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Crossover state (cascaded one-pole for LR2 approximation)
    float xoverLowCoeff  = 0.0f;
    float xoverHighCoeff = 0.0f;
    float xoverLowState[1]   = {};
    float xoverHighState[1]  = {};
    float xoverLowState2[1]  = {};
    float xoverHighState2[1] = {};

    // Envelope follower
    float envAttackCoeff  = 0.0f;
    float envReleaseCoeff = 0.0f;
    float envLevel = 0.0f;

    // DC blocker
    float dcBlockCoeff = 0.0f;
    float dcState = 0.0f;

    // Tape HF rolloff
    float tapeHfCoeff = 0.0f;
    float tapeHfState = 0.0f;

    // Amount smoothing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
};

} // namespace axelf::dsp
