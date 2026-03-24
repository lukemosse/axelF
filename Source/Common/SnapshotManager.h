#pragma once

#include "Pattern.h"
#include "PatternEngine.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

namespace axelf
{

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

struct Snapshot
{
    juce::String name = "Empty";
    std::array<SynthPattern, 4> synthPatterns;
    DrumPattern drumPattern;
    MixerSnapshot mixerState;
};

class SnapshotManager
{
public:
    SnapshotManager() { for(int i=0;i<16;++i) addSnapshot(); }

    std::vector<Snapshot>& getSnapshots() { return snapshots; }
    const std::vector<Snapshot>& getSnapshots() const { return snapshots; }

    Snapshot& getSnapshot(int index);
    const Snapshot& getSnapshot(int index) const;

    void addSnapshot();
    void removeSnapshot(int index);
    int getSnapshotCount() const { return static_cast<int>(snapshots.size()); }

    int getActiveSnapshotIndex() const { return activeSnapshotIndex; }

    void saveToSnapshot(int index, const PatternEngine& engine, const MixerSnapshot& mixer);
    void loadSnapshot(int index, PatternEngine& engine, MixerSnapshot& mixer);
    
    void setSnapshotName(int index, const juce::String& name);
    juce::String getSnapshotName(int index) const;

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml(const juce::XmlElement* xml);

    bool saveSnapshotToFile(int index, const juce::File& file) const;
    bool loadSnapshotFromFile(const juce::File& file);

private:
    std::vector<Snapshot> snapshots;
    int activeSnapshotIndex = 0;
};

} // namespace axelf
