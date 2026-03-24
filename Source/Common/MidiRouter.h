#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace axelf
{

class MidiRouter
{
public:
    void routeMidi(const juce::MidiBuffer& input,
                   juce::MidiBuffer& jupiter8Midi,
                   juce::MidiBuffer& moog15Midi,
                   juce::MidiBuffer& jx3pMidi,
                   juce::MidiBuffer& dx7Midi,
                   juce::MidiBuffer& ppgwaveMidi,
                   juce::MidiBuffer& linnDrumMidi);

    void setModuleChannel(int moduleIndex, int channel);
    int getModuleChannel(int moduleIndex) const;

private:
    // Default all to omni (0) so every module responds to any MIDI channel.
    // Set per-module channels for channel-specific routing.
    std::array<int, 6> moduleChannels = { 0, 0, 0, 0, 0, 0 };
};

} // namespace axelf
