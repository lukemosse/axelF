#pragma once

#include "ArrangementTimeline.h"
#include <vector>
#include <cmath>
#include <atomic>

namespace axelf
{

class ArrangementCapture
{
public:
    void startCapture(double currentBeat, float bpm, int beatsPerBar)
    {
        capturingFlag.store(true);
        captureStartBeat = currentBeat;
        captureEndBeat = currentBeat;
        captureBpm = bpm;
        captureBeatsPerBar = beatsPerBar;
        events.clear();

        // Record the initial state for all modules (set by caller)
    }

    void recordSceneChange(int module, int sceneIndex, double beat)
    {
        if (!capturingFlag.load()) return;
        events.push_back({ module, sceneIndex, beat });
    }

    void recordInitialState(int module, int sceneIndex)
    {
        if (!capturingFlag.load()) return;
        events.push_back({ module, sceneIndex, captureStartBeat });
    }

    void stopCapture(double endBeat)
    {
        capturingFlag.store(false);
        captureEndBeat = endBeat;
    }

    bool isCapturing() const { return capturingFlag.load(); }
    double getCaptureStartBeat() const { return captureStartBeat; }

    // Get a live snapshot of the arrangement-in-progress (for display while recording)
    ArrangementTimeline getLiveTimeline(double currentBeat) const
    {
        ArrangementTimeline timeline;
        timeline.setMode(ArrangementMode::Song);

        if (events.empty() || captureBeatsPerBar <= 0)
            return timeline;

        const double beatsPerBarD = static_cast<double>(captureBeatsPerBar);
        const double totalBeats = currentBeat - captureStartBeat;
        const int totalBars = std::max(1, static_cast<int>(std::ceil(totalBeats / beatsPerBarD)));

        for (int m = 0; m < kNumModules; ++m)
        {
            std::vector<CaptureEvent> moduleEvents;
            for (const auto& evt : events)
            {
                if (evt.module == m)
                    moduleEvents.push_back(evt);
            }
            if (moduleEvents.empty()) continue;

            std::sort(moduleEvents.begin(), moduleEvents.end(),
                      [](const CaptureEvent& a, const CaptureEvent& b)
                      { return a.startBeat < b.startBeat; });

            auto dedup = std::unique(moduleEvents.begin(), moduleEvents.end(),
                                     [](const CaptureEvent& a, const CaptureEvent& b)
                                     { return a.sceneIndex == b.sceneIndex; });
            moduleEvents.erase(dedup, moduleEvents.end());

            for (size_t i = 0; i < moduleEvents.size(); ++i)
            {
                const auto& evt = moduleEvents[i];
                double relBeat = evt.startBeat - captureStartBeat;
                int startBar = std::max(0, static_cast<int>(std::floor(relBeat / beatsPerBarD)));

                int endBar;
                if (i + 1 < moduleEvents.size())
                {
                    double nextRelBeat = moduleEvents[i + 1].startBeat - captureStartBeat;
                    endBar = static_cast<int>(std::floor(nextRelBeat / beatsPerBarD));
                    if (endBar <= startBar) endBar = startBar + 1;
                }
                else
                {
                    endBar = totalBars;
                }

                TimelineBlock block;
                block.sceneIndex = evt.sceneIndex;
                block.startBar = startBar;
                block.lengthBars = std::max(1, endBar - startBar);
                timeline.insertBlock(m, block);
            }
        }
        return timeline;
    }

    ArrangementTimeline convertToTimeline() const
    {
        ArrangementTimeline timeline;
        timeline.setMode(ArrangementMode::Song);

        if (events.empty() || captureBeatsPerBar <= 0)
            return timeline;

        const double beatsPerBarD = static_cast<double>(captureBeatsPerBar);
        const double totalBeats = captureEndBeat - captureStartBeat;
        const int totalBars = std::max(1, static_cast<int>(std::ceil(totalBeats / beatsPerBarD)));

        // For each module, find the sequence of scene changes
        for (int m = 0; m < kNumModules; ++m)
        {
            // Collect events for this module, sorted by beat
            std::vector<CaptureEvent> moduleEvents;
            for (const auto& evt : events)
            {
                if (evt.module == m)
                    moduleEvents.push_back(evt);
            }

            if (moduleEvents.empty()) continue;

            // Sort by beat
            std::sort(moduleEvents.begin(), moduleEvents.end(),
                      [](const CaptureEvent& a, const CaptureEvent& b)
                      { return a.startBeat < b.startBeat; });

            // Deduplicate consecutive events with the same scene
            auto dedup = std::unique(moduleEvents.begin(), moduleEvents.end(),
                                     [](const CaptureEvent& a, const CaptureEvent& b)
                                     { return a.sceneIndex == b.sceneIndex; });
            moduleEvents.erase(dedup, moduleEvents.end());

            // Convert to gap-free timeline blocks using floor() for start, ceil()/next for end
            for (size_t i = 0; i < moduleEvents.size(); ++i)
            {
                const auto& evt = moduleEvents[i];
                double relBeat = evt.startBeat - captureStartBeat;
                int startBar = std::max(0, static_cast<int>(std::floor(relBeat / beatsPerBarD)));

                // End bar: use floor of next event's beat (so blocks tile exactly)
                int endBar;
                if (i + 1 < moduleEvents.size())
                {
                    double nextRelBeat = moduleEvents[i + 1].startBeat - captureStartBeat;
                    endBar = static_cast<int>(std::floor(nextRelBeat / beatsPerBarD));
                    // If next event lands on same bar, push end to at least startBar + 1
                    if (endBar <= startBar)
                        endBar = startBar + 1;
                }
                else
                {
                    endBar = totalBars;
                }

                int lengthBars = std::max(1, endBar - startBar);

                TimelineBlock block;
                block.sceneIndex = evt.sceneIndex;
                block.startBar = startBar;
                block.lengthBars = lengthBars;
                timeline.insertBlock(m, block);
            }
        }

        return timeline;
    }

private:
    struct CaptureEvent
    {
        int module;
        int sceneIndex;
        double startBeat;
    };

    std::atomic<bool> capturingFlag { false };
    double captureStartBeat = 0.0;
    double captureEndBeat = 0.0;
    float captureBpm = 120.0f;
    int captureBeatsPerBar = 4;
    std::vector<CaptureEvent> events;
};

} // namespace axelf
