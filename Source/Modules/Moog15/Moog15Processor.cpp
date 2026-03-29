#include "Moog15Processor.h"
#include "Moog15Voice.h"
#include "Moog15Editor.h"
#include <pmmintrin.h>

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

    // Flush denormals at thread level (per synth-high-quality skill)
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    updateVoiceParameters();

    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampler.processSamplesUp(block);

    // Scale MIDI timestamps by oversampling factor (per spec + DSP conventions skill)
    // Without this, MIDI events land at wrong positions in the oversampled buffer.
    const int osFactor = static_cast<int>(oversampler.getOversamplingFactor());
    juce::MidiBuffer scaledMidi;
    for (const auto metadata : midiMessages)
    {
        scaledMidi.addEvent(metadata.getMessage(), metadata.samplePosition * osFactor);
    }

    float* channels[] = { oversampledBlock.getChannelPointer(0),
                          oversampledBlock.getChannelPointer(1) };
    juce::AudioBuffer<float> osBuffer(channels, 2,
                                       static_cast<int>(oversampledBlock.getNumSamples()));
    synth.renderNextBlock(osBuffer, scaledMidi, 0, static_cast<int>(oversampledBlock.getNumSamples()));

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

    // VCO-1
    int vco1Wave    = static_cast<int>(safeLoad("moog_vco1_waveform"));
    int vco1Range   = static_cast<int>(safeLoad("moog_vco1_footage", 1.0f));
    float vco1Level = safeLoad("moog_vco1_level", 0.8f);
    float vco1PW    = safeLoad("moog_vco1_pw", 0.5f);

    // VCO-2
    int vco2Wave    = static_cast<int>(safeLoad("moog_vco2_waveform"));
    int vco2Range   = static_cast<int>(safeLoad("moog_vco2_footage", 1.0f));
    float vco2Detune = safeLoad("moog_vco2_detune", 7.0f);
    float vco2Level = safeLoad("moog_vco2_level", 0.6f);
    float vco2PW    = safeLoad("moog_vco2_pw", 0.5f);

    // VCO-3
    int vco3Wave    = static_cast<int>(safeLoad("moog_vco3_waveform"));
    int vco3Range   = static_cast<int>(safeLoad("moog_vco3_footage", 1.0f));
    float vco3Detune = safeLoad("moog_vco3_detune", -5.0f);
    float vco3Level = safeLoad("moog_vco3_level", 0.4f);
    float vco3PW    = safeLoad("moog_vco3_pw", 0.5f);
    int vco3Ctrl    = static_cast<int>(safeLoad("moog_vco3_ctrl", 0.0f));

    // Noise & Feedback
    float noiseLvl  = safeLoad("moog_noise_level", 0.0f);
    int noiseColor   = static_cast<int>(safeLoad("moog_noise_color", 0.0f));
    float feedback   = safeLoad("moog_feedback", 0.0f);

    // VCF
    float cutoff     = safeLoad("moog_vcf_cutoff", 2000.0f);
    float reso       = safeLoad("moog_vcf_resonance", 0.2f);
    float drive      = safeLoad("moog_vcf_drive", 1.5f);
    float envDepth   = safeLoad("moog_vcf_env_depth", 0.3f);
    int keyTrack     = static_cast<int>(safeLoad("moog_vcf_key_track", 1.0f));

    // Filter Contour (Model D: no separate release — decay IS the release)
    float e1A = safeLoad("moog_env1_attack", 0.005f);
    float e1D = safeLoad("moog_env1_decay", 0.5f);
    float e1S = safeLoad("moog_env1_sustain", 0.3f);

    // Loudness Contour
    float e2A = safeLoad("moog_env2_attack", 0.003f);
    float e2D = safeLoad("moog_env2_decay", 0.2f);
    float e2S = safeLoad("moog_env2_sustain", 0.85f);

    // Glide
    float glideTime  = safeLoad("moog_glide_time", 0.05f);

    // Modulation
    float lfoPitchAmt = safeLoad("moog_lfo_pitch_amount", 0.0f);
    float vcfLfoAmt   = safeLoad("moog_vcf_lfo_amount", 0.0f);
    float lfoRate     = safeLoad("moog_lfo_rate", 5.0f);
    int lfoWaveform   = static_cast<int>(safeLoad("moog_lfo_waveform", 0.0f));

    // Master
    float masterVol   = safeLoad("moog_master_vol", 0.8f);

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<Moog15Voice*>(synth.getVoice(i)))
        {
            voice->setParameters(vco1Wave, vco1Range, vco1Level, vco1PW,
                                 vco2Wave, vco2Range, vco2Detune, vco2Level, vco2PW,
                                 vco3Wave, vco3Range, vco3Detune, vco3Level, vco3PW,
                                 vco3Ctrl,
                                 noiseLvl, noiseColor,
                                 feedback,
                                 cutoff, reso, drive, envDepth, keyTrack,
                                 e1A, e1D, e1S,
                                 e2A, e2D, e2S,
                                 glideTime,
                                 lfoPitchAmt, vcfLfoAmt,
                                 lfoRate, lfoWaveform,
                                 masterVol);
        }
    }
}

} // namespace axelf::moog15
