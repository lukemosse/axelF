#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

namespace axelf
{

// ── Quantize Grid ────────────────────────────────────────────
enum class QuantizeGrid
{
    Off,
    Quarter,     // 1/4  = 1.0 beat
    Eighth,      // 1/8  = 0.5 beat
    Sixteenth,   // 1/16 = 0.25 beat
    ThirtySecond // 1/32 = 0.125 beat
};

inline double quantizeGridSize(QuantizeGrid grid)
{
    switch (grid)
    {
        case QuantizeGrid::Quarter:      return 1.0;
        case QuantizeGrid::Eighth:       return 0.5;
        case QuantizeGrid::Sixteenth:    return 0.25;
        case QuantizeGrid::ThirtySecond: return 0.125;
        default:                         return 0.0;  // Off = no quantize
    }
}

inline double quantizeBeat(double beat, QuantizeGrid grid)
{
    const double gridSize = quantizeGridSize(grid);
    if (gridSize <= 0.0)
        return beat;
    return std::round(beat / gridSize) * gridSize;
}

// ── MIDI Event (for synth patterns) ─────────────────────────
struct MidiEvent
{
    int noteNumber = 60;
    float velocity = 1.0f;       // 0.0–1.0
    double startBeat = 0.0;
    double duration = 0.25;      // in beats (default 1/16)
};

// ── SynthPattern ─────────────────────────────────────────────
// Used by Jupiter-8, Moog 15, JX-3P, DX7
class SynthPattern
{
public:
    void setBarCount(int bars)
    {
        barCount = bars;
        trimEventsToLength();
    }
    int getBarCount() const { return barCount; }

    void setBeatsPerBar(int beats) { beatsPerBar = beats; }
    int getBeatsPerBar() const { return beatsPerBar; }

    double getLengthInBeats() const
    {
        return static_cast<double>(barCount) * static_cast<double>(beatsPerBar);
    }

    void addEvent(const MidiEvent& event)
    {
        events.push_back(event);
        sortEvents();
    }

    void removeEvent(size_t index)
    {
        if (index < events.size())
            events.erase(events.begin() + static_cast<std::ptrdiff_t>(index));
    }

    void clear() { events.clear(); }

    size_t getNumEvents() const { return events.size(); }
    const MidiEvent& getEvent(size_t index) const { return events[index]; }

    // Get events whose startBeat is in [startBeat, endBeat)
    // Handles wrap-around if endBeat > patternLength
    void getEventsInRange(double startBeat, double endBeat,
                          std::vector<MidiEvent>& result) const
    {
        result.clear();
        const double patternLen = getLengthInBeats();
        if (patternLen <= 0.0 || events.empty())
            return;

        for (const auto& evt : events)
        {
            if (evt.startBeat >= startBeat && evt.startBeat < endBeat)
                result.push_back(evt);

            // Handle wrap: if endBeat > patternLen, check events at the start
            if (endBeat > patternLen)
            {
                const double wrappedEnd = endBeat - patternLen;
                if (evt.startBeat < wrappedEnd)
                    result.push_back(evt);
            }
        }
    }

    const std::vector<MidiEvent>& getEvents() const { return events; }

private:
    std::vector<MidiEvent> events;
    int barCount = 4;
    int beatsPerBar = 4;

    void sortEvents()
    {
        std::sort(events.begin(), events.end(),
                  [](const MidiEvent& a, const MidiEvent& b)
                  { return a.startBeat < b.startBeat; });
    }

    void trimEventsToLength()
    {
        const double len = getLengthInBeats();
        events.erase(
            std::remove_if(events.begin(), events.end(),
                           [len](const MidiEvent& e) { return e.startBeat >= len; }),
            events.end());
    }
};

// ── DrumPattern ──────────────────────────────────────────────
// Used by LinnDrum — step sequencer grid
class DrumPattern
{
public:
    static constexpr int kMaxTracks = 15;
    static constexpr int kStepsPerBar = 16;
    static constexpr int kMaxBars = 16;
    static constexpr int kMaxSteps = kStepsPerBar * kMaxBars;

    void setBarCount(int bars)
    {
        barCount = std::clamp(bars, 1, kMaxBars);
    }

    int getBarCount() const { return barCount; }
    int getTotalSteps() const { return barCount * kStepsPerBar; }
    int getStepsPerBar() const { return kStepsPerBar; }

    // Drum-specific loop length (1, 2, or 4 bars) — independent of global bar count
    void setLoopBars(int bars) { loopBars = std::clamp(bars, 1, barCount); }
    int getLoopBars() const { return loopBars; }
    int getLoopSteps() const { return loopBars * kStepsPerBar; }

    void setStep(int track, int step, bool active, float vel = 1.0f)
    {
        if (track >= 0 && track < kMaxTracks && step >= 0 && step < getTotalSteps())
        {
            steps[static_cast<size_t>(track)][static_cast<size_t>(step)] = active;
            velocity[static_cast<size_t>(track)][static_cast<size_t>(step)] = vel;
        }
    }

    bool getStep(int track, int step) const
    {
        if (track >= 0 && track < kMaxTracks && step >= 0 && step < getTotalSteps())
            return steps[static_cast<size_t>(track)][static_cast<size_t>(step)];
        return false;
    }

    float getVelocity(int track, int step) const
    {
        if (track >= 0 && track < kMaxTracks && step >= 0 && step < getTotalSteps())
            return velocity[static_cast<size_t>(track)][static_cast<size_t>(step)];
        return 0.0f;
    }

    void clear()
    {
        for (auto& t : steps)
            t.fill(false);
        for (auto& v : velocity)
            v.fill(1.0f);
    }

    // Get all tracks that trigger at the given step
    void getTriggersAtStep(int step, std::array<bool, kMaxTracks>& triggers,
                           std::array<float, kMaxTracks>& velocities) const
    {
        triggers.fill(false);
        velocities.fill(0.0f);
        if (step < 0 || step >= getTotalSteps())
            return;

        for (int t = 0; t < kMaxTracks; ++t)
        {
            triggers[static_cast<size_t>(t)] = steps[static_cast<size_t>(t)][static_cast<size_t>(step)];
            velocities[static_cast<size_t>(t)] = velocity[static_cast<size_t>(t)][static_cast<size_t>(step)];
        }
    }

private:
    int barCount = 4;
    int loopBars = 4;
    std::array<std::array<bool, kMaxSteps>, kMaxTracks> steps = {};
    std::array<std::array<float, kMaxSteps>, kMaxTracks> velocity = {};
};

} // namespace axelf
