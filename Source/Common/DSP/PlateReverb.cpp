#include "PlateReverb.h"
#include <cmath>

namespace axelf::dsp
{

void PlateReverb::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (maxBlockSize);
    spec.numChannels      = 2;
    reverb.prepare (spec);

    // Allocate pre-delay for up to 100 ms
    const int maxPreDelay = static_cast<int> (std::ceil (sampleRate * 0.1)) + 1;
    preDelayBufL.assign (static_cast<size_t> (maxPreDelay), 0.0f);
    preDelayBufR.assign (static_cast<size_t> (maxPreDelay), 0.0f);
    preDelayWritePos = 0;

    paramsDirty = true;
    updateReverbParams();
}

void PlateReverb::reset()
{
    reverb.reset();
    std::fill (preDelayBufL.begin(), preDelayBufL.end(), 0.0f);
    std::fill (preDelayBufR.begin(), preDelayBufR.end(), 0.0f);
    preDelayWritePos = 0;
}

void PlateReverb::setSize    (float v) { if (size    != v) { size    = v; paramsDirty = true; } }
void PlateReverb::setDamping (float v) { if (damping != v) { damping = v; paramsDirty = true; } }
void PlateReverb::setWidth   (float v) { if (width   != v) { width   = v; paramsDirty = true; } }
void PlateReverb::setMix     (float v) { mix = v; }
void PlateReverb::setPreDelay (float ms)
{
    preDelayMs = ms;
    preDelaySamples = static_cast<int> (currentSampleRate * ms * 0.001);
    if (preDelaySamples >= static_cast<int> (preDelayBufL.size()))
        preDelaySamples = static_cast<int> (preDelayBufL.size()) - 1;
}

void PlateReverb::updateReverbParams()
{
    if (! paramsDirty)
        return;

    reverbParams.roomSize   = size;
    reverbParams.damping    = damping;
    reverbParams.width      = width;
    reverbParams.wetLevel   = 1.0f;   // We handle wet/dry ourselves
    reverbParams.dryLevel   = 0.0f;
    reverbParams.freezeMode = 0.0f;
    reverb.setParameters (reverbParams);
    paramsDirty = false;
}

void PlateReverb::process (juce::AudioBuffer<float>& buffer)
{
    updateReverbParams();

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    float* dataL = buffer.getWritePointer (0);
    float* dataR = buffer.getWritePointer (1);
    const int bufSize = static_cast<int> (preDelayBufL.size());

    // ── Apply pre-delay if > 0 ────────────────────────────────
    if (preDelaySamples > 0)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            preDelayBufL[static_cast<size_t> (preDelayWritePos)] = dataL[i];
            preDelayBufR[static_cast<size_t> (preDelayWritePos)] = dataR[i];

            int readPos = preDelayWritePos - preDelaySamples;
            while (readPos < 0) readPos += bufSize;
            if (readPos < 0) readPos = 0;
            if (readPos >= bufSize) readPos = 0;

            dataL[i] = preDelayBufL[static_cast<size_t> (readPos)];
            dataR[i] = preDelayBufR[static_cast<size_t> (readPos)];

            preDelayWritePos = (preDelayWritePos + 1) % bufSize;
        }
    }

    // ── Run reverb (100% wet) ─────────────────────────────────
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    reverb.process (context);

    // ── Apply return level ────────────────────────────────────
    // This is a send/return effect — the dry signal is already in
    // the main bus, so mix acts as a return level that scales the
    // wet output only (no dry crossfade).
    if (mix < 0.999f)
    {
        buffer.applyGain (mix);
    }
}

} // namespace axelf::dsp
