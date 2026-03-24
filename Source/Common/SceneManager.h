#pragma once

#include "Pattern.h"
#include "PatternEngine.h"
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace axelf
{

enum class LaunchQuantize
{
    Immediate,  // Switch scenes instantly
    NextBeat,   // Switch on the next beat boundary
    NextBar     // Switch on the next bar/loop boundary (default legacy behavior)
};

struct MixerSnapshot
{
    struct Strip
    {
        float level = 1.0f;
        float pan = 0.0f;
        bool mute = false;
        bool solo = false;
        float send1 = 0.0f;
        float send2 = 0.0f;
    };
    std::array<Strip, kNumModules> strips;
};

struct Scene
{
    juce::String name = "Empty";
    std::array<SynthPattern, 5> synthPatterns;
    DrumPattern drumPattern;
    MixerSnapshot mixerState;
};

struct ChainEntry
{
    int sceneIndex = 0;
    int repeatCount = 1;
};

class SceneManager
{
public:
    static constexpr int kMaxScenes = 16;

    SceneManager();

    // Scene access
    Scene& getScene(int index);
    const Scene& getScene(int index) const;
    int getActiveSceneIndex() const { return activeScene; }

    // Save current state into a scene slot
    void saveToScene(int sceneIndex, const PatternEngine& engine, const MixerSnapshot& mixer);

    // Load a scene into the pattern engine + mixer
    void loadScene(int sceneIndex, PatternEngine& engine, MixerSnapshot& mixer);

    // Switch to a scene (saves nothing — just loads)
    void switchScene(int sceneIndex, PatternEngine& engine, MixerSnapshot& mixer);

    // Chain
    void setChain(const std::vector<ChainEntry>& newChain);
    const std::vector<ChainEntry>& getChain() const { return chain; }

    void setChainMode(bool enabled) { chainMode = enabled; chainPosition = 0; chainRepeat = 0; }
    bool isChainMode() const { return chainMode; }

    // Launch quantize
    void setLaunchQuantize(LaunchQuantize lq) { launchQuantize = lq; }
    LaunchQuantize getLaunchQuantize() const { return launchQuantize; }

    // Called on loop boundary — advances to next scene in chain if applicable
    // Returns true if scene changed
    bool advanceChain(PatternEngine& engine, MixerSnapshot& mixer);

    // Scene names
    void setSceneName(int index, const juce::String& name);
    juce::String getSceneName(int index) const;

    // Copy scene
    void copyScene(int srcIndex, int destIndex);

    // Per-module scene assignment (JAM mode)
    void setModuleScene(int moduleIndex, int sceneIndex);
    int getModuleScene(int moduleIndex) const;
    void queueModuleScene(int moduleIndex, int sceneIndex);
    int getQueuedModuleScene(int moduleIndex) const;
    bool hasQueuedScenes() const;
    void applyQueuedScenes(PatternEngine& engine);
    void loadSceneForModule(int sceneIndex, int moduleIndex, PatternEngine& engine);
    void saveModuleToScene(int sceneIndex, int moduleIndex, const PatternEngine& engine);

    // Serialization
    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml(const juce::XmlElement* xml);

private:
    std::array<Scene, kMaxScenes> scenes;
    int activeScene = 0;

    // Chain playback
    std::vector<ChainEntry> chain;
    bool chainMode = false;
    int chainPosition = 0;   // index into chain vector
    int chainRepeat = 0;     // current repeat count for current chain entry

    // Launch quantize
    LaunchQuantize launchQuantize = LaunchQuantize::NextBar;

    // Per-module JAM state
    std::array<int, kNumModules> moduleScenes = { 0, 0, 0, 0, 0, 0 };
    std::array<int, kNumModules> queuedScenes = { -1, -1, -1, -1, -1, -1 };  // -1 = no change queued
};

} // namespace axelf
