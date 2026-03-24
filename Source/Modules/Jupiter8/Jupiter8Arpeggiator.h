#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>

namespace axelf::jupiter8
{

enum class ArpMode { Up, Down, UpDown, Random };

class Jupiter8Arpeggiator
{
public:
    void setEnabled(bool on) { enabled = on; if (!on) reset(); }
    bool isEnabled() const { return enabled; }

    void setMode(ArpMode m) { mode = m; }
    void setOctaveRange(int range) { octaveRange = std::clamp(range, 1, 3); }
    void setRate(double beatsPerStep) { stepBeats = beatsPerStep; }

    void reset()
    {
        heldNotes.clear();
        heldVelocities.clear();
        currentIndex = 0;
        goingUp = true;
        samplesUntilNextStep = 0.0;
        lastArpNote = -1;
    }

    // Call from processBlock. Reads incoming MIDI, outputs arpeggiated MIDI.
    // Uses internal sample counting for step timing (works regardless of transport state).
    void process(const juce::MidiBuffer& input, juce::MidiBuffer& output,
                 double /*blockStartBeat*/, int numSamples, double beatsPerSample)
    {
        if (!enabled)
        {
            // Pass through unchanged
            for (const auto metadata : input)
                output.addEvent(metadata.getMessage(), metadata.samplePosition);
            return;
        }

        // Track held notes from input (don't pass note-on/off to output)
        for (const auto metadata : input)
        {
            const auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                addNote(msg.getNoteNumber(), msg.getFloatVelocity());
            }
            else if (msg.isNoteOff())
            {
                removeNote(msg.getNoteNumber());
            }
            else
            {
                // Pass through non-note messages (CC, pitch bend, etc.)
                output.addEvent(msg, metadata.samplePosition);
            }
        }

        if (heldNotes.empty() || stepBeats <= 0.0 || beatsPerSample <= 0.0)
        {
            // No notes held: send note-off for last arp note
            if (lastArpNote >= 0)
            {
                output.addEvent(juce::MidiMessage::noteOff(1, lastArpNote), 0);
                lastArpNote = -1;
            }
            samplesUntilNextStep = 0.0;
            return;
        }

        // Build the full note sequence across octaves
        buildSequence();

        // Internal timing via sample counting
        double samplesPerStep = stepBeats / beatsPerSample;
        int remaining = numSamples;
        int blockOffset = 0;

        while (remaining > 0)
        {
            if (samplesUntilNextStep <= 0.0)
            {
                int samplePos = std::min(blockOffset, numSamples - 1);

                // Note-off for previous note
                if (lastArpNote >= 0)
                    output.addEvent(juce::MidiMessage::noteOff(1, lastArpNote), samplePos);

                // Get next note from sequence
                int note = getNextNote();
                float vel = getVelocityForCurrentNote();

                output.addEvent(juce::MidiMessage::noteOn(1, note, vel), samplePos);
                lastArpNote = note;

                samplesUntilNextStep += samplesPerStep;
            }

            int advance = std::min(remaining,
                                   std::max(1, static_cast<int>(std::ceil(samplesUntilNextStep))));
            samplesUntilNextStep -= advance;
            blockOffset += advance;
            remaining -= advance;
        }
    }

private:
    bool enabled = false;
    ArpMode mode = ArpMode::Up;
    int octaveRange = 1;
    double stepBeats = 0.25; // default 1/16th note

    std::vector<int> heldNotes;
    std::vector<float> heldVelocities;
    std::vector<int> sequence;
    int currentIndex = 0;
    bool goingUp = true;
    double samplesUntilNextStep = 0.0;
    int lastArpNote = -1;

    void addNote(int note, float velocity)
    {
        // Insert sorted
        auto it = std::lower_bound(heldNotes.begin(), heldNotes.end(), note);
        auto idx = std::distance(heldNotes.begin(), it);
        if (it == heldNotes.end() || *it != note)
        {
            heldNotes.insert(it, note);
            heldVelocities.insert(heldVelocities.begin() + idx, velocity);
        }
    }

    void removeNote(int note)
    {
        auto it = std::lower_bound(heldNotes.begin(), heldNotes.end(), note);
        if (it != heldNotes.end() && *it == note)
        {
            auto idx = std::distance(heldNotes.begin(), it);
            heldNotes.erase(it);
            heldVelocities.erase(heldVelocities.begin() + idx);
        }
    }

    void buildSequence()
    {
        sequence.clear();
        for (int oct = 0; oct < octaveRange; ++oct)
            for (int n : heldNotes)
                sequence.push_back(n + oct * 12);

        if (currentIndex >= static_cast<int>(sequence.size()))
            currentIndex = 0;
    }

    int getNextNote()
    {
        if (sequence.empty())
            return 60;

        int seqSize = static_cast<int>(sequence.size());

        switch (mode)
        {
            case ArpMode::Up:
                currentIndex = currentIndex % seqSize;
                break;

            case ArpMode::Down:
                currentIndex = currentIndex % seqSize;
                return sequence[static_cast<size_t>(seqSize - 1 - currentIndex++)];

            case ArpMode::UpDown:
            {
                if (seqSize <= 1)
                {
                    currentIndex = 0;
                    break;
                }
                // Ping-pong: 0,1,...,N-1,N-2,...,1,0,1,...
                int period = (seqSize - 1) * 2;
                int pos = currentIndex % period;
                currentIndex++;
                if (pos < seqSize)
                    return sequence[static_cast<size_t>(pos)];
                else
                    return sequence[static_cast<size_t>(period - pos)];
            }

            case ArpMode::Random:
            {
                int idx = rand() % seqSize;
                return sequence[static_cast<size_t>(idx)];
            }
        }

        int note = sequence[static_cast<size_t>(currentIndex % seqSize)];
        currentIndex++;
        return note;
    }

    float getVelocityForCurrentNote() const
    {
        if (heldVelocities.empty())
            return 0.8f;
        float sum = 0.0f;
        for (float v : heldVelocities)
            sum += v;
        return sum / static_cast<float>(heldVelocities.size());
    }
};

} // namespace axelf::jupiter8
