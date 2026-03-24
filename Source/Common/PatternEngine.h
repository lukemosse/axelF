#pragma once

#include "GlobalTransport.h"
#include "Pattern.h"
#include "PatternUndoManager.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace axelf
{

// Indices for the 5 modules
enum ModuleIndex
{
    kJupiter8 = 0,
    kMoog15   = 1,
    kJX3P     = 2,
    kDX7      = 3,
    kPPGWave  = 4,
    kLinnDrum = 5,
    kNumModules = 6
};

enum class RecordMode
{
    Off,
    Overdub,  // Add new notes on top of existing
    Replace   // Clear pattern on record start, record fresh
};

class PatternEngine
{
public:
    PatternEngine();

    // Pattern access
    SynthPattern& getSynthPattern(int moduleIndex);
    const SynthPattern& getSynthPattern(int moduleIndex) const;
    DrumPattern& getDrumPattern() { return drumPattern; }
    const DrumPattern& getDrumPattern() const { return drumPattern; }

    // Record arming
    void setRecordArm(int moduleIndex, bool armed);
    bool isRecordArmed(int moduleIndex) const;

    void setRecordMode(int moduleIndex, RecordMode mode);
    RecordMode getRecordMode(int moduleIndex) const;

    // Quantize
    void setQuantizeGrid(int moduleIndex, QuantizeGrid grid);
    QuantizeGrid getQuantizeGrid(int moduleIndex) const;

    // Per-module mute (mutes pattern playback, not live MIDI)
    void setPatternMute(int moduleIndex, bool muted);
    bool isPatternMuted(int moduleIndex) const;

    // Main process call — called from PluginProcessor::processBlock
    // liveMidi[i]  = incoming MIDI for module i (from MidiRouter or active-tab routing)
    // outputMidi[i] = final MIDI to feed into module i's processBlock (pattern + live merged)
    void processBlock(GlobalTransport& transport,
                      std::array<juce::MidiBuffer*, kNumModules>& liveMidi,
                      std::array<juce::MidiBuffer*, kNumModules>& outputMidi,
                      int numSamples,
                      double sampleRate);

    // Pattern clipboard operations
    void copyPattern(int moduleIndex);
    void pastePattern(int moduleIndex);
    void clearPattern(int moduleIndex);
    bool hasClipboard() const { return clipboardValid; }

    // Per-module swing (50 = straight, >50 = swung)
    void setSwing(int moduleIndex, float swingPercent);
    float getSwing(int moduleIndex) const;

    // Undo / Redo
    bool undo(int moduleIndex);
    bool redo(int moduleIndex);
    bool canUndo(int moduleIndex) const { return undoManager.canUndo(moduleIndex); }
    bool canRedo(int moduleIndex) const { return undoManager.canRedo(moduleIndex); }

    // Synchronise bar counts from transport
    void syncBarCounts(int bars, int beatsPerBar);

    // Notify that a scene was just loaded for a module (flushes pending note-offs)
    void notifySceneChanged(int moduleIndex);

    // Serialization
    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml(const juce::XmlElement* xml);

    // Note map for drum trigger generation (matches LinnDrum GM mapping)
    static constexpr int kDrumNoteMap[15] = {
        36, 38, 42, 46, 51, 49, 50, 47, 45, 62, 63, 39, 56, 54, 69
    };

private:
    std::array<SynthPattern, 5> synthPatterns;
    DrumPattern drumPattern;

    std::array<bool, kNumModules> recordArmed = {};
    std::array<RecordMode, kNumModules> recordModes = {};
    std::array<QuantizeGrid, kNumModules> quantizeGrids = {};
    std::array<bool, kNumModules> patternMuted = {};

    // Per-module swing (50 = straight)
    std::array<float, kNumModules> swingPercent = { 50.f, 50.f, 50.f, 50.f, 50.f, 50.f };

    // Scene-change flush flag (set from outside, consumed in processSynthPlayback)
    std::array<bool, kNumModules> sceneJustChanged = {};

    // Pattern clipboard (can hold one synth pattern or one drum pattern)
    bool clipboardValid = false;
    bool clipboardIsDrum = false;
    SynthPattern clipboardSynth;
    DrumPattern clipboardDrum;

    PatternUndoManager undoManager;

    // Track whether we've pushed an undo snapshot for the current recording pass
    std::array<bool, kNumModules> recordUndoPushed = {};

    // Tracking active note-ons for recording (to compute duration at note-off)
    struct ActiveNote
    {
        int moduleIndex;
        int noteNumber;
        float velocity;
        double startBeat;
    };
    std::vector<ActiveNote> activeRecordNotes;

    // Pending note-offs for synth playback (notes whose off-time is in a future block)
    struct PendingNoteOff
    {
        int moduleIndex;
        int noteNumber;
        double offBeat;  // absolute beat position for note-off
    };
    std::vector<PendingNoteOff> pendingNoteOffs;

    // Previous position for detecting step transitions (drum pattern)
    int previousStep = -1;

    void processSynthPlayback(int moduleIndex, GlobalTransport& transport,
                              juce::MidiBuffer& outputMidi,
                              int numSamples, double sampleRate);

    void processDrumPlayback(GlobalTransport& transport,
                             juce::MidiBuffer& outputMidi,
                             int numSamples, double sampleRate);

    void processRecording(int moduleIndex, GlobalTransport& transport,
                          const juce::MidiBuffer& liveMidi,
                          double sampleRate, int numSamples);
};

} // namespace axelf
