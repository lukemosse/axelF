#pragma once

#include "../../Common/ModuleProcessor.h"
#include "Jupiter8Params.h"
#include "Jupiter8Arpeggiator.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace axelf::jupiter8
{

class Jupiter8Processor : public axelf::ModuleProcessor
{
public:
    Jupiter8Processor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;
    void releaseResources() override;

    juce::Component* createEditor();

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    void setTransportInfo(double startBeat, double bpm)
    {
        blockStartBeat = startBeat;
        currentBpm = bpm;
    }

private:
    // Dummy processor needed for APVTS construction
    struct DummyProcessor : juce::AudioProcessor
    {
        const juce::String getName() const override { return "Jupiter8"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String&) override {}
        void getStateInformation(juce::MemoryBlock&) override {}
        void setStateInformation(const void*, int) override {}
    };

    DummyProcessor dummyProcessor;
    juce::AudioProcessorValueTreeState apvts;
    juce::Synthesiser synth;
    Jupiter8Arpeggiator arpeggiator;

    // Voice mode tracking
    int currentVoiceMode = 0;  // 0=Poly, 1=Unison, 2=Solo Last, 3=Solo Low, 4=Solo High
    std::vector<int> soloHeldNotes;
    std::vector<float> soloHeldVelocities;
    int lastSoloNote = -1;

    // Transport info set by PluginProcessor before processBlock
    double blockStartBeat = 0.0;
    double currentBpm = 120.0;

    // 2x oversampling for PWM / cross-mod
    juce::dsp::Oversampling<float> oversampler {
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
    };
    double oversampledRate = 44100.0;

    void updateVoiceParameters();

public:
    int getLatencyInSamples() const override
    {
        return static_cast<int>(oversampler.getLatencyInSamples());
    }
};

} // namespace axelf::jupiter8
