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
                int startBeat = std::max(0, static_cast<int>(std::floor(relBeat)));

                int endBeat;
                if (i + 1 < moduleEvents.size())
                {
                    double nextRelBeat = moduleEvents[i + 1].startBeat - captureStartBeat;
                    endBeat = static_cast<int>(std::floor(nextRelBeat));
                    if (endBeat <= startBeat) endBeat = startBeat + 1;
                }
                else
                {
                    endBeat = totalBars * captureBeatsPerBar;
                }

                TimelineBlock block;
                block.sceneIndex = evt.sceneIndex;
                block.startBeat = startBeat;
                block.lengthBeats = std::max(1, endBeat - startBeat);
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

            for (size_t i = 0; i < moduleEvents.size(); ++i)
            {
                const auto& evt = moduleEvents[i];
                double relBeat = evt.startBeat - captureStartBeat;
                int startBeat = std::max(0, static_cast<int>(std::floor(relBeat)));

                // End beat: use floor of next event's beat (so blocks tile exactly)
                int endBeat;
                if (i + 1 < moduleEvents.size())
                {
                    double nextRelBeat = moduleEvents[i + 1].startBeat - captureStartBeat;
                    endBeat = static_cast<int>(std::floor(nextRelBeat));
                    // If next event lands on same beat, push end to at least startBeat + 1
                    if (endBeat <= startBeat)
                        endBeat = startBeat + 1;
                }
                else
                {
                    endBeat = totalBars * captureBeatsPerBar;
                }

                int lengthBeats = std::max(1, endBeat - startBeat);

                TimelineBlock block;
                block.sceneIndex = evt.sceneIndex;
                block.startBeat = startBeat;
                block.lengthBeats = lengthBeats;
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
