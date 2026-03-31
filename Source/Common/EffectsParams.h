#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf
{

/**
 *  APVTS parameter layout for the global effects bus:
 *    - Plate Reverb (Send 1)
 *    - Stereo Delay  (Send 2)
 *    - Master Bus EQ / Compressor / Limiter
 */
inline juce::AudioProcessorValueTreeState::ParameterLayout createEffectsParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── Reverb ────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_reverb_size", 1 }, "Reverb Size",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_reverb_damping", 1 }, "Reverb Damping",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_reverb_width", 1 }, "Reverb Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_reverb_mix", 1 }, "Reverb Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_reverb_predelay", 1 }, "Reverb Predelay",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f));

    // ── Delay (DiffuseDelay — FDN-based) ─────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_time_l", 1 }, "Delay Time",
        juce::NormalisableRange<float> (1.0f, 2000.0f, 0.1f, 0.4f), 375.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_feedback", 1 }, "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_mix", 1 }, "Delay Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_density", 1 }, "Delay Density",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_warp", 1 }, "Delay Warp",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_mod_depth", 1 }, "Delay Mod Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_mod_rate", 1 }, "Delay Mod Rate",
        juce::NormalisableRange<float> (0.01f, 10.0f, 0.01f, 0.4f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_highcut", 1 }, "Delay Damp High",
        juce::NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.4f), 12000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_lowcut", 1 }, "Delay Damp Low",
        juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f), 80.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_delay_width", 1 }, "Delay Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "fx_delay_sync", 1 }, "Delay Sync",
        juce::StringArray { "Off", "1/4", "1/8", "Dotted 1/8", "1/16", "Triplet" }, 0));

    // ── Master Bus EQ ─────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_eq_low", 1 }, "Master EQ Low",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_eq_mid", 1 }, "Master EQ Mid",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_eq_high", 1 }, "Master EQ High",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));

    // ── Master Bus Compressor ────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_comp_threshold", 1 }, "Comp Threshold",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -12.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_comp_ratio", 1 }, "Comp Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.5f), 2.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_comp_attack", 1 }, "Comp Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.1f, 0.4f), 10.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_comp_release", 1 }, "Comp Release",
        juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f), 100.0f));

    // ── Master Bus Limiter ──────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "fx_master_limiter", 1 }, "Limiter On", true));

    // ── Master Bus — Console Summing Saturation ──────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_summing", 1 }, "Summing Saturation",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "fx_master_summing_color", 1 }, "Summing Color",
        juce::StringArray { "Clean", "Neve", "SSL", "API" }, 0));

    // ── Master Bus — Parallel Compression ────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_parallel_blend", 1 }, "Parallel Compression",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // ── Master Bus — Stereo Width (M/S) ─────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_width", 1 }, "Stereo Width",
        juce::NormalisableRange<float> (0.0f, 1.5f, 0.01f), 1.0f));

    // ── Master Bus — Analog Drift ───────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_master_drift", 1 }, "Analog Drift",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // ── Chorus (Send 3) ──────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_chorus_rate", 1 }, "Chorus Rate",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.4f), 1.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_chorus_depth", 1 }, "Chorus Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_chorus_mix", 1 }, "Chorus Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // ── Flanger (Send 4) ─────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_flanger_rate", 1 }, "Flanger Rate",
        juce::NormalisableRange<float> (0.05f, 5.0f, 0.01f, 0.4f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_flanger_depth", 1 }, "Flanger Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_flanger_feedback", 1 }, "Flanger Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_flanger_mix", 1 }, "Flanger Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    // ── Distortion (Send 5) ──────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_distortion_drive", 1 }, "Distortion Drive",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_distortion_tone", 1 }, "Distortion Tone",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fx_distortion_mix", 1 }, "Distortion Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    return { params.begin(), params.end() };
}

} // namespace axelf
