#include "Moog15Params.h"

namespace axelf::moog15
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // VCO-1
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco1_waveform", 1 }, "VCO-1 Waveform",
        juce::StringArray{ "Sawtooth", "Triangle", "Pulse", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco1_range", 1 }, "VCO-1 Range",
        juce::NormalisableRange<float>(2.0f, 32.0f, 1.0f), 16.0f));

    // VCO-2
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco2_waveform", 1 }, "VCO-2 Waveform",
        juce::StringArray{ "Sawtooth", "Triangle", "Pulse", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco2_detune", 1 }, "VCO-2 Detune",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));

    // VCO-3
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco3_waveform", 1 }, "VCO-3 Waveform",
        juce::StringArray{ "Sawtooth", "Triangle", "Pulse", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco3_detune", 1 }, "VCO-3 Detune",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));

    // Mixer
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_mix_vco1", 1 }, "VCO-1 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_mix_vco2", 1 }, "VCO-2 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_mix_vco3", 1 }, "VCO-3 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f));

    // VCF (Ladder)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_cutoff", 1 }, "VCF Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 400.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_resonance", 1 }, "VCF Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_drive", 1 }, "VCF Drive",
        juce::NormalisableRange<float>(1.0f, 5.0f, 0.01f), 1.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_env_depth", 1 }, "VCF Env Depth",
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.6f));

    // Envelope 1 (VCF)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env1_attack", 1 }, "Env-1 Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.005f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env1_decay", 1 }, "Env-1 Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env1_sustain", 1 }, "Env-1 Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env1_release", 1 }, "Env-1 Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));

    // Envelope 2 (VCA)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env2_attack", 1 }, "Env-2 Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.005f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env2_decay", 1 }, "Env-2 Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env2_sustain", 1 }, "Env-2 Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env2_release", 1 }, "Env-2 Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.2f));

    // Glide
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_glide_time", 1 }, "Glide Time",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f), 0.05f));

    // LFO (921)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_lfo_rate", 1 }, "LFO Rate",
        juce::NormalisableRange<float>(0.01f, 30.0f, 0.01f, 0.3f), 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_lfo_depth", 1 }, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_lfo_waveform", 1 }, "LFO Waveform",
        juce::StringArray{ "Sine", "Triangle", "Sawtooth", "Square", "S&H" }, 0));

    // Noise Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_noise_level", 1 }, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // VCF Key Tracking
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vcf_key_track", 1 }, "VCF Key Tracking",
        juce::StringArray{ "Off", "33%", "67%", "100%" }, 0));

    // VCF LFO Amount
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_lfo_amount", 1 }, "VCF LFO Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // LFO Pitch Mod Amount
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_lfo_pitch_amount", 1 }, "LFO Pitch Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Pulse Width per VCO
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco1_pw", 1 }, "VCO-1 Pulse Width",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco2_pw", 1 }, "VCO-2 Pulse Width",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco3_pw", 1 }, "VCO-3 Pulse Width",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // VCO-2 and VCO-3 Range
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco2_range", 1 }, "VCO-2 Range",
        juce::NormalisableRange<float>(2.0f, 32.0f, 1.0f), 16.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco3_range", 1 }, "VCO-3 Range",
        juce::NormalisableRange<float>(2.0f, 32.0f, 1.0f), 16.0f));

    return { params.begin(), params.end() };
}

} // namespace axelf::moog15
