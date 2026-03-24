#pragma once

#include "../../Common/ModuleProcessor.h"
#include "DX7Params.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf::dx7
{

class DX7Processor : public axelf::ModuleProcessor
{
public:
    DX7Processor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;
    void releaseResources() override;

    juce::Component* createEditor();

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    struct DummyProcessor : juce::AudioProcessor
    {
        const juce::String getName() const override { return "DX7"; }
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

    // 4x oversampling for FM synthesis
    juce::dsp::Oversampling<float> oversampler {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
    };
    double oversampledRate = 44100.0;

    void updateVoiceParameters();

    // DC blocker (single-pole HPF ~5 Hz)
    float dcX[2] = {};
    float dcY[2] = {};
    float dcR = 0.0f;

public:
    int getLatencyInSamples() const override
    {
        return static_cast<int>(oversampler.getLatencyInSamples());
    }
};

} // namespace axelf::dx7
