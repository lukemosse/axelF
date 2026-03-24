#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Common/MasterMixer.h"
#include "Common/ModulationMatrix.h"
#include "Common/PresetManager.h"
#include "Common/GlobalTransport.h"
#include "Common/PatternEngine.h"
#include "Common/SceneManager.h"
#include "Common/ArrangementTimeline.h"
#include "Common/ArrangementCapture.h"
#include "Common/MetronomeClick.h"
#include "Common/MidiLearn.h"
#include "Common/DSP/PlateReverb.h"
#include "Common/DSP/StereoDelay.h"
#include "Common/DSP/StereoChorus.h"
#include "Common/DSP/StereoFlanger.h"
#include "Common/DSP/Distortion.h"
#include "Common/DSP/MasterBus.h"
#include <juce_core/juce_core.h>
#include "Modules/Jupiter8/Jupiter8Processor.h"
#include "Modules/Moog15/Moog15Processor.h"
#include "Modules/JX3P/JX3PProcessor.h"
#include "Modules/DX7/DX7Processor.h"
#include "Modules/LinnDrum/LinnDrumProcessor.h"

namespace axelf
{

class AxelFProcessor : public juce::AudioProcessor
{
public:
    AxelFProcessor();
    ~AxelFProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Module accessors
    jupiter8::Jupiter8Processor& getJupiter8() { return jupiter8; }
    moog15::Moog15Processor& getMoog15() { return moog15; }
    jx3p::JX3PProcessor& getJX3P() { return jx3p; }
    dx7::DX7Processor& getDX7() { return dx7; }
    linndrum::LinnDrumProcessor& getLinnDrum() { return linnDrum; }
    MasterMixer& getMixer() { return mixer; }
    juce::AudioProcessorValueTreeState& getMixerAPVTS() { return mixerAPVTS; }
    juce::AudioProcessorValueTreeState& getEffectsAPVTS() { return effectsAPVTS; }
    GlobalTransport& getTransport() { return transport; }
    MetronomeClick& getMetronome() { return metronome; }
    PatternEngine& getPatternEngine() { return patternEngine; }
    SceneManager& getSceneManager() { return sceneManager; }
    ArrangementTimeline& getArrangement() { return arrangement; }
    ArrangementCapture& getArrangementCapture() { return arrangementCapture; }
    MidiLearn& getMidiLearn() { return midiLearn; }

    void setActiveModule(int index) { activeModuleIndex.store(index); }
    int getActiveModule() const { return activeModuleIndex.load(); }
    void queueAllNotesOff(int moduleIndex) { pendingAllNotesOff.store(moduleIndex); }

    // ── Session file management ──────────────────────────────
    bool saveSessionToFile(const juce::File& file);
    bool loadSessionFromFile(const juce::File& file);
    void newSession();
    juce::File getCurrentSessionFile() const { return currentSessionFile; }
    void setCurrentSessionFile(const juce::File& f) { currentSessionFile = f; }
    static juce::File getSessionsDirectory();
    static juce::File getAutoSaveFile();

    // CPU usage (0.0–1.0) measured in processBlock
    float getCpuUsage() const { return cpuUsage.load(std::memory_order_relaxed); }

    // MIDI activity flags — set by audio thread, read/cleared by UI
    bool hasMidiActivity(int moduleIndex) const
    {
        if (moduleIndex >= 0 && moduleIndex < kNumModules)
            return midiActivity[static_cast<size_t>(moduleIndex)].load(std::memory_order_relaxed);
        return false;
    }
    void clearMidiActivity(int moduleIndex)
    {
        if (moduleIndex >= 0 && moduleIndex < kNumModules)
            midiActivity[static_cast<size_t>(moduleIndex)].store(false, std::memory_order_relaxed);
    }

    // Virtual keyboard state (UI-thread writes, audio-thread reads)
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }

private:
    std::atomic<int> activeModuleIndex { 0 };
    std::atomic<int> pendingAllNotesOff { -1 };
    std::atomic<float> cpuUsage { 0.0f };
    std::array<std::atomic<bool>, kNumModules> midiActivity { {false, false, false, false, false} };
    juce::MidiKeyboardState keyboardState;
    GlobalTransport transport;
    PatternEngine patternEngine;
    SceneManager sceneManager;
    ArrangementTimeline arrangement;
    ArrangementCapture arrangementCapture;
    juce::AudioProcessorValueTreeState mixerAPVTS;
    juce::AudioProcessorValueTreeState effectsAPVTS;
    MasterMixer mixer;
    ModulationMatrix modMatrix;
    PresetManager presetManager;

    jupiter8::Jupiter8Processor jupiter8;
    moog15::Moog15Processor moog15;
    jx3p::JX3PProcessor jx3p;
    dx7::DX7Processor dx7;
    linndrum::LinnDrumProcessor linnDrum;

    // Per-module MIDI buffers (live input)
    juce::MidiBuffer jup8Midi, moogMidi, jx3pMidi, dx7Midi, linnMidi;

    // Per-module MIDI buffers (output from pattern engine)
    juce::MidiBuffer jup8OutMidi, moogOutMidi, jx3pOutMidi, dx7OutMidi, linnOutMidi;

    // Per-module audio buffers
    juce::AudioBuffer<float> jup8Buffer, moogBuffer, jx3pBuffer, dx7Buffer, linnBuffer;

    // Aux send / return buffers
    juce::AudioBuffer<float> aux1Buffer, aux2Buffer, aux3Buffer, aux4Buffer, aux5Buffer;

    // Global effects bus
    dsp::PlateReverb plateReverb;
    dsp::StereoDelay stereoDelay;
    dsp::StereoChorus stereoChorus;
    dsp::StereoFlanger stereoFlanger;
    dsp::Distortion distortion;
    dsp::MasterBus   masterBus;

    bool prepared = false;
    MetronomeClick metronome;
    MidiLearn midiLearn;

    // Session file tracking
    juce::File currentSessionFile;

    // SONG mode: track which scene each module is currently playing
    std::array<int, kNumModules> songModeActiveScenes = { -1, -1, -1, -1, -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AxelFProcessor)
};

} // namespace axelf
