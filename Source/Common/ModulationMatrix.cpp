#include "ModulationMatrix.h"

namespace axelf
{

void ModulationMatrix::addParameterTree(juce::AudioProcessorValueTreeState* apvts)
{
    if (apvts != nullptr)
        trees.push_back(apvts);
}

std::atomic<float>* ModulationMatrix::findParam(const juce::String& id) const
{
    for (auto* tree : trees)
    {
        if (auto* p = tree->getRawParameterValue(id))
            return p;
    }
    return nullptr;
}

void ModulationMatrix::process(int /*numSamples*/)
{
    for (auto& slot : slots)
    {
        if (!slot.active || slot.sourceId.isEmpty() || slot.targetId.isEmpty())
            continue;

        auto* src = findParam(slot.sourceId);
        auto* dst = findParam(slot.targetId);
        if (src == nullptr || dst == nullptr)
            continue;

        // Read normalised source value, scale by amount, add to target
        float srcVal = src->load();
        float modOffset = srcVal * slot.amount;

        // Apply as additive offset — clamped to parameter range
        auto* dstParam = [&]() -> juce::RangedAudioParameter*
        {
            for (auto* tree : trees)
                if (auto* p = tree->getParameter(slot.targetId))
                    return p;
            return nullptr;
        }();

        if (dstParam == nullptr)
            continue;

        auto range = dstParam->getNormalisableRange();
        float currentVal = dst->load();
        float newVal = juce::jlimit(range.start, range.end, currentVal + modOffset);
        dst->store(newVal);
    }
}

void ModulationMatrix::reset()
{
    for (auto& slot : slots)
    {
        slot.sourceId = {};
        slot.targetId = {};
        slot.amount = 0.0f;
        slot.active = false;
    }
}

} // namespace axelf
