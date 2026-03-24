#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace axelf
{

class ModuleProcessor
{
public:
    virtual ~ModuleProcessor() = default;

    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
    virtual void processBlock(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midiMessages) = 0;
    virtual void releaseResources() = 0;

    virtual int getLatencyInSamples() const { return 0; }

    void setMidiChannel(int channel) { midiChannel = channel; }
    int getMidiChannel() const { return midiChannel; }

protected:
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int midiChannel = 0;  // 0 = omni
};

} // namespace axelf
