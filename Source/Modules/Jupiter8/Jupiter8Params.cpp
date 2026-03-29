#include "Jupiter8Params.h"

namespace axelf::jupiter8
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // DCO-1
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_dco1_waveform", 1 }, "DCO-1 Waveform",
        juce::StringArray{ "Sawtooth", "Pulse", "Sub-Osc" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_dco1_range", 1 }, "DCO-1 Range",
        juce::NormalisableRange<float>(2.0f, 16.0f, 1.0f), 8.0f));

    // DCO-2
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_dco2_waveform", 1 }, "DCO-2 Waveform",
        juce::StringArray{ "Sawtooth", "Pulse", "Noise" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_dco2_detune", 1 }, "DCO-2 Detune",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 6.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_dco2_range", 1 }, "DCO-2 Range",
        juce::NormalisableRange<float>(2.0f, 16.0f, 1.0f), 8.0f));

    // Mixer
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_mix_dco1", 1 }, "DCO-1 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_mix_dco2", 1 }, "DCO-2 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // VCF
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_vcf_cutoff", 1 }, "VCF Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 13000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_vcf_resonance", 1 }, "VCF Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_vcf_env_depth", 1 }, "VCF Env Depth",
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_vcf_hpf", 1 }, "HPF Mode",
        juce::StringArray{ "Off", "1", "2", "3" }, 0));

    // Envelope 1 (VCF)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_env1_attack", 1 }, "Env-1 Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.01f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_env1_decay", 1 }, "Env-1 Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_env1_sustain", 1 }, "Env-1 Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_env1_release", 1 }, "Env-1 Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.4f));

    // Envelope 2 (VCA)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_env2_attack", 1 }, "Env-2 Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.01f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_env2_decay", 1 }, "Env-2 Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.25f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_env2_sustain", 1 }, "Env-2 Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_env2_release", 1 }, "Env-2 Release",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.35f));

    // LFO
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_lfo_rate", 1 }, "LFO Rate",
        juce::NormalisableRange<float>(0.1f, 30.0f, 0.01f, 0.3f), 5.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_lfo_depth", 1 }, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_lfo_waveform", 1 }, "LFO Waveform",
        juce::StringArray{ "Sine", "Triangle", "Sawtooth", "Square", "S&H" }, 0));

    // Voice mode
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_voice_mode", 1 }, "Voice Mode",
        juce::StringArray{ "Poly", "Unison", "Solo Last", "Solo Low", "Solo High" }, 0));

    // Pulse Width
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_pulse_width", 1 }, "Pulse Width",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));

    // Sub-Osc Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_sub_level", 1 }, "Sub-Osc Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Noise Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_noise_level", 1 }, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // VCF Key Tracking
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_vcf_key_track", 1 }, "VCF Key Tracking",
        juce::StringArray{ "Off", "50%", "100%" }, 0));

    // VCF LFO Amount
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_vcf_lfo_amount", 1 }, "VCF LFO Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // LFO Delay
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_lfo_delay", 1 }, "LFO Delay",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f, 0.3f), 0.0f));

    // Portamento
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_portamento", 1 }, "Portamento",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.001f, 0.3f), 0.0f));

    // Cross-Modulation Depth
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "jup8_dco_cross_mod", 1 }, "Cross-Mod Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Oscillator Sync (DCO-2 synced to DCO-1)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_dco_sync", 1 }, "DCO Sync",
        juce::StringArray{ "Off", "On" }, 0));

    // Arpeggiator
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_arp_on", 1 }, "Arp On/Off",
        juce::StringArray{ "Off", "On" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_arp_mode", 1 }, "Arp Mode",
        juce::StringArray{ "Up", "Down", "Up/Down", "Random" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_arp_range", 1 }, "Arp Range",
        juce::StringArray{ "1 Oct", "2 Oct", "3 Oct" }, 0));

    // Arp Rate (beats per step: 1/32 to 1/4)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "jup8_arp_rate", 1 }, "Arp Rate",
        juce::StringArray{ "1/32", "1/16", "1/8", "1/4" }, 1));

    return { params.begin(), params.end() };
}

} // namespace axelf::jupiter8
