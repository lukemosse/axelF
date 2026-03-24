#include "Moog15Processor.h"
#include "Moog15Voice.h"
#include "Moog15Editor.h"

namespace axelf::moog15
{

Moog15Processor::Moog15Processor()
    : apvts(dummyProcessor, nullptr, "Moog15", createParameterLayout())
{
    // Monophonic — single voice
    synth.addVoice(new Moog15Voice());
    synth.addSound(new Moog15Sound());
}

void Moog15Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    oversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampledRate = sampleRate * static_cast<double>(oversampler.getOversamplingFactor());
    synth.setCurrentPlaybackSampleRate(oversampledRate);
}

void Moog15Processor::processBlock(juce::AudioBuffer<float>& buffer,
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
}

void Moog15Processor::releaseResources() {}

juce::Component* Moog15Processor::createEditor()
{
    return nullptr;  // Editors created in PluginEditor
}

void Moog15Processor::updateVoiceParameters()
{
    auto safeLoad = [&](const juce::String& id, float fallback = 0.0f) -> float
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return p->load();
        return fallback;
    };

    int vco1Wave   = static_cast<int>(safeLoad("moog_vco1_waveform"));
    float vco1Range = safeLoad("moog_vco1_range", 16.0f);
    int vco2Wave   = static_cast<int>(safeLoad("moog_vco2_waveform"));
    float vco2Detune = safeLoad("moog_vco2_detune", 0.0f);
    int vco3Wave   = static_cast<int>(safeLoad("moog_vco3_waveform"));
    float vco3Detune = safeLoad("moog_vco3_detune", 0.0f);

    float mixV1 = safeLoad("moog_mix_vco1", 0.8f);
    float mixV2 = safeLoad("moog_mix_vco2", 0.6f);
    float mixV3 = safeLoad("moog_mix_vco3", 0.4f);

    float cutoff   = safeLoad("moog_vcf_cutoff", 400.0f);
    float reso     = safeLoad("moog_vcf_resonance", 0.4f);
    float drive    = safeLoad("moog_vcf_drive", 1.5f);
    float envDepth = safeLoad("moog_vcf_env_depth", 0.6f);

    float e1A = safeLoad("moog_env1_attack", 0.005f);
    float e1D = safeLoad("moog_env1_decay", 0.5f);
    float e1S = safeLoad("moog_env1_sustain", 0.3f);
    float e1R = safeLoad("moog_env1_release", 0.3f);
    float e2A = safeLoad("moog_env2_attack", 0.005f);
    float e2D = safeLoad("moog_env2_decay", 0.4f);
    float e2S = safeLoad("moog_env2_sustain", 0.5f);
    float e2R = safeLoad("moog_env2_release", 0.2f);

    float glideTime = safeLoad("moog_glide_time", 0.05f);

    float lfoRate     = safeLoad("moog_lfo_rate", 5.0f);
    float lfoDepth    = safeLoad("moog_lfo_depth", 0.0f);
    int   lfoWaveform = static_cast<int>(safeLoad("moog_lfo_waveform", 0.0f));

    // Phase 2 params
    float vco1PW       = safeLoad("moog_vco1_pw", 0.5f);
    float vco2PW       = safeLoad("moog_vco2_pw", 0.5f);
    float vco3PW       = safeLoad("moog_vco3_pw", 0.5f);
    float noiseLvl     = safeLoad("moog_noise_level", 0.0f);
    int   vcfKeyTrack  = static_cast<int>(safeLoad("moog_vcf_key_track", 0.0f));
    float vcfLfoAmt    = safeLoad("moog_vcf_lfo_amount", 0.0f);
    float lfoPitchAmt  = safeLoad("moog_lfo_pitch_amount", 0.0f);

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<Moog15Voice*>(synth.getVoice(i)))
        {
            voice->setParameters(vco1Wave, vco1Range,
                                 vco2Wave, vco2Detune,
                                 vco3Wave, vco3Detune,
                                 mixV1, mixV2, mixV3,
                                 cutoff, reso, drive, envDepth,
                                 e1A, e1D, e1S, e1R,
                                 e2A, e2D, e2S, e2R,
                                 glideTime,
                                 lfoRate, lfoDepth, lfoWaveform,
                                 vco1PW, vco2PW, vco3PW,
                                 noiseLvl, vcfKeyTrack,
                                 vcfLfoAmt, lfoPitchAmt);
        }
    }
}

} // namespace axelf::moog15

