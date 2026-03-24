#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

namespace axelf::dsp
{

/**
 *  Stereo flanger with feedback, modulated delay line.
 *  Process a stereo buffer in-place.
 */
class StereoFlanger
{
public:
    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        int maxDelay = static_cast<int> (sr * 0.02); // 20ms max
        bufL.assign (static_cast<size_t> (maxDelay), 0.0f);
        bufR.assign (static_cast<size_t> (maxDelay), 0.0f);
        writePos = 0;
        phase = 0.0;
        fbStateL = 0.0f;
        fbStateR = 0.0f;
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        writePos = 0;
        phase = 0.0;
        fbStateL = 0.0f;
        fbStateR = 0.0f;
    }

    void setRate     (float hz)  { rate = hz; }
    void setDepth    (float d)   { depth = d; }
    void setFeedback (float fb)  { feedback = std::clamp (fb, 0.0f, 0.95f); }
    void setMix      (float m)   { mix = m; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;
        const int numSamples = buffer.getNumSamples();
        const int bufSize = static_cast<int> (bufL.size());
        if (bufSize < 4) return;

        auto* dataL = buffer.getWritePointer (0);
        auto* dataR = buffer.getWritePointer (1);

        const double phaseInc = rate / sr;
        const float baseDelay = static_cast<float> (sr * 0.001); // 1ms base
        const float modRange  = static_cast<float> (sr * 0.005) * depth; // up to 5ms modulation

        for (int s = 0; s < numSamples; ++s)
        {
            // Write input + feedback into delay
            bufL[static_cast<size_t> (writePos)] = dataL[s] + fbStateL * feedback;
            bufR[static_cast<size_t> (writePos)] = dataR[s] + fbStateR * feedback;

            // LFO — inverted phase for stereo spread
            float lfo = std::sin (static_cast<float> (phase * 2.0 * juce::MathConstants<double>::pi));
            float delayL = baseDelay + (lfo * 0.5f + 0.5f) * modRange;
            float delayR = baseDelay + (-lfo * 0.5f + 0.5f) * modRange;

            float wetL = readInterp (bufL, writePos, delayL, bufSize);
            float wetR = readInterp (bufR, writePos, delayR, bufSize);

            fbStateL = std::clamp (wetL, -1.0f, 1.0f);
            fbStateR = std::clamp (wetR, -1.0f, 1.0f);

            // Output wet only (send effect topology)
            dataL[s] = wetL * mix;
            dataR[s] = wetR * mix;

            writePos = (writePos + 1) % bufSize;
            phase += phaseInc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

private:
    static float readInterp (const std::vector<float>& buf, int wp, float delaySamples, int bufSize)
    {
        float readPos = static_cast<float> (wp) - delaySamples;
        if (readPos < 0.0f) readPos += static_cast<float> (bufSize);
        int idx0 = static_cast<int> (readPos);
        int idx1 = (idx0 + 1) % bufSize;
        float frac = readPos - static_cast<float> (idx0);
        idx0 = idx0 % bufSize;
        return buf[static_cast<size_t> (idx0)] * (1.0f - frac) + buf[static_cast<size_t> (idx1)] * frac;
    }

    double sr = 44100.0;
    std::vector<float> bufL, bufR;
    int writePos = 0;
    double phase = 0.0;
    float fbStateL = 0.0f, fbStateR = 0.0f;
    float rate = 0.3f, depth = 0.5f, feedback = 0.5f, mix = 0.5f;
};

} // namespace axelf::dsp
