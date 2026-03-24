#include "MidiRouter.h"

namespace axelf
{

void MidiRouter::routeMidi(const juce::MidiBuffer& input,
                            juce::MidiBuffer& jupiter8Midi,
                            juce::MidiBuffer& moog15Midi,
                            juce::MidiBuffer& jx3pMidi,
                            juce::MidiBuffer& dx7Midi,
                            juce::MidiBuffer& linnDrumMidi)
{
    jupiter8Midi.clear();
    moog15Midi.clear();
    jx3pMidi.clear();
    dx7Midi.clear();
    linnDrumMidi.clear();

    for (const auto metadata : input)
    {
        const auto msg = metadata.getMessage();
        const int channel = msg.getChannel();  // 1-16, 0 for non-channel messages
        const int samplePosition = metadata.samplePosition;

        std::array<juce::MidiBuffer*, 5> buffers = {
            &jupiter8Midi, &moog15Midi, &jx3pMidi, &dx7Midi, &linnDrumMidi
        };

        if (channel == 0)
        {
            // Non-channel messages (clock, sysex) go to all modules
            for (auto* buf : buffers)
                buf->addEvent(msg, samplePosition);
        }
        else
        {
            for (int i = 0; i < 5; ++i)
            {
                if (moduleChannels[static_cast<size_t>(i)] == 0 ||
                    moduleChannels[static_cast<size_t>(i)] == channel)
                {
                    buffers[static_cast<size_t>(i)]->addEvent(msg, samplePosition);
                }
            }
        }
    }
}

void MidiRouter::setModuleChannel(int moduleIndex, int channel)
{
    if (moduleIndex >= 0 && moduleIndex < 5)
        moduleChannels[static_cast<size_t>(moduleIndex)] = channel;
}

int MidiRouter::getModuleChannel(int moduleIndex) const
{
    if (moduleIndex >= 0 && moduleIndex < 5)
        return moduleChannels[static_cast<size_t>(moduleIndex)];
    return 0;
}

} // namespace axelf
