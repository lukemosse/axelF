#include "GlobalTransport.h"
#include <algorithm>

namespace axelf
{

void GlobalTransport::setBpm(float newBpm)
{
    bpm.store(std::clamp(newBpm, 20.0f, 300.0f));
}

void GlobalTransport::setBarCount(int bars)
{
    // Clamp to valid values: 1, 2, 4, 8, 16
    if (bars < 1) bars = 1;
    else if (bars > 16) bars = 16;
    barCount.store(bars);
}

void GlobalTransport::setTimeSignatureNumerator(int num)
{
    if (num >= 1 && num <= 16)
        timeSigNumerator.store(num);
}

void GlobalTransport::play()
{
    if (state.load() == TransportState::Stopped)
    {
        // Don't reset position — start from current cue point
        blockStartPosition = positionInBeats;
        loopResetFlag = false;

        if (countInEnabled.load())
        {
            // Start 1-bar count-in
            countInBeatsRemaining = static_cast<double>(timeSigNumerator.load());
            countInBeatAccumulator = 0.0;
            postCountInState = TransportState::Playing;
            state.store(TransportState::CountIn);
            // Fire initial downbeat tick
            if (onMetronomeTick)
                onMetronomeTick(0, true);
            return;
        }
    }
    state.store(TransportState::Playing);
}

void GlobalTransport::stop()
{
    auto prev = state.load();
    state.store(TransportState::Stopped);
    loopResetFlag = false;
    countInBeatsRemaining = 0.0;
    countInBeatAccumulator = 0.0;
    hasPendingCue.store(false);

    if (prev == TransportState::Stopped)
    {
        // Second stop press: return to beginning
        positionInBeats = 0.0;
        blockStartPosition = 0.0;
        totalBeatsElapsed = 0.0;
    }
    // First stop: keep current position (pause behavior)
}

void GlobalTransport::startRecording()
{
    if (state.load() == TransportState::Stopped)
    {
        // Don't reset position — start recording from current cue point
        blockStartPosition = positionInBeats;
        loopResetFlag = false;

        if (countInEnabled.load())
        {
            countInBeatsRemaining = static_cast<double>(timeSigNumerator.load());
            countInBeatAccumulator = 0.0;
            postCountInState = TransportState::Recording;
            state.store(TransportState::CountIn);
            if (onMetronomeTick)
                onMetronomeTick(0, true);
            return;
        }
    }
    state.store(TransportState::Recording);
}

void GlobalTransport::setCountInEnabled(bool enabled)
{
    countInEnabled.store(enabled);
}

void GlobalTransport::advance(int numSamples, double sampleRate)
{
    loopResetFlag = false;
    beatCrossFlag = false;

    auto currentState = state.load();
    if (currentState == TransportState::Stopped || sampleRate <= 0.0)
        return;

    const double currentBpm = static_cast<double>(bpm.load());
    const double beatsPerSample = currentBpm / (60.0 * sampleRate);
    const double advanceBeats = beatsPerSample * numSamples;

    // Handle count-in phase
    if (currentState == TransportState::CountIn)
    {
        blockStartPosition = positionInBeats;
        double prevAccum = countInBeatAccumulator;
        countInBeatAccumulator += advanceBeats;
        countInBeatsRemaining -= advanceBeats;

        // Fire metronome ticks on each beat boundary
        if (onMetronomeTick)
        {
            int beatsPerBar = timeSigNumerator.load();
            int prevBeat = static_cast<int>(prevAccum);
            int currBeat = static_cast<int>(countInBeatAccumulator);
            for (int b = prevBeat + 1; b <= currBeat && b < beatsPerBar; ++b)
                onMetronomeTick(b, false);
        }

        if (countInBeatsRemaining <= 0.0)
        {
            // Count-in finished — transition to target state
            positionInBeats = 0.0;
            countInBeatsRemaining = 0.0;
            state.store(postCountInState);
        }
        return;
    }

    const double patternLength = getPatternLengthInBeats();

    // Save position at the START of this block (before advancing)
    blockStartPosition = positionInBeats;

    double prevBeatPos = positionInBeats;
    positionInBeats += advanceBeats;
    totalBeatsElapsed += advanceBeats;

    // Fire metronome ticks for beat boundaries crossed during playback
    if (onMetronomeTick)
    {
        int beatsPerBar = timeSigNumerator.load();
        int prevWholeBeat = static_cast<int>(prevBeatPos);
        int currWholeBeat = static_cast<int>(positionInBeats);
        for (int b = prevWholeBeat + 1; b <= currWholeBeat; ++b)
        {
            int beatInBar = b % beatsPerBar;
            onMetronomeTick(beatInBar, beatInBar == 0);
        }
    }

    // Detect beat boundary crossing (for scene launch quantize)
    {
        int prevWholeBeat = static_cast<int>(prevBeatPos);
        int currWholeBeat = static_cast<int>(positionInBeats);
        if (currWholeBeat > prevWholeBeat)
            beatCrossFlag = true;
    }

    // Consume pending cue at bar boundary
    {
        int beatsPerBar = timeSigNumerator.load();
        int prevBar = static_cast<int>(prevBeatPos) / beatsPerBar;
        int currBar = static_cast<int>(positionInBeats) / beatsPerBar;
        if (currBar > prevBar)
        {
            double cueBeat = 0.0;
            if (consumePendingCue(cueBeat))
            {
                positionInBeats = std::max(0.0, cueBeat);
                blockStartPosition = positionInBeats;
                loopResetFlag = true;
                if (onLoopReset)
                    onLoopReset();
                return;
            }
        }
    }

    // Check for loop wrap
    if (positionInBeats >= patternLength)
    {
        positionInBeats -= patternLength;

        // Clamp to avoid drift past zero
        if (positionInBeats < 0.0)
            positionInBeats = 0.0;

        loopResetFlag = true;

        if (onLoopReset)
            onLoopReset();
    }
}

int GlobalTransport::getCurrentBar() const
{
    const int beatsPerBar = timeSigNumerator.load();
    if (beatsPerBar <= 0) return 0;
    return static_cast<int>(positionInBeats) / beatsPerBar;
}

double GlobalTransport::getBeatInBar() const
{
    const int beatsPerBar = timeSigNumerator.load();
    if (beatsPerBar <= 0) return 0.0;
    return positionInBeats - (static_cast<double>(getCurrentBar()) * beatsPerBar);
}

int GlobalTransport::getCurrentStep() const
{
    // Each beat = 4 sixteenth notes
    return static_cast<int>(positionInBeats * 4.0);
}

double GlobalTransport::getPatternLengthInBeats() const
{
    double override = patternLengthOverrideBeat.load();
    if (override > 0.0)
        return override;
    return static_cast<double>(barCount.load()) * static_cast<double>(timeSigNumerator.load());
}

void GlobalTransport::syncToHost(double hostBpm, double hostPositionInBeats, bool hostIsPlaying)
{
    bpm.store(static_cast<float>(std::clamp(hostBpm, 20.0, 300.0)));

    if (hostIsPlaying)
    {
        const double patternLength = getPatternLengthInBeats();
        if (patternLength > 0.0)
        {
            positionInBeats = std::fmod(hostPositionInBeats, patternLength);
            if (positionInBeats < 0.0)
                positionInBeats = 0.0;
        }
        if (state.load() == TransportState::Stopped)
            state.store(TransportState::Playing);
    }
    else
    {
        if (state.load() != TransportState::Stopped)
            state.store(TransportState::Stopped);
    }
}

void GlobalTransport::setSwing(float swingPercent)
{
    swing.store(std::clamp(swingPercent, 0.0f, 100.0f));
}

std::unique_ptr<juce::XmlElement> GlobalTransport::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("Transport");
    xml->setAttribute("bpm", static_cast<double>(bpm.load()));
    xml->setAttribute("barCount", barCount.load());
    xml->setAttribute("timeSigNumerator", timeSigNumerator.load());
    xml->setAttribute("swing", static_cast<double>(swing.load()));
    return xml;
}

void GlobalTransport::fromXml(const juce::XmlElement* xml)
{
    if (xml == nullptr || !xml->hasTagName("Transport"))
        return;

    setBpm(static_cast<float>(xml->getDoubleAttribute("bpm", 120.0)));
    setBarCount(xml->getIntAttribute("barCount", 4));
    setTimeSignatureNumerator(xml->getIntAttribute("timeSigNumerator", 4));
    setSwing(static_cast<float>(xml->getDoubleAttribute("swing", 50.0)));
}

} // namespace axelf
