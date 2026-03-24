#include "DX7Params.h"

namespace axelf::dx7
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Algorithm selector (1-32) — default 5: two 3-op stacks (bell/epiano)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ "dx7_algorithm", 1 }, "Algorithm",
        1, 32, 5));

    // Feedback (0-7) — moderate feedback for warmth
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ "dx7_feedback", 1 }, "Feedback",
        0, 7, 3));

    // Sensible defaults for Algorithm 5 (two 3-op stacks)
    // Op1,4 = carriers (full level, ratio 1.0), Op2,3,5,6 = modulators (reduced levels, harmonic ratios)
    constexpr float defaultLevels[] = { 99.0f, 72.0f, 50.0f, 85.0f, 60.0f, 40.0f };
    constexpr float defaultRatios[] = { 1.0f, 2.0f, 3.0f, 1.0f, 4.0f, 2.0f };
    constexpr float defaultEGRates[6][4] = {
        {99.0f, 65.0f, 50.0f, 40.0f},  // Op1 carrier — gentle decay
        {99.0f, 80.0f, 50.0f, 50.0f},  // Op2 modulator — faster decay
        {99.0f, 80.0f, 50.0f, 50.0f},  // Op3 modulator
        {99.0f, 65.0f, 50.0f, 40.0f},  // Op4 carrier — gentle decay
        {99.0f, 80.0f, 50.0f, 50.0f},  // Op5 modulator
        {99.0f, 80.0f, 50.0f, 50.0f},  // Op6 modulator
    };
    constexpr float defaultEGLevels[6][4] = {
        {99.0f, 75.0f, 50.0f, 0.0f},  // Op1 carrier
        {99.0f, 30.0f, 0.0f, 0.0f},   // Op2 modulator — quick decay = percussive
        {99.0f, 30.0f, 0.0f, 0.0f},   // Op3 modulator
        {99.0f, 75.0f, 50.0f, 0.0f},  // Op4 carrier
        {99.0f, 30.0f, 0.0f, 0.0f},   // Op5 modulator
        {99.0f, 30.0f, 0.0f, 0.0f},   // Op6 modulator
    };

    // Per-operator parameters (6 operators)
    for (int op = 1; op <= 6; ++op)
    {
        auto prefix = "dx7_op" + juce::String(op) + "_";
        const int opIdx = op - 1;

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ prefix + "level", 1 },
            "Op" + juce::String(op) + " Level",
            juce::NormalisableRange<float>(0.0f, 99.0f, 1.0f), defaultLevels[opIdx]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ prefix + "ratio", 1 },
            "Op" + juce::String(op) + " Ratio",
            juce::NormalisableRange<float>(0.5f, 31.0f, 0.01f), defaultRatios[opIdx]));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ prefix + "detune", 1 },
            "Op" + juce::String(op) + " Detune",
            juce::NormalisableRange<float>(-7.0f, 7.0f, 1.0f), 0.0f));

        // EG rates (R1-R4)
        for (int r = 1; r <= 4; ++r)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ prefix + "eg_r" + juce::String(r), 1 },
                "Op" + juce::String(op) + " EG R" + juce::String(r),
                juce::NormalisableRange<float>(0.0f, 99.0f, 1.0f), defaultEGRates[opIdx][r - 1]));
        }

        // EG levels (L1-L4)
        for (int l = 1; l <= 4; ++l)
        {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ prefix + "eg_l" + juce::String(l), 1 },
                "Op" + juce::String(op) + " EG L" + juce::String(l),
                juce::NormalisableRange<float>(0.0f, 99.0f, 1.0f), defaultEGLevels[opIdx][l - 1]));
        }

        // Velocity sensitivity (0 = none, 7 = full)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ prefix + "vel_sens", 1 },
            "Op" + juce::String(op) + " Vel Sens",
            juce::NormalisableRange<float>(0.0f, 7.0f, 1.0f), 3.0f));
    }

    // LFO
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dx7_lfo_speed", 1 }, "LFO Speed",
        juce::NormalisableRange<float>(0.0f, 99.0f, 1.0f), 35.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dx7_lfo_delay", 1 }, "LFO Delay",
        juce::NormalisableRange<float>(0.0f, 99.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dx7_lfo_pmd", 1 }, "LFO PMD",
        juce::NormalisableRange<float>(0.0f, 99.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "dx7_lfo_amd", 1 }, "LFO AMD",
        juce::NormalisableRange<float>(0.0f, 99.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "dx7_lfo_wave", 1 }, "LFO Wave",
        juce::StringArray{ "Triangle", "SawDown", "SawUp", "Square", "Sine", "S&H" }, 0));

    return { params.begin(), params.end() };
}

} // namespace axelf::dx7
