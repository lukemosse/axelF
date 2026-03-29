#include "JX3PProcessor.h"
#include "JX3PVoice.h"
#include "JX3PEditor.h"
#include <cmath>

namespace axelf::jx3p
{

JX3PProcessor::JX3PProcessor()
    : apvts(dummyProcessor, nullptr, "JX3P", createParameterLayout())
{
    // 6-voice polyphonic
    for (int i = 0; i < 6; ++i)
        synth.addVoice(new JX3PVoice());

    synth.addSound(new JX3PSound());
}

void JX3PProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    oversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampledRate = sampleRate * static_cast<double>(oversampler.getOversamplingFactor());
    synth.setCurrentPlaybackSampleRate(oversampledRate);
    chorus.setSampleRate(sampleRate);
}

void JX3PProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    updateVoiceParameters();

    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampler.processSamplesUp(block);

    float* channels[] = { oversampledBlock.getChannelPointer(0),
                          oversampledBlock.getChannelPointer(1) };
    juce::AudioBuffer<float> osBuffer(channels, 2,
                                       static_cast<int>(oversampledBlock.getNumSamples()));
    synth.renderNextBlock(osBuffer, midiMessages, 0, static_cast<int>(oversampledBlock.getNumSamples()));

    oversampler.processSamplesDown(block);

    // Apply chorus post-synth (at original sample rate)
    auto* chorusModeParam = apvts.getRawParameterValue("jx3p_chorus_mode");
    auto* chorusRateParam = apvts.getRawParameterValue("jx3p_chorus_rate");
    auto* chorusDepthParam = apvts.getRawParameterValue("jx3p_chorus_depth");

    if (chorusModeParam == nullptr || chorusRateParam == nullptr || chorusDepthParam == nullptr)
        return;

    chorus.setParameters(static_cast<int>(chorusModeParam->load()),
                         chorusRateParam->load(),
                         chorusDepthParam->load());

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float l = buffer.getSample(0, sample);
        float r = buffer.getSample(1, sample);
        chorus.processStereo(l, r);

        // Sanitise output — clamp NaN/Inf to zero to prevent loud crashes
        if (!std::isfinite(l)) l = 0.0f;
        if (!std::isfinite(r)) r = 0.0f;

        buffer.setSample(0, sample, l);
        buffer.setSample(1, sample, r);
    }
}

void JX3PProcessor::releaseResources() {}

juce::Component* JX3PProcessor::createEditor()
{
    return nullptr;  // Editors created in PluginEditor
}

void JX3PProcessor::updateVoiceParameters()
{
    auto safeLoad = [&](const juce::String& id, float fallback = 0.0f) -> float
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return p->load();
        return fallback;
    };

    int dco1Wave    = static_cast<int>(safeLoad("jx3p_dco1_waveform"));
    int dco2Wave    = static_cast<int>(safeLoad("jx3p_dco2_waveform"));
    float dco2Detune = safeLoad("jx3p_dco2_detune", 0.0f);
    float cutoff    = safeLoad("jx3p_vcf_cutoff", 5000.0f);
    float reso      = safeLoad("jx3p_vcf_resonance", 0.2f);
    float envDepth  = safeLoad("jx3p_vcf_env_depth", 0.4f);

    // Env 1 (VCF)
    float e1A = safeLoad("jx3p_env_attack", 0.01f);
    float e1D = safeLoad("jx3p_env_decay", 0.3f);
    float e1S = safeLoad("jx3p_env_sustain", 0.6f);
    float e1R = safeLoad("jx3p_env_release", 0.3f);

    // Env 2 (VCA)
    float e2A = safeLoad("jx3p_env2_attack", 0.01f);
    float e2D = safeLoad("jx3p_env2_decay", 0.25f);
    float e2S = safeLoad("jx3p_env2_sustain", 0.7f);
    float e2R = safeLoad("jx3p_env2_release", 0.35f);

    // LFO
    float lfoRate     = safeLoad("jx3p_lfo_rate", 5.0f);
    float lfoDepth    = safeLoad("jx3p_lfo_depth", 0.0f);
    int   lfoWaveform = static_cast<int>(safeLoad("jx3p_lfo_waveform", 0.0f));

    // Phase 2 params
    float dco1Range      = safeLoad("jx3p_dco1_range", 8.0f);
    float mixDco1        = safeLoad("jx3p_mix_dco1", 0.5f);
    float mixDco2        = safeLoad("jx3p_mix_dco2", 0.5f);
    float vcfLfoAmount   = safeLoad("jx3p_vcf_lfo_amount", 0.0f);
    int   vcfKeyTrack    = static_cast<int>(safeLoad("jx3p_vcf_key_track", 0.0f));
    float lfoDelay       = safeLoad("jx3p_lfo_delay", 0.0f);
    float crossModDepth  = safeLoad("jx3p_dco_cross_mod", 0.0f);
    int   dcoSync        = static_cast<int>(safeLoad("jx3p_dco_sync", 0.0f));
    float pulseWidth     = safeLoad("jx3p_pulse_width", 0.5f);
    float dco2PW         = safeLoad("jx3p_dco2_pw", 0.5f);
    float noiseLvl       = safeLoad("jx3p_noise_level", 0.0f);
    float portamento     = safeLoad("jx3p_portamento", 0.0f);
    float chorusSpread   = safeLoad("jx3p_chorus_spread", 0.5f);

    chorus.setSpread(chorusSpread);

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<JX3PVoice*>(synth.getVoice(i)))
        {
            voice->setParameters(dco1Wave, dco2Wave, dco2Detune,
                                 cutoff, reso, envDepth,
                                 e1A, e1D, e1S, e1R,
                                 e2A, e2D, e2S, e2R,
                                 lfoRate, lfoDepth, lfoWaveform,
                                 dco1Range, mixDco1, mixDco2,
                                 vcfLfoAmount, vcfKeyTrack,
                                 lfoDelay, crossModDepth, dcoSync,
                                 pulseWidth, dco2PW,
                                 noiseLvl, portamento);
        }
    }
}

} // namespace axelf::jx3p

