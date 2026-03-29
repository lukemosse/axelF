#include "PPGWaveProcessor.h"
#include "PPGWaveVoice.h"

namespace axelf::ppgwave
{

PPGWaveProcessor::PPGWaveProcessor()
    : apvts(dummyProcessor, nullptr, "PPGWave", createParameterLayout())
{
    // Build factory wavetables once
    PPGWaveOsc::initFactoryTables();

    // 8-voice polyphonic (matches original PPG Wave 2.2)
    for (int i = 0; i < 8; ++i)
        synth.addVoice(new PPGWaveVoice());

    synth.addSound(new PPGWaveSound());
}

void PPGWaveProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void PPGWaveProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    updateVoiceParameters();
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

void PPGWaveProcessor::releaseResources() {}

juce::Component* PPGWaveProcessor::createEditor()
{
    return nullptr;  // Editors created in PluginEditor
}

void PPGWaveProcessor::updateVoiceParameters()
{
    auto safeLoad = [&](const juce::String& id, float fallback = 0.0f) -> float
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return p->load();
        return fallback;
    };

    // Filter mode mapping
    int filterTypeInt = static_cast<int>(safeLoad("ppg_vcf_type", 0.0f));
    SSM2044Filter::Mode filterMode;
    switch (filterTypeInt)
    {
        case 1:  filterMode = SSM2044Filter::Mode::LP12; break;
        case 2:  filterMode = SSM2044Filter::Mode::BP12; break;
        case 3:  filterMode = SSM2044Filter::Mode::HP12; break;
        default: filterMode = SSM2044Filter::Mode::LP24; break;
    }

    // Keytrack choice (0=0%, 1=50%, 2=100%) → float
    int keytrackIdx = static_cast<int>(safeLoad("ppg_vcf_keytrack", 1.0f));
    float keytrackFloat = (keytrackIdx == 0) ? 0.0f : (keytrackIdx == 1) ? 0.5f : 1.0f;

    // Sub octave choice (0="-1", 1="-2") → actual octave offset
    int subOctaveIdx = static_cast<int>(safeLoad("ppg_sub_octave", 0.0f));
    int subOctaveVal = -(subOctaveIdx + 1);  // 0→-1, 1→-2

    // Noise color choice (0="White", 1="Pink") → filter coefficient
    // White = bright (high coeff), Pink = dark (low coeff)
    int noiseColorIdx = static_cast<int>(safeLoad("ppg_noise_color", 0.0f));
    float noiseColorVal = (noiseColorIdx == 0) ? 1.0f : 0.3f;

    // Voice mode: 0=Poly, 1=Unison, 2=Dual
    int voiceModeInt = static_cast<int>(safeLoad("ppg_voice_mode", 0.0f));
    float unisonDetuneCents = safeLoad("ppg_unison_detune", 15.0f);
    synth.unisonMode = (voiceModeInt == 1);

    for (int v = 0; v < synth.getNumVoices(); ++v)
    {
        if (auto* voice = dynamic_cast<PPGWaveVoice*>(synth.getVoice(v)))
        {
            // Osc A — normalize level 0-127 → 0-1, env/lfo amounts ±63 → ±1
            voice->setOscAParams(
                static_cast<int>(safeLoad("ppg_waveA_table", 0.0f)),
                safeLoad("ppg_waveA_pos", 0.0f),
                safeLoad("ppg_waveA_env_amt", 0.0f) / 63.0f,
                safeLoad("ppg_waveA_lfo_amt", 0.0f) / 63.0f,
                static_cast<int>(safeLoad("ppg_waveA_range", 8.0f)),
                static_cast<int>(safeLoad("ppg_waveA_semi", 0.0f)),
                safeLoad("ppg_waveA_detune", 0.0f),
                safeLoad("ppg_waveA_keytrack", 1.0f) > 0.5f,
                safeLoad("ppg_waveA_level", 1.0f) / 127.0f);

            // Osc B — same normalization
            voice->setOscBParams(
                static_cast<int>(safeLoad("ppg_waveB_table", 0.0f)),
                safeLoad("ppg_waveB_pos", 0.0f),
                safeLoad("ppg_waveB_env_amt", 0.0f) / 63.0f,
                safeLoad("ppg_waveB_lfo_amt", 0.0f) / 63.0f,
                static_cast<int>(safeLoad("ppg_waveB_range", 8.0f)),
                static_cast<int>(safeLoad("ppg_waveB_semi", 0.0f)),
                safeLoad("ppg_waveB_detune", 0.0f),
                safeLoad("ppg_waveB_keytrack", 1.0f) > 0.5f,
                safeLoad("ppg_waveB_level", 0.5f) / 127.0f);

            // Sub osc — use mapped octave value
            voice->setSubOsc(
                static_cast<int>(safeLoad("ppg_sub_wave", 0.0f)),
                subOctaveVal,
                safeLoad("ppg_sub_level", 0.0f) / 127.0f);

            // Noise — use mapped color
            voice->setNoise(
                safeLoad("ppg_noise_level", 0.0f) / 127.0f,
                noiseColorVal);

            // Mixer — normalize 0-127 → 0-1
            voice->setMixer(
                safeLoad("ppg_mix_waveA", 1.0f) / 127.0f,
                safeLoad("ppg_mix_waveB", 0.0f) / 127.0f,
                safeLoad("ppg_mix_sub", 0.0f) / 127.0f,
                safeLoad("ppg_mix_noise", 0.0f) / 127.0f,
                safeLoad("ppg_mix_ringmod", 0.0f) / 127.0f);

            // Filter — normalize resonance 0-127→0-1, env amt ±127→±1, lfo amt 0-127→0-1
            voice->setFilter(
                safeLoad("ppg_vcf_cutoff", 10000.0f),
                safeLoad("ppg_vcf_resonance", 0.0f) / 127.0f,
                safeLoad("ppg_vcf_env_amt", 0.0f) / 127.0f,
                safeLoad("ppg_vcf_lfo_amt", 0.0f) / 127.0f,
                keytrackFloat,
                filterMode);

            // VCA — normalize level 0-127→0-1, vel sens 0-7→0-1
            voice->setVCA(
                safeLoad("ppg_vca_level", 1.0f) / 127.0f,
                safeLoad("ppg_vca_vel_sens", 0.5f) / 7.0f);

            // Envelopes: convert ms→seconds, sustain 0-127→0-1
            voice->setEnvelope(0,
                safeLoad("ppg_env1_attack", 50.0f) * 0.001f,
                safeLoad("ppg_env1_decay", 800.0f) * 0.001f,
                safeLoad("ppg_env1_sustain", 0.0f) / 127.0f,
                safeLoad("ppg_env1_release", 500.0f) * 0.001f);

            voice->setEnvelope(1,
                safeLoad("ppg_env2_attack", 20.0f) * 0.001f,
                safeLoad("ppg_env2_decay", 0.5f) * 0.001f,
                safeLoad("ppg_env2_sustain", 127.0f) / 127.0f,
                safeLoad("ppg_env2_release", 600.0f) * 0.001f);

            voice->setEnvelope(2,
                safeLoad("ppg_env3_attack", 300.0f) * 0.001f,
                safeLoad("ppg_env3_decay", 2000.0f) * 0.001f,
                safeLoad("ppg_env3_sustain", 38.0f) / 127.0f,
                safeLoad("ppg_env3_release", 1000.0f) * 0.001f);

            // LFO — normalize amounts 0-127→0-1
            voice->setLFO(
                static_cast<int>(safeLoad("ppg_lfo_wave", 0.0f)),
                safeLoad("ppg_lfo_rate", 1.0f),
                safeLoad("ppg_lfo_delay", 0.0f),
                safeLoad("ppg_lfo_pitch_amt", 0.0f) / 127.0f,
                safeLoad("ppg_lfo_cutoff_amt", 0.0f) / 127.0f,
                safeLoad("ppg_lfo_amp_amt", 0.0f) / 127.0f,
                safeLoad("ppg_lfo_sync", 0.0f) > 0.5f);

            // Performance
            voice->setBendRange(safeLoad("ppg_bend_range", 2.0f));
            voice->setPortamento(
                safeLoad("ppg_porta_time", 0.0f),
                static_cast<int>(safeLoad("ppg_porta_mode", 0.0f)));

            // Unison: per-voice detune spread and stereo pan
            if (voiceModeInt == 1 && synth.getNumVoices() > 1)
            {
                float t = static_cast<float>(v) / static_cast<float>(synth.getNumVoices() - 1);
                float spread = (t - 0.5f) * 2.0f;  // -1 to +1
                voice->setUnisonParams(spread * unisonDetuneCents, t);
            }
            else
            {
                voice->setUnisonParams(0.0f, 0.5f);
            }
        }
    }
}

} // namespace axelf::ppgwave
