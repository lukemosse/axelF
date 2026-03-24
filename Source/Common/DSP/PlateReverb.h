#pragma once

#include <juce_dsp/juce_dsp.h>

namespace axelf::dsp
{

/**
 *  Stereo plate reverb built on juce::dsp::Reverb with
 *  APVTS-friendly parameter interface and pre-delay line.
 */
class PlateReverb
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // Set parameters (call per-block from audio thread)
    void setSize     (float v);   // 0–1
    void setDamping  (float v);   // 0–1
    void setWidth    (float v);   // 0–1
    void setMix      (float v);   // 0–1  wet/dry on return
    void setPreDelay (float ms);  // 0–100 ms

    /** Process a stereo buffer **in-place**. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    void updateReverbParams();

    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;

    // Pre-delay circular buffer (stereo)
    std::vector<float> preDelayBufL, preDelayBufR;
    int preDelayWritePos = 0;
    int preDelaySamples  = 0;
    double currentSampleRate = 44100.0;

    float size    = 0.5f;
    float damping = 0.5f;
    float width   = 1.0f;
    float mix     = 1.0f;
    float preDelayMs = 0.0f;
    bool  paramsDirty = true;
};

} // namespace axelf::dsp
