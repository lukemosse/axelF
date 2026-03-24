#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf
{

class ModulationMatrix
{
public:
    static constexpr int kMaxSlots = 16;

    struct Slot
    {
        juce::String sourceId;
        juce::String targetId;
        float amount = 0.0f;
        bool active  = false;
    };

    void addParameterTree(juce::AudioProcessorValueTreeState* apvts);
    void process(int numSamples);
    void reset();

    Slot& getSlot(int index) { return slots[static_cast<size_t>(index)]; }

private:
    std::atomic<float>* findParam(const juce::String& id) const;

    std::array<Slot, kMaxSlots> slots;
    std::vector<juce::AudioProcessorValueTreeState*> trees;
};

} // namespace axelf
