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
    int startBar = 0;
    int lengthBars = 4;
};

struct ModuleLane
{
    std::vector<TimelineBlock> blocks;

    void sortBlocks()
    {
        std::sort(blocks.begin(), blocks.end(),
                  [](const TimelineBlock& a, const TimelineBlock& b)
                  { return a.startBar < b.startBar; });
    }
};

class ArrangementTimeline
{
public:
    std::array<ModuleLane, kNumModules> lanes;

    ArrangementMode getMode() const { return mode; }
    void setMode(ArrangementMode m) { mode = m; }

    int getTotalBars() const
    {
        int maxBar = 0;
        for (const auto& lane : lanes)
        {
            for (const auto& block : lane.blocks)
            {
                int end = block.startBar + block.lengthBars;
                if (end > maxBar)
                    maxBar = end;
            }
        }
        return maxBar;
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

    void moveBlock(int module, int blockIndex, int newStartBar)
    {
        if (module < 0 || module >= kNumModules) return;
        auto& blocks = lanes[static_cast<size_t>(module)].blocks;
        if (blockIndex >= 0 && blockIndex < static_cast<int>(blocks.size()))
        {
            blocks[static_cast<size_t>(blockIndex)].startBar = std::max(0, newStartBar);
            lanes[static_cast<size_t>(module)].sortBlocks();
        }
    }

    void resizeBlock(int module, int blockIndex, int newLength)
    {
        if (module < 0 || module >= kNumModules) return;
        auto& blocks = lanes[static_cast<size_t>(module)].blocks;
        if (blockIndex >= 0 && blockIndex < static_cast<int>(blocks.size()))
            blocks[static_cast<size_t>(blockIndex)].lengthBars = std::max(1, newLength);
    }

    // Returns the scene index for a module at a given bar, or -1 for silence
    int getSceneForModuleAtBar(int module, int bar) const
    {
        if (module < 0 || module >= kNumModules) return -1;
        const auto& blocks = lanes[static_cast<size_t>(module)].blocks;

        for (const auto& block : blocks)
        {
            if (bar >= block.startBar && bar < block.startBar + block.lengthBars)
                return block.sceneIndex;
        }
        return -1;
    }

    void clear()
    {
        for (auto& lane : lanes)
            lane.blocks.clear();
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
                blockXml->setAttribute("start", block.startBar);
                blockXml->setAttribute("length", block.lengthBars);
            }
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
                block.startBar = blockXml->getIntAttribute("start", 0);
                block.lengthBars = std::max(1, blockXml->getIntAttribute("length", 4));
                lanes[static_cast<size_t>(m)].blocks.push_back(block);
            }
            lanes[static_cast<size_t>(m)].sortBlocks();
        }
    }

private:
    ArrangementMode mode = ArrangementMode::Jam;
};

} // namespace axelf
