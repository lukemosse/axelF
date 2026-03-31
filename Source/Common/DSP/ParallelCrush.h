#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace axelf::dsp
{

/**
 *  Parallel compression bus ("New York Compression").
 *  Taps from mix bus, applies aggressive compression with HPF pre-filter,
 *  and blends back at user-controlled level.
 *  Fixed settings: threshold −30 dB, ratio 10:1, attack 1ms, release 80ms.
 *  HPF at 100 Hz before compressor to prevent bass-induced pumping.
 */
class ParallelCrush
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (maxBlockSize);
        spec.numChannels      = 2;

        compressor.prepare (spec);
        compressor.setThreshold (-30.0f);
        compressor.setRatio (10.0f);
        compressor.setAttack (1.0f);
        compressor.setRelease (80.0f);

        // HPF at 100 Hz (2nd-order Butterworth)
        computeHpfCoeffs (100.0);

        blendSmoothed.reset (sampleRate, 0.01);
        blendSmoothed.setCurrentAndTargetValue (0.0f);

        reset();
    }

    void reset()
    {
        compressor.reset();
        for (auto& s : hpfStateL) s = 0.0f;
        for (auto& s : hpfStateR) s = 0.0f;
    }

    void setBlend (float b) { blendSmoothed.setTargetValue (b); }

    /** Process in-place: buffer is the main mix bus.
     *  Internally copies, crushes, and blends back. */
    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
            return;

        const int numSamples = buffer.getNumSamples();

        // Check if blend is essentially zero
        if (!blendSmoothed.isSmoothing() && blendSmoothed.getTargetValue() < 0.0001f)
            return;

        // Copy input to temp buffer for parallel processing
        crushBuffer.setSize (2, numSamples, false, false, true);
        crushBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        crushBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);

        // Apply HPF to prevent pumping
        auto* cL = crushBuffer.getWritePointer (0);
        auto* cR = crushBuffer.getWritePointer (1);
        for (int s = 0; s < numSamples; ++s)
        {
            cL[s] = applyHpf (cL[s], hpfStateL);
            cR[s] = applyHpf (cR[s], hpfStateR);
        }

        // Compress
        juce::dsp::AudioBlock<float> block (crushBuffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        compressor.process (context);

        // Blend back into main buffer
        auto* outL = buffer.getWritePointer (0);
        auto* outR = buffer.getWritePointer (1);
        for (int s = 0; s < numSamples; ++s)
        {
            const float blend = blendSmoothed.getNextValue();
            outL[s] += cL[s] * blend;
            outR[s] += cR[s] * blend;
        }
    }

private:
    void computeHpfCoeffs (double freq)
    {
        const double w0    = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double cosw  = std::cos (w0);
        const double sinw  = std::sin (w0);
        const double alpha = sinw / (2.0 * 0.7071067811865476);
        const double a0inv = 1.0 / (1.0 + alpha);

        hpfB0 = static_cast<float> ((1.0 + cosw) * 0.5 * a0inv);
        hpfB1 = static_cast<float> (-(1.0 + cosw) * a0inv);
        hpfB2 = hpfB0;
        hpfA1 = static_cast<float> (-2.0 * cosw * a0inv);
        hpfA2 = static_cast<float> ((1.0 - alpha) * a0inv);
    }

    float applyHpf (float input, float* s) const
    {
        float output = hpfB0 * input + s[0];
        s[0] = hpfB1 * input - hpfA1 * output + s[1];
        s[1] = hpfB2 * input - hpfA2 * output;
        return output;
    }

    double sr = 44100.0;
    juce::dsp::Compressor<float> compressor;

    // HPF coefficients
    float hpfB0 = 1.0f, hpfB1 = 0.0f, hpfB2 = 0.0f;
    float hpfA1 = 0.0f, hpfA2 = 0.0f;
    float hpfStateL[2] = {};
    float hpfStateR[2] = {};

    juce::AudioBuffer<float> crushBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> blendSmoothed;
};

} // namespace axelf::dsp
