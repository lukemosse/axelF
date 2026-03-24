#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace axelf::dsp
{

/**
 *  Simple stereo distortion with soft-clipping waveshaper,
 *  tone control (one-pole LP), and wet/dry mix.
 *  Process a stereo buffer in-place.
 */
class Distortion
{
public:
    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        lpStateL = 0.0f;
        lpStateR = 0.0f;
        updateToneCoeff();
    }

    void reset()
    {
        lpStateL = 0.0f;
        lpStateR = 0.0f;
    }

    void setDrive (float d) { drive = d; }
    void setTone  (float t) { tone = t; updateToneCoeff(); }
    void setMix   (float m) { mix = m; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;
        const int numSamples = buffer.getNumSamples();

        auto* dataL = buffer.getWritePointer (0);
        auto* dataR = buffer.getWritePointer (1);

        // Drive maps 0-1 to gain 1x–20x
        const float gain = 1.0f + drive * 19.0f;

        for (int s = 0; s < numSamples; ++s)
        {
            float dryL = dataL[s];
            float dryR = dataR[s];

            // Apply gain + soft clip (tanh)
            float wetL = std::tanh (dryL * gain);
            float wetR = std::tanh (dryR * gain);

            // Tone filter (one-pole LP)
            lpStateL += toneCoeff * (wetL - lpStateL);
            lpStateR += toneCoeff * (wetR - lpStateR);
            wetL = lpStateL;
            wetR = lpStateR;

            // Output wet only (send effect topology)
            dataL[s] = wetL * mix;
            dataR[s] = wetR * mix;
        }
    }

private:
    void updateToneCoeff()
    {
        // tone 0 = dark (1kHz), tone 1 = bright (20kHz)
        float freq = 1000.0f + tone * 19000.0f;
        float w = 2.0f * juce::MathConstants<float>::pi * freq / static_cast<float> (sr);
        toneCoeff = std::clamp (w / (1.0f + w), 0.0001f, 0.9999f);
    }

    double sr = 44100.0;
    float drive = 0.3f, tone = 0.5f, mix = 0.5f;
    float toneCoeff = 0.5f;
    float lpStateL = 0.0f, lpStateR = 0.0f;
};

} // namespace axelf::dsp
