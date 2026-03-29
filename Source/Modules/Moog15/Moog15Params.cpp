#include "Moog15Params.h"

namespace axelf::moog15
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── VCO-1 (4 params) ───────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco1_waveform", 1 }, "VCO-1 Waveform",
        juce::StringArray{ "Triangle", "Shark Tooth", "Sawtooth", "Reverse Saw", "Square", "Pulse" },
        2));  // default: Sawtooth

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco1_footage", 1 }, "VCO-1 Range",
        juce::StringArray{ "32'", "16'", "8'", "4'", "2'" },
        1));  // default: 16'

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco1_level", 1 }, "VCO-1 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco1_pw", 1 }, "VCO-1 Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

    // ── VCO-2 (5 params) ───────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco2_waveform", 1 }, "VCO-2 Waveform",
        juce::StringArray{ "Triangle", "Shark Tooth", "Sawtooth", "Reverse Saw", "Square", "Pulse" },
        2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco2_footage", 1 }, "VCO-2 Range",
        juce::StringArray{ "32'", "16'", "8'", "4'", "2'" },
        1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco2_detune", 1 }, "VCO-2 Detune",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 7.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco2_level", 1 }, "VCO-2 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.6f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco2_pw", 1 }, "VCO-2 Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

    // ── VCO-3 (6 params) ───────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco3_waveform", 1 }, "VCO-3 Waveform",
        juce::StringArray{ "Triangle", "Shark Tooth", "Sawtooth", "Reverse Saw", "Square", "Pulse" },
        2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco3_footage", 1 }, "VCO-3 Range",
        juce::StringArray{ "32'", "16'", "8'", "4'", "2'", "Lo" },
        1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco3_detune", 1 }, "VCO-3 Detune",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), -5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco3_level", 1 }, "VCO-3 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vco3_pw", 1 }, "VCO-3 Pulse Width",
        juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vco3_ctrl", 1 }, "Osc 3 Ctrl",
        juce::StringArray{ "Off", "On" },
        0));

    // ── Noise & Feedback (3 params) ─────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_noise_level", 1 }, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_noise_color", 1 }, "Noise Color",
        juce::StringArray{ "White", "Pink" },
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_feedback", 1 }, "Feedback",
        juce::NormalisableRange<float>(0.0f, 0.8f, 0.01f), 0.0f));

    // ── VCF (5 params) ─────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_cutoff", 1 }, "VCF Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 2000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_resonance", 1 }, "VCF Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_env_depth", 1 }, "VCF Env Depth",
        juce::NormalisableRange<float>(-1.0f, 1.0f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_vcf_key_track", 1 }, "VCF Key Tracking",
        juce::StringArray{ "Off", "33%", "67%", "100%" },
        1));  // default: 33%

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_drive", 1 }, "VCF Drive",
        juce::NormalisableRange<float>(1.0f, 5.0f, 0.01f), 1.5f));

    // ── Filter Contour (3 params — no separate release, Model D authentic) ──
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env1_attack", 1 }, "Filter Contour Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.005f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env1_decay", 1 }, "Filter Contour Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env1_sustain", 1 }, "Filter Contour Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f));

    // ── Loudness Contour (3 params) ─────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env2_attack", 1 }, "Loudness Contour Attack",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.003f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env2_decay", 1 }, "Loudness Contour Decay",
        juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_env2_sustain", 1 }, "Loudness Contour Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.85f));

    // ── Glide (1 param) ─────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_glide_time", 1 }, "Glide Time",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.001f, 0.3f), 0.05f));

    // ── Modulation (4 params) ───────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_lfo_pitch_amount", 1 }, "LFO Pitch Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_vcf_lfo_amount", 1 }, "LFO Filter Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_lfo_rate", 1 }, "LFO Rate",
        juce::NormalisableRange<float>(0.1f, 30.0f, 0.01f, 0.3f), 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "moog_lfo_waveform", 1 }, "LFO Waveform",
        juce::StringArray{ "Sine", "Triangle", "Sawtooth", "Square", "S&H" },
        0));

    // ── Master (1 param) ────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "moog_master_vol", 1 }, "Master Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));

    return { params.begin(), params.end() };
}

} // namespace axelf::moog15
