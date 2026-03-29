#include "JX3PParams.h"

namespace axelf::jx3p
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // DCO-1
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jx3p_dco1_waveform", 1 }, "DCO-1 Waveform",
        juce::StringArray{ "Sawtooth", "Pulse", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_dco1_range", 1 }, "DCO-1 Range",
        juce::NormalisableRange<float>(2.0f, 16.0f, 1.0f), 8.0f));

    // DCO-2
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jx3p_dco2_waveform", 1 }, "DCO-2 Waveform",
        juce::StringArray{ "Sawtooth", "Pulse", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_dco2_detune", 1 }, "DCO-2 Detune",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f));

    // VCF (IR3109)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_vcf_cutoff", 1 }, "VCF Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 5000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_vcf_resonance", 1 }, "VCF Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_vcf_env_depth", 1 }, "VCF Env Depth",
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.4f));

    // Envelope
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_env_attack", 1 }, "Env Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.01f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_env_decay", 1 }, "Env Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_env_sustain", 1 }, "Env Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_env_release", 1 }, "Env Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));

    // Envelope 2 (VCA) — separate from Env 1 (VCF)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_env2_attack", 1 }, "Env-2 Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.01f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_env2_decay", 1 }, "Env-2 Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_env2_sustain", 1 }, "Env-2 Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_env2_release", 1 }, "Env-2 Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.35f));

    // LFO
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_lfo_rate", 1 }, "LFO Rate",
        juce::NormalisableRange<float>(0.05f, 50.0f, 0.01f, 0.3f), 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_lfo_depth", 1 }, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jx3p_lfo_waveform", 1 }, "LFO Waveform",
        juce::StringArray{ "Sine", "Triangle", "Sawtooth", "Square", "S&H" }, 0));

    // Chorus
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jx3p_chorus_mode", 1 }, "Chorus Mode",
        juce::StringArray{ "Off", "I", "II" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_chorus_rate", 1 }, "Chorus Rate",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_chorus_depth", 1 }, "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // DCO-2 Range
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_dco2_range", 1 }, "DCO-2 Range",
        juce::NormalisableRange<float>(2.0f, 16.0f, 1.0f), 8.0f));

    // DCO-1 Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_mix_dco1", 1 }, "DCO-1 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));

    // DCO-2 Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_mix_dco2", 1 }, "DCO-2 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // VCF LFO Amount
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_vcf_lfo_amount", 1 }, "VCF LFO Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // VCF Key Tracking
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jx3p_vcf_key_track", 1 }, "VCF Key Tracking",
        juce::StringArray{ "Off", "Half", "Full" }, 0));

    // LFO Delay
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_lfo_delay", 1 }, "LFO Delay",
        juce::NormalisableRange<float>(0.0f, 3.0f, 0.001f, 0.3f), 0.0f));

    // Cross-Modulation
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_dco_cross_mod", 1 }, "Cross-Mod Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Oscillator Sync
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jx3p_dco_sync", 1 }, "DCO Sync",
        juce::StringArray{ "Off", "On" }, 0));

    // Chorus Spread (stereo width)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_chorus_spread", 1 }, "Chorus Spread",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // Pulse Width (DCO-1)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_pulse_width", 1 }, "DCO-1 Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

    // Pulse Width (DCO-2)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_dco2_pw", 1 }, "DCO-2 Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

    // Noise Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_noise_level", 1 }, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Portamento
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jx3p_portamento", 1 }, "Portamento Time",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f), 0.0f));

    return { params.begin(), params.end() };
}

} // namespace axelf::jx3p
