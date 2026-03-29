#include "Jupiter8Processor.h"
#include "Jupiter8Voice.h"
#include "Jupiter8Editor.h"
#include <algorithm>

namespace axelf::jupiter8
{

Jupiter8Processor::Jupiter8Processor()
    : apvts(dummyProcessor, nullptr, "Jupiter8", createParameterLayout())
{
    // Add 8 voices for polyphonic mode
    for (int i = 0; i < 8; ++i)
    {
        auto* voice = new Jupiter8Voice();
        voice->setVoiceIndex(i);
        synth.addVoice(voice);
    }

    synth.addSound(new Jupiter8Sound());
}

void Jupiter8Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    oversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampledRate = sampleRate * static_cast<double>(oversampler.getOversamplingFactor());
    synth.setCurrentPlaybackSampleRate(oversampledRate);
}

void Jupiter8Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    updateVoiceParameters();

    // Update arpeggiator settings from APVTS
    {
        auto safeLoad = [&](const juce::String& id, float fb = 0.0f) -> float {
            if (auto* p = apvts.getRawParameterValue(id)) return p->load();
            return fb;
        };
        bool arpOn = static_cast<int>(safeLoad("jup8_arp_on")) > 0;
        int arpMode = static_cast<int>(safeLoad("jup8_arp_mode"));
        int arpRange = static_cast<int>(safeLoad("jup8_arp_range")) + 1; // 0-based → 1-3

        arpeggiator.setEnabled(arpOn);
        arpeggiator.setMode(static_cast<ArpMode>(arpMode));
        arpeggiator.setOctaveRange(arpRange);

        // Map arp rate choice to beats-per-step: 1/32=0.125, 1/16=0.25, 1/8=0.5, 1/4=1.0
        static constexpr double arpRateTable[] = { 0.125, 0.25, 0.5, 1.0 };
        int arpRateIdx = std::clamp(static_cast<int>(safeLoad("jup8_arp_rate")), 0, 3);
        arpeggiator.setRate(arpRateTable[arpRateIdx]);
    }

    // Process arpeggiator: transform MIDI if enabled
    juce::MidiBuffer arpMidi;
    double beatsPerSample = currentBpm / (60.0 * currentSampleRate);
    arpeggiator.process(midiMessages, arpMidi, blockStartBeat,
                        buffer.getNumSamples(), beatsPerSample);

    // Voice mode MIDI transformation
    juce::MidiBuffer voiceMidi;
    if (currentVoiceMode == 1) // Unison — duplicate each note-on 8 times
    {
        for (const auto metadata : arpMidi)
        {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                for (int v = 0; v < 8; ++v)
                    voiceMidi.addEvent(msg, metadata.samplePosition);
            }
            else
            {
                voiceMidi.addEvent(msg, metadata.samplePosition);
            }
        }
    }
    else if (currentVoiceMode >= 2) // Solo modes
    {
        for (const auto metadata : arpMidi)
        {
            auto msg = metadata.getMessage();
            int pos = metadata.samplePosition;
            if (msg.isNoteOn())
            {
                int note = msg.getNoteNumber();
                float vel = msg.getFloatVelocity();
                soloHeldNotes.push_back(note);
                soloHeldVelocities.push_back(vel);

                int winner = note;
                float winVel = vel;
                if (currentVoiceMode == 3) // Solo Low
                {
                    for (size_t i = 0; i < soloHeldNotes.size(); ++i)
                        if (soloHeldNotes[i] < winner) { winner = soloHeldNotes[i]; winVel = soloHeldVelocities[i]; }
                }
                else if (currentVoiceMode == 4) // Solo High
                {
                    for (size_t i = 0; i < soloHeldNotes.size(); ++i)
                        if (soloHeldNotes[i] > winner) { winner = soloHeldNotes[i]; winVel = soloHeldVelocities[i]; }
                }
                // Solo Last: winner is already the new note

                if (lastSoloNote >= 0 && lastSoloNote != winner)
                    voiceMidi.addEvent(juce::MidiMessage::noteOff(1, lastSoloNote), pos);
                voiceMidi.addEvent(juce::MidiMessage::noteOn(1, winner, winVel), pos);
                lastSoloNote = winner;
            }
            else if (msg.isNoteOff())
            {
                int note = msg.getNoteNumber();
                for (auto it = soloHeldNotes.begin(); it != soloHeldNotes.end(); ++it)
                {
                    if (*it == note)
                    {
                        auto idx = std::distance(soloHeldNotes.begin(), it);
                        soloHeldNotes.erase(it);
                        soloHeldVelocities.erase(soloHeldVelocities.begin() + idx);
                        break;
                    }
                }

                if (soloHeldNotes.empty())
                {
                    voiceMidi.addEvent(juce::MidiMessage::noteOff(1, lastSoloNote), pos);
                    lastSoloNote = -1;
                }
                else if (note == lastSoloNote)
                {
                    // Retrigger the new winner
                    int winner = soloHeldNotes.back();
                    float winVel = soloHeldVelocities.back();
                    if (currentVoiceMode == 3) // Solo Low
                        for (size_t i = 0; i < soloHeldNotes.size(); ++i)
                            if (soloHeldNotes[i] < winner) { winner = soloHeldNotes[i]; winVel = soloHeldVelocities[i]; }
                    if (currentVoiceMode == 4) // Solo High
                        for (size_t i = 0; i < soloHeldNotes.size(); ++i)
                            if (soloHeldNotes[i] > winner) { winner = soloHeldNotes[i]; winVel = soloHeldVelocities[i]; }

                    voiceMidi.addEvent(juce::MidiMessage::noteOff(1, lastSoloNote), pos);
                    voiceMidi.addEvent(juce::MidiMessage::noteOn(1, winner, winVel), pos);
                    lastSoloNote = winner;
                }
            }
            else
            {
                voiceMidi.addEvent(msg, pos);
            }
        }
    }
    else // Poly — pass through
    {
        voiceMidi = arpMidi;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampler.processSamplesUp(block);

    float* channels[] = { oversampledBlock.getChannelPointer(0),
                          oversampledBlock.getChannelPointer(1) };
    juce::AudioBuffer<float> osBuffer(channels, 2,
                                       static_cast<int>(oversampledBlock.getNumSamples()));
    synth.renderNextBlock(osBuffer, voiceMidi, 0, static_cast<int>(oversampledBlock.getNumSamples()));

    oversampler.processSamplesDown(block);
}

void Jupiter8Processor::releaseResources()
{
    // Nothing to release
}

juce::Component* Jupiter8Processor::createEditor()
{
    return nullptr;  // Editors now created in PluginEditor with PatternEngine refs
}

void Jupiter8Processor::updateVoiceParameters()
{
    auto safeLoad = [&](const juce::String& id, float fallback = 0.0f) -> float
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return p->load();
        return fallback;
    };

    float dco1Wave    = safeLoad("jup8_dco1_waveform");
    float dco1Range   = safeLoad("jup8_dco1_range", 8.0f);
    float dco2Wave    = safeLoad("jup8_dco2_waveform");
    float dco2Range   = safeLoad("jup8_dco2_range", 8.0f);
    float dco2Detune  = safeLoad("jup8_dco2_detune", 6.0f);
    float mixDco1     = safeLoad("jup8_mix_dco1", 0.7f);
    float mixDco2     = safeLoad("jup8_mix_dco2", 0.5f);
    float vcfCutoff   = safeLoad("jup8_vcf_cutoff", 13000.0f);
    float vcfReso     = safeLoad("jup8_vcf_resonance", 0.15f);
    float vcfEnvDepth = safeLoad("jup8_vcf_env_depth", 0.3f);
    float e1A = safeLoad("jup8_env1_attack", 0.01f);
    float e1D = safeLoad("jup8_env1_decay", 0.3f);
    float e1S = safeLoad("jup8_env1_sustain", 0.0f);
    float e1R = safeLoad("jup8_env1_release", 0.3f);
    float e2A = safeLoad("jup8_env2_attack", 0.01f);
    float e2D = safeLoad("jup8_env2_decay", 0.3f);
    float e2S = safeLoad("jup8_env2_sustain", 0.6f);
    float e2R = safeLoad("jup8_env2_release", 0.3f);

    float lfoRate     = safeLoad("jup8_lfo_rate", 5.5f);
    float lfoDepth    = safeLoad("jup8_lfo_depth", 0.0f);
    int   lfoWaveform = static_cast<int>(safeLoad("jup8_lfo_waveform", 0.0f));

    // Phase 2 stub parameters
    float pulseWidth    = safeLoad("jup8_pulse_width", 0.5f);
    float subLevel      = safeLoad("jup8_sub_level", 0.0f);
    float noiseLevel    = safeLoad("jup8_noise_level", 0.0f);
    int   vcfKeyTrack   = static_cast<int>(safeLoad("jup8_vcf_key_track", 0.0f));
    float vcfLfoAmount  = safeLoad("jup8_vcf_lfo_amount", 0.0f);
    float lfoDelay      = safeLoad("jup8_lfo_delay", 0.0f);
    float portamento    = safeLoad("jup8_portamento", 0.0f);
    float crossModDepth = safeLoad("jup8_dco_cross_mod", 0.0f);
    int   dcoSync       = static_cast<int>(safeLoad("jup8_dco_sync", 0.0f));
    int   voiceMode     = static_cast<int>(safeLoad("jup8_voice_mode", 0.0f));
    int   hpfMode       = static_cast<int>(safeLoad("jup8_vcf_hpf", 0.0f));

    currentVoiceMode = voiceMode;

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<Jupiter8Voice*>(synth.getVoice(i)))
        {
            // In Unison mode, spread detune ±25 cents across 8 voices
            float unisonDetune = 0.0f;
            if (voiceMode == 1)
                unisonDetune = (static_cast<float>(i) - 3.5f) * (50.0f / 7.0f); // ±25 cents

            voice->setUnisonDetune(unisonDetune);
            voice->setParameters(dco1Wave, dco1Range,
                                 dco2Wave, dco2Range, dco2Detune,
                                 mixDco1, mixDco2,
                                 vcfCutoff, vcfReso, vcfEnvDepth,
                                 e1A, e1D, e1S, e1R,
                                 e2A, e2D, e2S, e2R,
                                 lfoRate, lfoDepth, lfoWaveform,
                                 pulseWidth, subLevel, noiseLevel,
                                 vcfKeyTrack, vcfLfoAmount, lfoDelay,
                                 portamento, crossModDepth, dcoSync,
                                 voiceMode, hpfMode);
        }
    }
}

} // namespace axelf::jupiter8

