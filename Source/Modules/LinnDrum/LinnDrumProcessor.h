#pragma once

#include "../../Common/ModuleProcessor.h"
#include "LinnDrumParams.h"
#include "LinnDrumVoice.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf::linndrum
{

class LinnDrumProcessor : public axelf::ModuleProcessor
{
public:
    LinnDrumProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;
    void releaseResources() override;

    juce::Component* createEditor();

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    struct DummyProcessor : juce::AudioProcessor
    {
        const juce::String getName() const override { return "LinnDrum"; }
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

    std::vector<DrumSampleBank> allBanks;   // index 0 = synthetic, 1-10 = disk banks
    int lastBankIndex = -1;                 // force initial assignment

    juce::AudioFormatManager formatManager;

    void loadAllBanks(double sampleRate);
    bool loadBankFromBinaryData(int bankIndex, int bankNumber, double sampleRate);
    bool loadBankFromDisk(int bankIndex, const juce::String& folderName, double sampleRate);
    bool loadSampleFromMemory(DrumSampleBank::Sample& sample, const void* data, int dataSize, double targetSR);
    void generateSyntheticSamples(DrumSampleBank& bank, double sampleRate);
    void updateVoiceParameters();
};

} // namespace axelf::linndrum
