#include "PatternEngine.h"

namespace axelf
{

PatternEngine::PatternEngine()
{
    recordArmed.fill(false);
    recordModes.fill(RecordMode::Overdub);
    quantizeGrids.fill(QuantizeGrid::Sixteenth);
    patternMuted.fill(false);
}

SynthPattern& PatternEngine::getSynthPattern(int moduleIndex)
{
    return synthPatterns[static_cast<size_t>(std::clamp(moduleIndex, 0, 4))];
}

const SynthPattern& PatternEngine::getSynthPattern(int moduleIndex) const
{
    return synthPatterns[static_cast<size_t>(std::clamp(moduleIndex, 0, 4))];
}

void PatternEngine::setRecordArm(int moduleIndex, bool armed)
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        recordArmed[static_cast<size_t>(moduleIndex)] = armed;
}

bool PatternEngine::isRecordArmed(int moduleIndex) const
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        return recordArmed[static_cast<size_t>(moduleIndex)];
    return false;
}

void PatternEngine::setRecordMode(int moduleIndex, RecordMode mode)
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        recordModes[static_cast<size_t>(moduleIndex)] = mode;
}

RecordMode PatternEngine::getRecordMode(int moduleIndex) const
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        return recordModes[static_cast<size_t>(moduleIndex)];
    return RecordMode::Off;
}

void PatternEngine::setQuantizeGrid(int moduleIndex, QuantizeGrid grid)
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        quantizeGrids[static_cast<size_t>(moduleIndex)] = grid;
}

QuantizeGrid PatternEngine::getQuantizeGrid(int moduleIndex) const
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        return quantizeGrids[static_cast<size_t>(moduleIndex)];
    return QuantizeGrid::Off;
}

void PatternEngine::setPatternMute(int moduleIndex, bool muted)
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        patternMuted[static_cast<size_t>(moduleIndex)] = muted;
}

bool PatternEngine::isPatternMuted(int moduleIndex) const
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        return patternMuted[static_cast<size_t>(moduleIndex)];
    return false;
}

void PatternEngine::setSwing(int moduleIndex, float swing)
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        swingPercent[static_cast<size_t>(moduleIndex)] = std::clamp(swing, 50.0f, 75.0f);
}

float PatternEngine::getSwing(int moduleIndex) const
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        return swingPercent[static_cast<size_t>(moduleIndex)];
    return 50.0f;
}

void PatternEngine::copyPattern(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= kNumModules) return;

    if (moduleIndex == kLinnDrum)
    {
        clipboardDrum = drumPattern;
        clipboardIsDrum = true;
    }
    else
    {
        clipboardSynth = synthPatterns[static_cast<size_t>(moduleIndex)];
        clipboardIsDrum = false;
    }
    clipboardValid = true;
}

void PatternEngine::pastePattern(int moduleIndex)
{
    if (!clipboardValid) return;
    if (moduleIndex < 0 || moduleIndex >= kNumModules) return;

    if (moduleIndex == kLinnDrum)
    {
        if (clipboardIsDrum)
        {
            undoManager.pushDrum(drumPattern);
            drumPattern = clipboardDrum;
        }
    }
    else
    {
        if (!clipboardIsDrum)
        {
            undoManager.pushSynth(moduleIndex, synthPatterns[static_cast<size_t>(moduleIndex)]);
            synthPatterns[static_cast<size_t>(moduleIndex)] = clipboardSynth;
        }
    }
}

void PatternEngine::clearPattern(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= kNumModules) return;

    if (moduleIndex == kLinnDrum)
    {
        undoManager.pushDrum(drumPattern);
        drumPattern.clear();
    }
    else
    {
        undoManager.pushSynth(moduleIndex, synthPatterns[static_cast<size_t>(moduleIndex)]);
        synthPatterns[static_cast<size_t>(moduleIndex)].clear();
    }
}

bool PatternEngine::undo(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= kNumModules) return false;

    if (moduleIndex == kLinnDrum)
        return undoManager.undoDrum(drumPattern);
    else
        return undoManager.undoSynth(moduleIndex, synthPatterns[static_cast<size_t>(moduleIndex)]);
}

bool PatternEngine::redo(int moduleIndex)
{
    if (moduleIndex < 0 || moduleIndex >= kNumModules) return false;

    if (moduleIndex == kLinnDrum)
        return undoManager.redoDrum(drumPattern);
    else
        return undoManager.redoSynth(moduleIndex, synthPatterns[static_cast<size_t>(moduleIndex)]);
}

void PatternEngine::syncBarCounts(int bars, int beatsPerBar)
{
    for (auto& sp : synthPatterns)
    {
        sp.setBarCount(bars);
        sp.setBeatsPerBar(beatsPerBar);
    }
    drumPattern.setBarCount(bars);
}

void PatternEngine::notifySceneChanged(int moduleIndex)
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        sceneJustChanged[static_cast<size_t>(moduleIndex)] = true;
}

std::unique_ptr<juce::XmlElement> PatternEngine::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("PatternEngine");

    // Synth patterns
    for (int m = 0; m < 5; ++m)
    {
        const auto& sp = synthPatterns[static_cast<size_t>(m)];
        auto* spXml = xml->createNewChildElement("SynthPattern");
        spXml->setAttribute("module", m);
        spXml->setAttribute("barCount", sp.getBarCount());
        spXml->setAttribute("beatsPerBar", sp.getBeatsPerBar());
        spXml->setAttribute("loopBars", sp.getLoopBars());

        for (size_t e = 0; e < sp.getNumEvents(); ++e)
        {
            const auto& evt = sp.getEvent(e);
            auto* evtXml = spXml->createNewChildElement("Event");
            evtXml->setAttribute("note", evt.noteNumber);
            evtXml->setAttribute("vel", static_cast<double>(evt.velocity));
            evtXml->setAttribute("start", evt.startBeat);
            evtXml->setAttribute("dur", evt.duration);
        }
    }

    // Drum pattern
    {
        auto* dpXml = xml->createNewChildElement("DrumPattern");
        dpXml->setAttribute("barCount", drumPattern.getBarCount());
        dpXml->setAttribute("loopBars", drumPattern.getLoopBars());

        const int totalSteps = drumPattern.getTotalSteps();
        for (int t = 0; t < DrumPattern::kMaxTracks; ++t)
        {
            auto* trackXml = dpXml->createNewChildElement("Track");
            trackXml->setAttribute("index", t);

            juce::String stepStr;
            juce::String velStr;
            for (int s = 0; s < totalSteps; ++s)
            {
                stepStr += drumPattern.getStep(t, s) ? "1" : "0";
                if (s > 0) velStr += ",";
                velStr += juce::String(drumPattern.getVelocity(t, s), 3);
            }
            trackXml->setAttribute("steps", stepStr);
            trackXml->setAttribute("velocities", velStr);
        }
    }

    // Swing
    {
        auto* swXml = xml->createNewChildElement("Swing");
        juce::String vals;
        for (int i = 0; i < kNumModules; ++i)
        {
            if (i > 0) vals += ",";
            vals += juce::String(swingPercent[static_cast<size_t>(i)], 1);
        }
        swXml->setAttribute("values", vals);
    }

    // Quantize grids
    {
        auto* qXml = xml->createNewChildElement("Quantize");
        juce::String vals;
        for (int i = 0; i < kNumModules; ++i)
        {
            if (i > 0) vals += ",";
            vals += juce::String(static_cast<int>(quantizeGrids[static_cast<size_t>(i)]));
        }
        qXml->setAttribute("values", vals);
    }

    return xml;
}

void PatternEngine::fromXml(const juce::XmlElement* xml)
{
    if (xml == nullptr || !xml->hasTagName("PatternEngine"))
        return;

    // Synth patterns
    for (auto* spXml : xml->getChildWithTagNameIterator("SynthPattern"))
    {
        int m = spXml->getIntAttribute("module", -1);
        if (m < 0 || m >= 5) continue;

        auto& sp = synthPatterns[static_cast<size_t>(m)];
        sp.clear();
        sp.setBarCount(spXml->getIntAttribute("barCount", 4));
        sp.setBeatsPerBar(spXml->getIntAttribute("beatsPerBar", 4));
        sp.setLoopBars(spXml->getIntAttribute("loopBars", 4));

        for (auto* evtXml : spXml->getChildWithTagNameIterator("Event"))
        {
            MidiEvent evt;
            evt.noteNumber = evtXml->getIntAttribute("note", 60);
            evt.velocity = static_cast<float>(evtXml->getDoubleAttribute("vel", 1.0));
            evt.startBeat = evtXml->getDoubleAttribute("start", 0.0);
            evt.duration = evtXml->getDoubleAttribute("dur", 0.25);
            sp.addEvent(evt);
        }
    }

    // Drum pattern
    if (auto* dpXml = xml->getChildByName("DrumPattern"))
    {
        drumPattern.clear();
        drumPattern.setBarCount(dpXml->getIntAttribute("barCount", 4));
        drumPattern.setLoopBars(dpXml->getIntAttribute("loopBars", 4));

        for (auto* trackXml : dpXml->getChildWithTagNameIterator("Track"))
        {
            int t = trackXml->getIntAttribute("index", -1);
            if (t < 0 || t >= DrumPattern::kMaxTracks) continue;

            auto stepStr = trackXml->getStringAttribute("steps");
            auto velStr = trackXml->getStringAttribute("velocities");
            juce::StringArray velTokens;
            velTokens.addTokens(velStr, ",", "");

            for (int s = 0; s < stepStr.length() && s < drumPattern.getTotalSteps(); ++s)
            {
                bool active = stepStr[s] == '1';
                float vel = (s < velTokens.size()) ? velTokens[s].getFloatValue() : 1.0f;
                drumPattern.setStep(t, s, active, vel);
            }
        }
    }

    // Swing
    if (auto* swXml = xml->getChildByName("Swing"))
    {
        auto vals = swXml->getStringAttribute("values");
        juce::StringArray tokens;
        tokens.addTokens(vals, ",", "");
        for (int i = 0; i < std::min(tokens.size(), static_cast<int>(kNumModules)); ++i)
            swingPercent[static_cast<size_t>(i)] = std::clamp(tokens[i].getFloatValue(), 50.0f, 75.0f);
    }

    // Quantize grids
    if (auto* qXml = xml->getChildByName("Quantize"))
    {
        auto vals = qXml->getStringAttribute("values");
        juce::StringArray tokens;
        tokens.addTokens(vals, ",", "");
        for (int i = 0; i < std::min(tokens.size(), static_cast<int>(kNumModules)); ++i)
        {
            int v = tokens[i].getIntValue();
            if (v >= 0 && v <= static_cast<int>(QuantizeGrid::ThirtySecond))
                quantizeGrids[static_cast<size_t>(i)] = static_cast<QuantizeGrid>(v);
        }
    }
}

void PatternEngine::processBlock(GlobalTransport& transport,
                                  std::array<juce::MidiBuffer*, kNumModules>& liveMidi,
                                  std::array<juce::MidiBuffer*, kNumModules>& outputMidi,
                                  int numSamples,
                                  double sampleRate)
{
    // If transport just stopped, flush all pending note-offs immediately
    if (!transport.isPlaying() && !pendingNoteOffs.empty())
    {
        for (int i = 0; i < kNumModules; ++i)
        {
            outputMidi[static_cast<size_t>(i)]->clear();
            for (const auto& pno : pendingNoteOffs)
            {
                if (pno.moduleIndex == i)
                    outputMidi[static_cast<size_t>(i)]->addEvent(
                        juce::MidiMessage::noteOff(1, pno.noteNumber), 0);
            }
        }
        pendingNoteOffs.clear();

        // Still pass through live MIDI
        for (int i = 0; i < kNumModules; ++i)
            for (const auto metadata : *liveMidi[static_cast<size_t>(i)])
                outputMidi[static_cast<size_t>(i)]->addEvent(metadata.getMessage(),
                                                              metadata.samplePosition);
        return;
    }

    // For each module: record live MIDI into pattern, then generate playback MIDI
    for (int i = 0; i < kNumModules; ++i)
    {
        auto idx = static_cast<size_t>(i);
        outputMidi[idx]->clear();

        // 1. If recording, capture live MIDI into pattern
        if (transport.isRecording() && recordArmed[idx])
        {
            processRecording(i, transport, *liveMidi[idx], sampleRate, numSamples);
        }
        else
        {
            // Reset undo-push flag when not recording
            recordUndoPushed[idx] = false;

            // Flush any orphaned active notes (note-on received but no note-off before recording stopped)
            for (auto it = activeRecordNotes.begin(); it != activeRecordNotes.end(); )
            {
                if (it->moduleIndex == i)
                {
                    double loopLen = synthPatterns[idx].getLoopLengthInBeats();
                    double duration = std::min(1.0, loopLen > 0.0 ? loopLen * 0.25 : 1.0);
                    MidiEvent evt;
                    evt.noteNumber = it->noteNumber;
                    evt.velocity   = it->velocity;
                    evt.startBeat  = it->startBeat;
                    evt.duration   = duration;
                    synthPatterns[idx].addEvent(evt);
                    it = activeRecordNotes.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // 2. Generate pattern playback MIDI (if transport is playing and not muted)
        if (transport.isPlaying() && !patternMuted[idx])
        {
            if (i == kLinnDrum)
                processDrumPlayback(transport, *outputMidi[idx], numSamples, sampleRate);
            else
                processSynthPlayback(i, transport, *outputMidi[idx], numSamples, sampleRate);
        }
        else if (patternMuted[idx])
        {
            // Flush pending note-offs so muting doesn't leave hanging notes
            for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end(); )
            {
                if (it->moduleIndex == i)
                {
                    outputMidi[idx]->addEvent(juce::MidiMessage::noteOff(1, it->noteNumber), 0);
                    it = pendingNoteOffs.erase(it);
                }
                else
                    ++it;
            }
        }

        // 3. Merge live MIDI passthrough (always hear what you play)
        for (const auto metadata : *liveMidi[idx])
            outputMidi[idx]->addEvent(metadata.getMessage(), metadata.samplePosition);
    }
}

void PatternEngine::processSynthPlayback(int moduleIndex, GlobalTransport& transport,
                                          juce::MidiBuffer& outputMidi,
                                          int numSamples, double sampleRate)
{
    const auto& pattern = synthPatterns[static_cast<size_t>(moduleIndex)];
    const double bpm = static_cast<double>(transport.getBpm());
    const double beatsPerSample = bpm / (60.0 * sampleRate);
    const double blockLengthBeats = beatsPerSample * numSamples;
    const double patternLen = pattern.getLoopLengthInBeats();

    // Wrap transport position to pattern's own cycle length.
    // In Song mode the transport spans the full arrangement, but patterns
    // are shorter and must loop within their own length.
    double blockStartBeat = transport.getBlockStartBeat();
    if (patternLen > 0.0)
    {
        blockStartBeat = std::fmod(blockStartBeat, patternLen);
        if (blockStartBeat < 0.0) blockStartBeat += patternLen;
    }
    const double endBeat = blockStartBeat + blockLengthBeats;

    // Flush pending note-offs when a scene just changed (prevents stuck notes)
    if (sceneJustChanged[static_cast<size_t>(moduleIndex)])
    {
        sceneJustChanged[static_cast<size_t>(moduleIndex)] = false;
        for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end(); )
        {
            if (it->moduleIndex == moduleIndex)
            {
                outputMidi.addEvent(juce::MidiMessage::noteOff(1, it->noteNumber), 0);
                it = pendingNoteOffs.erase(it);
            }
            else
                ++it;
        }
    }

    // At loop boundary, flush ALL pending note-offs for this module
    // to prevent stuck notes carrying over across loop iterations
    if (transport.didLoopReset())
    {
        for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end(); )
        {
            if (it->moduleIndex == moduleIndex)
            {
                outputMidi.addEvent(juce::MidiMessage::noteOff(1, it->noteNumber), 0);
                it = pendingNoteOffs.erase(it);
            }
            else
                ++it;
        }
    }

    // Process pending note-offs from previous blocks
    for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end(); )
    {
        if (it->moduleIndex == moduleIndex)
        {
            double offBeat = it->offBeat;
            // Wrap for pattern comparison
            if (patternLen > 0.0)
                while (offBeat < blockStartBeat)
                    offBeat += patternLen;

            if (offBeat >= blockStartBeat && offBeat < endBeat)
            {
                int sampleOffset = static_cast<int>((offBeat - blockStartBeat) / beatsPerSample);
                sampleOffset = std::clamp(sampleOffset, 0, numSamples - 1);
                outputMidi.addEvent(juce::MidiMessage::noteOff(1, it->noteNumber), sampleOffset);
                it = pendingNoteOffs.erase(it);
                continue;
            }
        }
        ++it;
    }

    if (pattern.getNumEvents() == 0)
        return;

    std::vector<MidiEvent> eventsInRange;
    pattern.getEventsInRange(blockStartBeat, endBeat, eventsInRange);

    for (const auto& evt : eventsInRange)
    {
        // Calculate sample offset within this block
        double eventBeat = evt.startBeat;
        if (eventBeat < blockStartBeat && patternLen > 0.0)
            eventBeat += patternLen;  // wrap-around event

        double offsetBeats = eventBeat - blockStartBeat;
        int sampleOffset = static_cast<int>(offsetBeats / beatsPerSample);
        sampleOffset = std::clamp(sampleOffset, 0, numSamples - 1);

        auto noteOn = juce::MidiMessage::noteOn(
            1, evt.noteNumber, evt.velocity);
        outputMidi.addEvent(noteOn, sampleOffset);

        // Schedule note-off
        double noteOffBeat = eventBeat + evt.duration;
        double durationSamples = evt.duration / beatsPerSample;
        int noteOffSample = sampleOffset + static_cast<int>(durationSamples);

        if (noteOffSample < numSamples)
        {
            outputMidi.addEvent(juce::MidiMessage::noteOff(1, evt.noteNumber), noteOffSample);
        }
        else
        {
            // Note-off is in a future block — add to pending list
            PendingNoteOff pno;
            pno.moduleIndex = moduleIndex;
            pno.noteNumber = evt.noteNumber;
            pno.offBeat = noteOffBeat;
            if (patternLen > 0.0)
                pno.offBeat = std::fmod(pno.offBeat, patternLen);
            pendingNoteOffs.push_back(pno);
        }
    }
}

void PatternEngine::processDrumPlayback(GlobalTransport& transport,
                                         juce::MidiBuffer& outputMidi,
                                         int numSamples, double sampleRate)
{
    const double bpm = static_cast<double>(transport.getBpm());
    const double beatsPerSample = bpm / (60.0 * sampleRate);
    const double blockStartBeat = transport.getBlockStartBeat();
    const double blockLengthBeats = beatsPerSample * numSamples;

    const int loopSteps = drumPattern.getLoopSteps();
    if (loopSteps <= 0)
        return;

    // Each step = 1 sixteenth note = 0.25 beats
    constexpr double beatsPerStep = 0.25;

    // Swing: shift odd 16th-note steps forward in time
    // swing 50% = straight, 66% = triplet feel, 75% = hard swing
    const float swingAmt = swingPercent[kLinnDrum];
    const double swingRatio = static_cast<double>(swingAmt) / 100.0;  // 0.5–0.75

    // Find all steps that fall within this block
    const double endBeat = blockStartBeat + blockLengthBeats;

    // Iterate through potential step positions in this block
    int firstStep = static_cast<int>(blockStartBeat / beatsPerStep);
    int lastStep = static_cast<int>(endBeat / beatsPerStep);

    for (int step = firstStep; step <= lastStep; ++step)
    {
        // Apply swing to odd (off-beat) steps:
        // A straight pair occupies [0, 0.25) and [0.25, 0.50).
        // With swing, the off-beat shifts: its position becomes
        // pairStart + 2 * swingRatio * beatsPerStep
        double stepBeat;
        if (step % 2 == 0)
        {
            stepBeat = static_cast<double>(step) * beatsPerStep;
        }
        else
        {
            int pairBase = step - 1;
            double pairStart = static_cast<double>(pairBase) * beatsPerStep;
            stepBeat = pairStart + 2.0 * swingRatio * beatsPerStep;
        }

        if (stepBeat < blockStartBeat)
            continue;
        if (stepBeat >= endBeat)
            break;

        // Wrap step into drum loop range (not global pattern range)
        int wrappedStep = step % loopSteps;
        if (wrappedStep < 0)
            wrappedStep += loopSteps;

        // Calculate sample offset
        double offsetBeats = stepBeat - blockStartBeat;
        int sampleOffset = static_cast<int>(offsetBeats / beatsPerSample);
        sampleOffset = std::clamp(sampleOffset, 0, numSamples - 1);

        // Check all tracks for triggers at this step
        std::array<bool, DrumPattern::kMaxTracks> triggers;
        std::array<float, DrumPattern::kMaxTracks> velocities;
        drumPattern.getTriggersAtStep(wrappedStep, triggers, velocities);

        for (int track = 0; track < DrumPattern::kMaxTracks; ++track)
        {
            if (triggers[static_cast<size_t>(track)])
            {
                auto noteOn = juce::MidiMessage::noteOn(
                    10, kDrumNoteMap[track],
                    velocities[static_cast<size_t>(track)]);
                outputMidi.addEvent(noteOn, sampleOffset);
            }
        }
    }
}

void PatternEngine::processRecording(int moduleIndex, GlobalTransport& transport,
                                      const juce::MidiBuffer& liveMidi,
                                      double sampleRate, int numSamples)
{
    // Push undo snapshot on first note-on of this recording pass
    if (!recordUndoPushed[static_cast<size_t>(moduleIndex)])
    {
        for (const auto metadata : liveMidi)
        {
            if (metadata.getMessage().isNoteOn())
            {
                if (moduleIndex == kLinnDrum)
                    undoManager.pushDrum(drumPattern);
                else
                    undoManager.pushSynth(moduleIndex, synthPatterns[static_cast<size_t>(moduleIndex)]);
                recordUndoPushed[static_cast<size_t>(moduleIndex)] = true;
                break;
            }
        }
    }

    const double bpm = static_cast<double>(transport.getBpm());
    const double beatsPerSample = bpm / (60.0 * sampleRate);
    const double blockStartBeat = transport.getBlockStartBeat();

    const auto grid = quantizeGrids[static_cast<size_t>(moduleIndex)];

    for (const auto metadata : liveMidi)
    {
        const auto msg = metadata.getMessage();
        const double eventBeat = blockStartBeat + (metadata.samplePosition * beatsPerSample);

        if (moduleIndex == kLinnDrum)
        {
            // Drum recording: note-on places a step in the grid
            if (msg.isNoteOn())
            {
                // Find which drum track this note maps to
                int noteNum = msg.getNoteNumber();
                int track = -1;
                for (int t = 0; t < 15; ++t)
                {
                    if (kDrumNoteMap[t] == noteNum)
                    {
                        track = t;
                        break;
                    }
                }

                if (track >= 0)
                {
                    double qBeat = quantizeBeat(eventBeat, grid);
                    int step = static_cast<int>(qBeat * 4.0);  // 4 steps per beat
                    step = step % drumPattern.getTotalSteps();
                    if (step < 0) step += drumPattern.getTotalSteps();

                    float vel = msg.getFloatVelocity();
                    drumPattern.setStep(track, step, true, vel);
                }
            }
        }
        else
        {
            // Synth recording: capture note-on/note-off pairs as MidiEvents
            if (msg.isNoteOn())
            {
                double qBeat = quantizeBeat(eventBeat, grid);
                double loopLen = synthPatterns[static_cast<size_t>(moduleIndex)].getLoopLengthInBeats();
                if (loopLen > 0.0)
                    qBeat = std::fmod(qBeat, loopLen);

                ActiveNote an;
                an.moduleIndex = moduleIndex;
                an.noteNumber = msg.getNoteNumber();
                an.velocity = msg.getFloatVelocity();
                an.startBeat = qBeat;
                activeRecordNotes.push_back(an);
            }
            else if (msg.isNoteOff())
            {
                // Find matching active note and compute duration
                for (auto it = activeRecordNotes.begin(); it != activeRecordNotes.end(); ++it)
                {
                    if (it->moduleIndex == moduleIndex && it->noteNumber == msg.getNoteNumber())
                    {
                        double endBeatRaw = quantizeBeat(eventBeat, grid);
                        double loopLen = synthPatterns[static_cast<size_t>(moduleIndex)].getLoopLengthInBeats();
                        if (loopLen > 0.0)
                            endBeatRaw = std::fmod(endBeatRaw, loopLen);

                        double duration = endBeatRaw - it->startBeat;
                        if (duration <= 0.0)
                            duration += loopLen;  // wrapped around
                        // Cap: if duration is still non-positive or equals the full loop
                        // (happens when note-on and note-off quantize to the same beat),
                        // use a sensible default instead of spanning the entire pattern.
                        if (duration <= 0.0 || duration >= loopLen)
                            duration = std::min(1.0, loopLen * 0.25);  // 1 beat or quarter loop

                        MidiEvent evt;
                        evt.noteNumber = it->noteNumber;
                        evt.velocity = it->velocity;
                        evt.startBeat = it->startBeat;
                        evt.duration = duration;

                        synthPatterns[static_cast<size_t>(moduleIndex)].addEvent(evt);
                        activeRecordNotes.erase(it);
                        break;
                    }
                }
            }
        }
    }
}

} // namespace axelf
