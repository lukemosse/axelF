#pragma once

#include "PatternEngine.h"
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <algorithm>

namespace axelf
{

enum class ArrangementMode
{
    Jam,
    Song
};

struct TimelineBlock
{
    int sceneIndex = 0;
    int startBeat = 0;        // Where this block starts (0-based, in beats)
    int lengthBeats = 16;     // How many beats this block spans (default = 4 bars × 4 beats)
    int trimStartBeats = 0;   // Beats trimmed from original scene start (for offset playback)
};

struct ModuleLane
{
    std::vector<TimelineBlock> blocks;

    void sortBlocks()
    {
        std::sort(blocks.begin(), blocks.end(),
                  [](const TimelineBlock& a, const TimelineBlock& b)
                  { return a.startBeat < b.startBeat; });
    }
};

// Named marker on the song timeline
struct SongMarker
{
    int beat = 0;           // Position in beats
    juce::String name;      // Short label (e.g. "Verse", "Chorus")
};

class ArrangementTimeline
{
public:
    std::array<ModuleLane, kNumModules> lanes;

    ArrangementMode getMode() const { return mode; }
    void setMode(ArrangementMode m) { mode = m; }

    int getTotalBeats() const
    {
        int maxBeat = 0;
        for (const auto& lane : lanes)
        {
            for (const auto& block : lane.blocks)
            {
                int end = block.startBeat + block.lengthBeats;
                if (end > maxBeat)
                    maxBeat = end;
            }
        }
        return maxBeat;
    }

    // Convenience: total bars (rounds up to nearest bar)
    int getTotalBars(int beatsPerBar = 4) const
    {
        int totalBeats = getTotalBeats();
        return (totalBeats + beatsPerBar - 1) / beatsPerBar;
    }

    void insertBlock(int module, const TimelineBlock& block)
    {
        if (module < 0 || module >= kNumModules) return;
        lanes[static_cast<size_t>(module)].blocks.push_back(block);
        lanes[static_cast<size_t>(module)].sortBlocks();
    }

    void removeBlock(int module, int blockIndex)
    {
        if (module < 0 || module >= kNumModules) return;
        auto& blocks = lanes[static_cast<size_t>(module)].blocks;
        if (blockIndex >= 0 && blockIndex < static_cast<int>(blocks.size()))
            blocks.erase(blocks.begin() + blockIndex);
    }

    void moveBlock(int module, int blockIndex, int newStartBeat)
    {
        if (module < 0 || module >= kNumModules) return;
        auto& blocks = lanes[static_cast<size_t>(module)].blocks;
        if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks.size())) return;

        auto& blk = blocks[static_cast<size_t>(blockIndex)];
        int len = blk.lengthBeats;
        int proposed = std::max(0, newStartBeat);

        // Clamp against previous block
        if (blockIndex > 0)
        {
            const auto& prev = blocks[static_cast<size_t>(blockIndex - 1)];
            int prevEnd = prev.startBeat + prev.lengthBeats;
            if (proposed < prevEnd)
                proposed = prevEnd;
        }
        // Clamp against next block
        if (blockIndex + 1 < static_cast<int>(blocks.size()))
        {
            int nextStart = blocks[static_cast<size_t>(blockIndex + 1)].startBeat;
            if (proposed + len > nextStart)
                proposed = nextStart - len;
        }
        proposed = std::max(0, proposed);
        blk.startBeat = proposed;
        lanes[static_cast<size_t>(module)].sortBlocks();
    }

    void resizeBlock(int module, int blockIndex, int newLengthBeats)
    {
        if (module < 0 || module >= kNumModules) return;
        auto& blocks = lanes[static_cast<size_t>(module)].blocks;
        if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks.size())) return;

        int clamped = std::max(1, newLengthBeats);

        // Clamp so we don't overlap the next block
        if (blockIndex + 1 < static_cast<int>(blocks.size()))
        {
            int nextStart = blocks[static_cast<size_t>(blockIndex + 1)].startBeat;
            int maxLen = nextStart - blocks[static_cast<size_t>(blockIndex)].startBeat;
            if (clamped > maxLen)
                clamped = maxLen;
        }
        blocks[static_cast<size_t>(blockIndex)].lengthBeats = std::max(1, clamped);
    }

    // Trim from the left: moves startBeat forward, shrinks length, increases trim offset
    void trimBlockStart(int module, int blockIndex, int newStartBeat)
    {
        if (module < 0 || module >= kNumModules) return;
        auto& blocks = lanes[static_cast<size_t>(module)].blocks;
        if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks.size())) return;

        auto& blk = blocks[static_cast<size_t>(blockIndex)];
        int proposed = std::max(0, newStartBeat);

        // Clamp: can't move past the block's own end (leave at least 1 beat)
        int blockEnd = blk.startBeat + blk.lengthBeats;
        if (proposed >= blockEnd)
            proposed = blockEnd - 1;

        // Clamp: can't move before previous block's end
        if (blockIndex > 0)
        {
            const auto& prev = blocks[static_cast<size_t>(blockIndex - 1)];
            int prevEnd = prev.startBeat + prev.lengthBeats;
            if (proposed < prevEnd)
                proposed = prevEnd;
        }

        int delta = proposed - blk.startBeat;
        if (delta == 0) return;

        blk.trimStartBeats += delta;
        if (blk.trimStartBeats < 0) blk.trimStartBeats = 0;
        blk.lengthBeats -= delta;
        blk.startBeat = proposed;
    }

    // Returns the scene index for a module at a given beat, or -1 for silence
    int getSceneForModuleAtBeat(int module, int beat) const
    {
        if (module < 0 || module >= kNumModules) return -1;
        const auto& blocks = lanes[static_cast<size_t>(module)].blocks;

        for (const auto& block : blocks)
        {
            if (beat >= block.startBeat && beat < block.startBeat + block.lengthBeats)
                return block.sceneIndex;
        }
        return -1;
    }

    // Returns scene index and the trim offset (beats into the pattern to start from).
    // outTrimBeats is only valid when return value >= 0.
    int getSceneForModuleAtBeat(int module, int beat, int& outTrimBeats) const
    {
        outTrimBeats = 0;
        if (module < 0 || module >= kNumModules) return -1;
        const auto& blocks = lanes[static_cast<size_t>(module)].blocks;

        for (const auto& block : blocks)
        {
            if (beat >= block.startBeat && beat < block.startBeat + block.lengthBeats)
            {
                outTrimBeats = block.trimStartBeats;
                return block.sceneIndex;
            }
        }
        return -1;
    }

    void clear()
    {
        for (auto& lane : lanes)
            lane.blocks.clear();
        markers.clear();
    }

    // ── Song Markers ────────────────────────────────────
    static constexpr int kMaxMarkers = 10;

    const std::vector<SongMarker>& getMarkers() const { return markers; }

    void setMarker(int slot, int beat, const juce::String& name = {})
    {
        if (slot < 0 || slot >= kMaxMarkers) return;
        // Remove existing marker at this slot
        if (slot < static_cast<int>(markers.size()))
            markers.erase(markers.begin() + slot);
        SongMarker m;
        m.beat = std::max(0, beat);
        m.name = name.isEmpty() ? juce::String("M") + juce::String(slot + 1) : name;
        if (slot <= static_cast<int>(markers.size()))
            markers.insert(markers.begin() + std::min(slot, static_cast<int>(markers.size())), m);
        else
            markers.push_back(m);
    }

    // Add or update marker at beat position (auto-assigns next available slot)
    int addMarker(int beat, const juce::String& name = {})
    {
        if (static_cast<int>(markers.size()) >= kMaxMarkers) return -1;
        SongMarker m;
        m.beat = std::max(0, beat);
        m.name = name.isEmpty()
            ? juce::String("M") + juce::String(static_cast<int>(markers.size()) + 1)
            : name;
        markers.push_back(m);
        std::sort(markers.begin(), markers.end(),
                  [](const SongMarker& a, const SongMarker& b) { return a.beat < b.beat; });
        // Return the index of the inserted marker
        for (int i = 0; i < static_cast<int>(markers.size()); ++i)
            if (markers[static_cast<size_t>(i)].beat == m.beat && markers[static_cast<size_t>(i)].name == m.name)
                return i;
        return static_cast<int>(markers.size()) - 1;
    }

    void removeMarker(int index)
    {
        if (index >= 0 && index < static_cast<int>(markers.size()))
            markers.erase(markers.begin() + index);
    }

    // Get marker beat for slot (for hotkey jump); returns -1 if no marker
    int getMarkerBeat(int slot) const
    {
        if (slot >= 0 && slot < static_cast<int>(markers.size()))
            return markers[static_cast<size_t>(slot)].beat;
        return -1;
    }

    // ── Undo / Redo ─────────────────────────────────────
    void pushUndoState()
    {
        undoStack.push_back(lanes);
        if (static_cast<int>(undoStack.size()) > kMaxUndoDepth)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
    }

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    bool undo()
    {
        if (undoStack.empty()) return false;
        redoStack.push_back(lanes);
        lanes = undoStack.back();
        undoStack.pop_back();
        return true;
    }

    bool redo()
    {
        if (redoStack.empty()) return false;
        undoStack.push_back(lanes);
        lanes = redoStack.back();
        redoStack.pop_back();
        return true;
    }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("Arrangement");
        xml->setAttribute("mode", mode == ArrangementMode::Song ? "song" : "jam");

        for (int m = 0; m < kNumModules; ++m)
        {
            const auto& lane = lanes[static_cast<size_t>(m)];
            if (lane.blocks.empty()) continue;

            auto* laneXml = xml->createNewChildElement("Lane");
            laneXml->setAttribute("module", m);

            for (const auto& block : lane.blocks)
            {
                auto* blockXml = laneXml->createNewChildElement("Block");
                blockXml->setAttribute("scene", block.sceneIndex);
                blockXml->setAttribute("startBeat", block.startBeat);
                blockXml->setAttribute("lengthBeats", block.lengthBeats);
                if (block.trimStartBeats > 0)
                    blockXml->setAttribute("trimStartBeats", block.trimStartBeats);
            }
        }

        // Serialize markers
        for (const auto& marker : markers)
        {
            auto* mxml = xml->createNewChildElement("Marker");
            mxml->setAttribute("beat", marker.beat);
            mxml->setAttribute("name", marker.name);
        }

        return xml;
    }

    void fromXml(const juce::XmlElement* xml)
    {
        if (xml == nullptr || !xml->hasTagName("Arrangement"))
            return;

        auto modeStr = xml->getStringAttribute("mode", "jam");
        mode = (modeStr == "song") ? ArrangementMode::Song : ArrangementMode::Jam;

        clear();

        for (auto* laneXml : xml->getChildWithTagNameIterator("Lane"))
        {
            int m = laneXml->getIntAttribute("module", -1);
            if (m < 0 || m >= kNumModules) continue;

            for (auto* blockXml : laneXml->getChildWithTagNameIterator("Block"))
            {
                TimelineBlock block;
                block.sceneIndex = blockXml->getIntAttribute("scene", 0);
                // Support both old (bar) and new (beat) format
                if (blockXml->hasAttribute("startBeat"))
                {
                    block.startBeat = blockXml->getIntAttribute("startBeat", 0);
                    block.lengthBeats = std::max(1, blockXml->getIntAttribute("lengthBeats", 16));
                    block.trimStartBeats = std::max(0, blockXml->getIntAttribute("trimStartBeats", 0));
                }
                else
                {
                    // Legacy: convert bars → beats (assume 4/4)
                    block.startBeat = blockXml->getIntAttribute("start", 0) * 4;
                    block.lengthBeats = std::max(1, blockXml->getIntAttribute("length", 4)) * 4;
                }
                lanes[static_cast<size_t>(m)].blocks.push_back(block);
            }
            lanes[static_cast<size_t>(m)].sortBlocks();
        }

        // Load markers
        for (auto* mxml : xml->getChildWithTagNameIterator("Marker"))
        {
            if (static_cast<int>(markers.size()) >= kMaxMarkers) break;
            SongMarker marker;
            marker.beat = mxml->getIntAttribute("beat", 0);
            marker.name = mxml->getStringAttribute("name", "M");
            markers.push_back(marker);
        }
    }

private:
    static constexpr int kMaxUndoDepth = 32;
    ArrangementMode mode = ArrangementMode::Jam;

    using LaneSnapshot = std::array<ModuleLane, kNumModules>;
    std::vector<LaneSnapshot> undoStack;
    std::vector<LaneSnapshot> redoStack;

    std::vector<SongMarker> markers;
};

} // namespace axelf
