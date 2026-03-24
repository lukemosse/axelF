#include "LinnDrumParams.h"

namespace axelf::linndrum
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Per-drum parameters
    for (const auto& drumName : getDrumNames())
    {
        auto prefix = "linn_" + drumName + "_";

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ prefix + "level", 1 },
            drumName + " Level",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ prefix + "tune", 1 },
            drumName + " Tune",
            juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ prefix + "decay", 1 },
            drumName + " Decay",
            juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ prefix + "pan", 1 },
            drumName + " Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    }

    // Global
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "linn_master_tune", 1 }, "Master Tune",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "linn_swing", 1 }, "Swing",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "linn_tempo", 1 }, "Tempo",
        juce::NormalisableRange<float>(60.0f, 240.0f, 0.1f), 120.0f));

    // Sample bank selector
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "linn_bank", 1 }, "Sample Bank",
        getBankNames(), 1));   // default to "OG Clean" (index 1)

    return { params.begin(), params.end() };
}

} // namespace axelf::linndrum
