#include "PPGWaveParams.h"

namespace axelf::ppgwave
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── Voice Mode ───────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_voice_mode", 1 }, "Voice Mode",
        juce::StringArray{ "Poly", "Unison", "Dual" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_unison_detune", 1 }, "Unison Detune",
        juce::NormalisableRange<float>(0.0f, 50.0f, 0.1f), 15.0f));

    // ── WaveA ────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_waveA_table", 1 }, "WaveA Table",
        juce::StringArray{ "Upper Waves", "Resonant", "Resonant 2", "Mallet",
                           "Anharmonic", "E-Piano", "Bass", "Organ",
                           "Vocal", "Pad", "Digital Harsh", "Sync Sweep",
                           "Classic Analog", "Additive", "Noise Shapes", "User" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveA_pos", 1 }, "WaveA Position",
        juce::NormalisableRange<float>(0.0f, 63.0f, 1.0f), 8.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveA_env_amt", 1 }, "WaveA Env Amt",
        juce::NormalisableRange<float>(-63.0f, 63.0f, 1.0f), 30.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveA_lfo_amt", 1 }, "WaveA LFO Amt",
        juce::NormalisableRange<float>(0.0f, 63.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_waveA_range", 1 }, "WaveA Range",
        juce::StringArray{ "64'", "32'", "16'", "8'", "4'", "2'" }, 3));  // default 8'

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveA_semi", 1 }, "WaveA Semi",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveA_detune", 1 }, "WaveA Detune",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "ppg_waveA_keytrack", 1 }, "WaveA Keytrack", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveA_level", 1 }, "WaveA Level",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 100.0f));

    // ── WaveB ────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_waveB_table", 1 }, "WaveB Table",
        juce::StringArray{ "Upper Waves", "Resonant", "Resonant 2", "Mallet",
                           "Anharmonic", "E-Piano", "Bass", "Organ",
                           "Vocal", "Pad", "Digital Harsh", "Sync Sweep",
                           "Classic Analog", "Additive", "Noise Shapes", "User" }, 4));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveB_pos", 1 }, "WaveB Position",
        juce::NormalisableRange<float>(0.0f, 63.0f, 1.0f), 12.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveB_env_amt", 1 }, "WaveB Env Amt",
        juce::NormalisableRange<float>(-63.0f, 63.0f, 1.0f), 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveB_lfo_amt", 1 }, "WaveB LFO Amt",
        juce::NormalisableRange<float>(0.0f, 63.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_waveB_range", 1 }, "WaveB Range",
        juce::StringArray{ "64'", "32'", "16'", "8'", "4'", "2'" }, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveB_semi", 1 }, "WaveB Semi",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveB_detune", 1 }, "WaveB Detune",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 7.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "ppg_waveB_keytrack", 1 }, "WaveB Keytrack", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_waveB_level", 1 }, "WaveB Level",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 80.0f));

    // ── Sub Oscillator ───────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_sub_wave", 1 }, "Sub Wave",
        juce::StringArray{ "Sine", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_sub_octave", 1 }, "Sub Octave",
        juce::StringArray{ "-1", "-2" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_sub_level", 1 }, "Sub Level",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 0.0f));

    // ── Noise ────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_noise_level", 1 }, "Noise Level",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_noise_color", 1 }, "Noise Color",
        juce::StringArray{ "White", "Pink" }, 0));

    // ── Mixer ────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_mix_waveA", 1 }, "Mix WaveA",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_mix_waveB", 1 }, "Mix WaveB",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 80.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_mix_sub", 1 }, "Mix Sub",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_mix_noise", 1 }, "Mix Noise",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_mix_ringmod", 1 }, "Mix Ring Mod",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 0.0f));

    // ── Filter (VCF) ────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_vcf_cutoff", 1 }, "VCF Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f), 8000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_vcf_resonance", 1 }, "VCF Resonance",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 30.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_vcf_env_amt", 1 }, "VCF Env Amt",
        juce::NormalisableRange<float>(-127.0f, 127.0f, 1.0f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_vcf_lfo_amt", 1 }, "VCF LFO Amt",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_vcf_keytrack", 1 }, "VCF Keytrack",
        juce::StringArray{ "0%", "50%", "100%" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_vcf_type", 1 }, "VCF Type",
        juce::StringArray{ "LP24", "LP12", "BP12", "HP12" }, 0));

    // ── VCA ──────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_vca_level", 1 }, "VCA Level",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_vca_vel_sens", 1 }, "VCA Vel Sens",
        juce::NormalisableRange<float>(0.0f, 7.0f, 1.0f), 3.0f));

    // ── Env 1 (Filter) ──────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env1_attack", 1 }, "Env1 Attack",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env1_decay", 1 }, "Env1 Decay",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 800.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env1_sustain", 1 }, "Env1 Sustain",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env1_release", 1 }, "Env1 Release",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 500.0f));

    // ── Env 2 (Amplitude) ───────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env2_attack", 1 }, "Env2 Attack",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env2_decay", 1 }, "Env2 Decay",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env2_sustain", 1 }, "Env2 Sustain",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 127.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env2_release", 1 }, "Env2 Release",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 600.0f));

    // ── Env 3 (Wave Position) ───────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env3_attack", 1 }, "Env3 Attack",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 300.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env3_decay", 1 }, "Env3 Decay",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 2000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env3_sustain", 1 }, "Env3 Sustain",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 38.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_env3_release", 1 }, "Env3 Release",
        juce::NormalisableRange<float>(0.5f, 10000.0f, 0.1f, 0.3f), 1000.0f));

    // ── LFO ──────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_lfo_wave", 1 }, "LFO Wave",
        juce::StringArray{ "Triangle", "Sawtooth", "RevSaw", "Square", "Sine", "S&H" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_lfo_rate", 1 }, "LFO Rate",
        juce::NormalisableRange<float>(0.01f, 50.0f, 0.01f, 0.3f), 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_lfo_delay", 1 }, "LFO Delay",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_lfo_pitch_amt", 1 }, "LFO Pitch",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_lfo_cutoff_amt", 1 }, "LFO Cutoff",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_lfo_amp_amt", 1 }, "LFO Amp",
        juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ "ppg_lfo_sync", 1 }, "LFO Sync", false));

    // ── Performance ──────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_bend_range", 1 }, "Bend Range",
        juce::NormalisableRange<float>(1.0f, 12.0f, 1.0f), 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ppg_porta_time", 1 }, "Portamento",
        juce::NormalisableRange<float>(0.0f, 5.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "ppg_porta_mode", 1 }, "Portamento Mode",
        juce::StringArray{ "Off", "Legato", "Always" }, 0));

    return { params.begin(), params.end() };
}

} // namespace axelf::ppgwave
