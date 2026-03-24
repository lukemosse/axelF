#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <atomic>

namespace axelf
{

enum class TransportState
{
    Stopped,
    CountIn,
    Playing,
    Recording
};

class GlobalTransport
{
public:
    void setBpm(float newBpm);
    float getBpm() const { return bpm.load(); }

    void setBarCount(int bars);
    int getBarCount() const { return barCount.load(); }

    void setTimeSignatureNumerator(int num);
    int getTimeSignatureNumerator() const { return timeSigNumerator.load(); }

    void play();
    void stop();
    void startRecording();
    TransportState getState() const { return state.load(); }
    bool isPlaying() const { auto s = state.load(); return s == TransportState::Playing || s == TransportState::Recording; }
    bool isRecording() const { return state.load() == TransportState::Recording; }
    bool isCountingIn() const { return state.load() == TransportState::CountIn; }

    // Count-in: 1 bar of metronome before recording/playing starts
    void setCountInEnabled(bool enabled);
    bool isCountInEnabled() const { return countInEnabled.load(); }
    double getCountInBeatsRemaining() const { return countInBeatsRemaining; }

    // Call at the top of every processBlock
    void advance(int numSamples, double sampleRate);

    // Position queries
    double getPositionInBeats() const { return positionInBeats; }
    double getBlockStartBeat() const { return blockStartPosition; }
    int getCurrentBar() const;       // 0-based
    double getBeatInBar() const;
    int getCurrentStep() const;      // 16th note index within the full pattern (0-based)
    double getPatternLengthInBeats() const;

    // Returns true on the sample where the pattern wraps back to 0
    bool didLoopReset() const { return loopResetFlag; }

    // Returns true when a beat boundary was crossed in this block
    bool didBeatCross() const { return beatCrossFlag; }

    // Monotonically increasing beat counter (never resets on loop)
    double getTotalBeatsElapsed() const { return totalBeatsElapsed; }

    // Callback fired on loop reset (called from audio thread — keep fast)
    std::function<void()> onLoopReset;

    // Callback for metronome clicks (beat number 0-based within bar, isDownbeat)
    std::function<void(int beatInBar, bool isDownbeat)> onMetronomeTick;

    // Host sync: call from processBlock if host provides position info
    void syncToHost(double hostBpm, double hostPositionInBeats, bool hostIsPlaying);

    void setSwing(float swingPercent);
    float getSwing() const { return swing.load(); }

    // Override pattern length (e.g. for Song mode arrangement total length)
    // Set to 0 to revert to default barCount * timeSigNumerator
    void setPatternLengthOverride(double beats) { patternLengthOverrideBeat.store(beats); }
    double getPatternLengthOverride() const { return patternLengthOverrideBeat.load(); }

    // Serialization
    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml(const juce::XmlElement* xml);

private:
    std::atomic<float> bpm { 120.0f };
    std::atomic<int> barCount { 4 };
    std::atomic<int> timeSigNumerator { 4 };
    std::atomic<float> swing { 50.0f };  // 50 = straight
    std::atomic<double> patternLengthOverrideBeat { 0.0 };
    std::atomic<TransportState> state { TransportState::Stopped };

    double positionInBeats = 0.0;
    double blockStartPosition = 0.0;  // position before advance() — start of current audio block
    bool loopResetFlag = false;
    bool beatCrossFlag = false;
    double totalBeatsElapsed = 0.0;

    // Count-in state
    std::atomic<bool> countInEnabled { false };
    double countInBeatsRemaining = 0.0;
    double countInBeatAccumulator = 0.0;  // tracks fractional beat for metronome ticks
    TransportState postCountInState = TransportState::Playing;  // state to enter after count-in
};

} // namespace axelf
