#pragma once

#include "../../Common/ModuleProcessor.h"
#include "PPGWaveParams.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf::ppgwave
{

// Synthesiser subclass that triggers all voices on a single note in Unison mode
class UnisonSynth : public juce::Synthesiser
{
public:
    bool unisonMode = false;

protected:
    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override
    {
        if (!unisonMode)
        {
            Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
            return;
        }

        auto sound = getSound(0);
        if (sound == nullptr) return;

        for (int i = 0; i < getNumVoices(); ++i)
        {
            auto* voice = getVoice(i);
            if (voice != nullptr)
            {
                stopVoice(voice, 0.0f, false);
                startVoice(voice, sound.get(), midiChannel, midiNoteNumber, velocity);
            }
        }
    }

    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override
    {
        if (!unisonMode)
        {
            Synthesiser::noteOff(midiChannel, midiNoteNumber, velocity, allowTailOff);
            return;
        }

        for (int i = 0; i < getNumVoices(); ++i)
        {
            auto* voice = getVoice(i);
            if (voice != nullptr && voice->getCurrentlyPlayingNote() == midiNoteNumber)
                stopVoice(voice, velocity, allowTailOff);
        }
    }
};

class PPGWaveProcessor : public axelf::ModuleProcessor
{
public:
    PPGWaveProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;
    void releaseResources() override;

    juce::Component* createEditor();

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    struct DummyProcessor : juce::AudioProcessor
    {
        const juce::String getName() const override { return "PPGWave"; }
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
    UnisonSynth synth;

    void updateVoiceParameters();
};

} // namespace axelf::ppgwave
