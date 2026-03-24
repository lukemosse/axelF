import ctypes
import ctypes.wintypes as wintypes
import os

GENERIC_WRITE = 0x40000000
GENERIC_READ = 0x80000000
FILE_SHARE_READ = 0x00000001
FILE_SHARE_WRITE = 0x00000002
FILE_SHARE_DELETE = 0x00000004
CREATE_ALWAYS = 2
TRUNCATE_EXISTING = 5
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x00000080

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

def write_file_shared(filepath, content):
    data = content.encode('utf-8')
    handle = kernel32.CreateFileW(
        filepath,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        None,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        None
    )
    if handle == -1 or handle == 0xFFFFFFFF or handle == 0xFFFFFFFFFFFFFFFF:
        err = ctypes.get_last_error()
        print(f"FAILED: CreateFileW error {err} for {filepath}")
        return False
    
    written = wintypes.DWORD(0)
    buf = ctypes.create_string_buffer(data)
    result = kernel32.WriteFile(handle, buf, len(data), ctypes.byref(written), None)
    kernel32.CloseHandle(handle)
    
    if result:
        print(f"SUCCESS: Wrote {written.value} bytes to {filepath}")
        return True
    else:
        err = ctypes.get_last_error()
        print(f"FAILED: WriteFile error {err}")
        return False

# Read content from the Python files we already created
# But actually, let's just embed the content directly

moog15_content = '''#include "Moog15Voice.h"
#include <cmath>
#include <algorithm>

namespace axelf::moog15
{

bool Moog15Voice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<Moog15Sound*>(sound) != nullptr;
}

void Moog15Voice::startNote(int midiNoteNumber, float vel,
                             juce::SynthesiserSound*, int pitchWheel)
{
    velocity = vel;
    midiNoteNum = midiNoteNumber;

    float freq = static_cast<float>(
        juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

    glideTarget = freq;
    pitchBendSemitones = (static_cast<float>(pitchWheel) / 8192.0f) * 2.0f;

    if (glideRate <= 0.0f || !isVoiceActive())
        currentFreq = freq;

    noteFrequency = freq;

    vco1.setSampleRate(getSampleRate());
    vco2.setSampleRate(getSampleRate());
    vco3.setSampleRate(getSampleRate());
    vcf.setSampleRate(getSampleRate());
    env1.setSampleRate(getSampleRate());
    env2.setSampleRate(getSampleRate());
    lfo.setSampleRate(getSampleRate());

    float bendMul = std::pow(2.0f, pitchBendSemitones / 12.0f);
    float detune2 = std::pow(2.0f, vco2DetuneCents / 1200.0f);
    float detune3 = std::pow(2.0f, vco3DetuneCents / 1200.0f);

    vco1.setFrequency(currentFreq * bendMul);
    vco2.setFrequency(currentFreq * bendMul * detune2);
    vco3.setFrequency(currentFreq * bendMul * detune3);

    env1.noteOn();
    env2.noteOn();
}

void Moog15Voice::stopNote(float, bool allowTailOff)
{
    env1.noteOff();
    env2.noteOff();

    if (!allowTailOff)
    {
        env1.reset();
        env2.reset();
        clearCurrentNote();
    }
}

void Moog15Voice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchBendSemitones = (static_cast<float>(newPitchWheelValue) / 8192.0f) * 2.0f;
}

void Moog15Voice::controllerMoved(int, int) {}

void Moog15Voice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                   int startSample, int numSamples)
{
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float env2Val = env2.getNextSample();
        float env1Val = env1.getNextSample();

        if (!env2.isActive())
        {
            clearCurrentNote();
            break;
        }

        if (glideRate > 0.0f && std::abs(currentFreq - glideTarget) > 0.01f)
        {
            float glideSpeed = 1.0f / (glideRate * static_cast<float>(getSampleRate()));
            currentFreq += (glideTarget - currentFreq) * glideSpeed;
        }
        else
        {
            currentFreq = glideTarget;
        }

        float lfoVal = lfo.getNextSample();
        float lfoPitchMod = lfoPitchAmt * lfoVal * 0.02f;

        float bendMul = std::pow(2.0f, pitchBendSemitones / 12.0f);
        float pitchMod = bendMul * (1.0f + lfoPitchMod);
        float detune2 = std::pow(2.0f, vco2DetuneCents / 1200.0f);
        float detune3 = std::pow(2.0f, vco3DetuneCents / 1200.0f);

        vco1.setFrequency(currentFreq * pitchMod);
        vco2.setFrequency(currentFreq * pitchMod * detune2);
        vco3.setFrequency(currentFreq * pitchMod * detune3);

        float oscOut = vco1.getNextSample() * mixVco1Level
                     + vco2.getNextSample() * mixVco2Level
                     + vco3.getNextSample() * mixVco3Level;

        if (noiseLevel > 0.001f)
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            float noise = static_cast<float>(static_cast<int32_t>(noiseState)) / 2147483648.0f;
            oscOut += noise * noiseLevel;
        }

        float keyTrackOffset = 0.0f;
        if (vcfKeyTrackMode == 1)
            keyTrackOffset = (static_cast<float>(midiNoteNum) - 60.0f) * 50.0f;
        else if (vcfKeyTrackMode == 2)
            keyTrackOffset = (static_cast<float>(midiNoteNum) - 60.0f) * 100.0f;

        float lfoFilterMod = vcfLfoAmount * lfoVal * 3000.0f;
        float modCutoff = vcfBaseCutoff + vcfEnvDepth * env1Val * 10000.0f
                        + lfoFilterMod + keyTrackOffset;
        modCutoff = std::clamp(modCutoff, 20.0f, 20000.0f);
        vcf.setParameters(modCutoff, vcfResonance, vcfDrive);

        float filtered = vcf.processSample(oscOut);
        float output = filtered * env2Val * velocity;

        int pos = startSample + sample;
        buffer.addSample(0, pos, output);
        buffer.addSample(1, pos, output);
    }
}

void Moog15Voice::setParameters(int vco1Wave, float vco1Range,
                                 int vco2Wave, float vco2Detune,
                                 int vco3Wave, float vco3Detune,
                                 float mixV1, float mixV2, float mixV3,
                                 float cutoff, float reso, float drive, float envDepth,
                                 float e1A, float e1D, float e1S, float e1R,
                                 float e2A, float e2D, float e2S, float e2R,
                                 float glideTime,
                                 float lfoRate, float lfoDepth, int lfoWaveform,
                                 float vco1PW, float vco2PW, float vco3PW,
                                 float noiseLvl, int vcfKeyTrk,
                                 float vcfLfo, float lfoPitch)
{
    vco1.setWaveform(vco1Wave);
    vco1.setRange(vco1Range);
    vco1.setPulseWidth(vco1PW);
    vco2.setWaveform(vco2Wave);
    vco2.setPulseWidth(vco2PW);
    vco3.setWaveform(vco3Wave);
    vco3.setPulseWidth(vco3PW);

    vco2DetuneCents = vco2Detune;
    vco3DetuneCents = vco3Detune;

    mixVco1Level = mixV1;
    mixVco2Level = mixV2;
    mixVco3Level = mixV3;

    vcfBaseCutoff = cutoff;
    vcfResonance = reso;
    vcfDrive = drive;
    vcfEnvDepth = envDepth;
    vcf.setParameters(cutoff, reso, drive);

    env1.setParameters(e1A, e1D, e1S, e1R);
    env2.setParameters(e2A, e2D, e2S, e2R);

    glideRate = glideTime;

    lfo.setFrequency(lfoRate);
    lfoDepthAmount = lfoDepth;
    lfo.setWaveform(static_cast<axelf::dsp::LFOWaveform>(lfoWaveform));

    noiseLevel = noiseLvl;
    vcfKeyTrackMode = vcfKeyTrk;
    vcfLfoAmount = vcfLfo;
    lfoPitchAmt = lfoPitch;
}

} // namespace axelf::moog15
'''

jx3p_content = '''#include "JX3PProcessor.h"
#include "JX3PVoice.h"
#include "JX3PEditor.h"

namespace axelf::jx3p
{

JX3PProcessor::JX3PProcessor()
    : apvts(dummyProcessor, nullptr, "JX3P", createParameterLayout())
{
    for (int i = 0; i < 6; ++i)
        synth.addVoice(new JX3PVoice());

    synth.addSound(new JX3PSound());
}

void JX3PProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    synth.setCurrentPlaybackSampleRate(sampleRate);
    chorus.setSampleRate(sampleRate);
}

void JX3PProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    updateVoiceParameters();
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

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
        buffer.setSample(0, sample, l);
        buffer.setSample(1, sample, r);
    }
}

void JX3PProcessor::releaseResources() {}

juce::Component* JX3PProcessor::createEditor()
{
    return nullptr;
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

    float e1A = safeLoad("jx3p_env_attack", 0.01f);
    float e1D = safeLoad("jx3p_env_decay", 0.3f);
    float e1S = safeLoad("jx3p_env_sustain", 0.6f);
    float e1R = safeLoad("jx3p_env_release", 0.3f);

    float e2A = safeLoad("jx3p_env2_attack", 0.01f);
    float e2D = safeLoad("jx3p_env2_decay", 0.25f);
    float e2S = safeLoad("jx3p_env2_sustain", 0.7f);
    float e2R = safeLoad("jx3p_env2_release", 0.35f);

    float lfoRate     = safeLoad("jx3p_lfo_rate", 5.0f);
    float lfoDepth    = safeLoad("jx3p_lfo_depth", 0.0f);
    int   lfoWaveform = static_cast<int>(safeLoad("jx3p_lfo_waveform", 0.0f));

    float dco1Range     = safeLoad("jx3p_dco1_range", 8.0f);
    float mixDco1       = safeLoad("jx3p_mix_dco1", 0.5f);
    float mixDco2       = safeLoad("jx3p_mix_dco2", 0.5f);
    float vcfLfoAmount  = safeLoad("jx3p_vcf_lfo_amount", 0.0f);
    int   vcfKeyTrack   = static_cast<int>(safeLoad("jx3p_vcf_key_track", 0.0f));
    float lfoDelay      = safeLoad("jx3p_lfo_delay", 0.0f);
    float crossModDepth = safeLoad("jx3p_dco_cross_mod", 0.0f);
    int   dcoSync       = static_cast<int>(safeLoad("jx3p_dco_sync", 0.0f));
    float pulseWidth    = safeLoad("jx3p_pulse_width", 0.5f);
    float chorusSpread  = safeLoad("jx3p_chorus_spread", 0.5f);

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
                                 pulseWidth);
        }
    }
}

} // namespace axelf::jx3p
'''

write_file_shared(r'c:\axelF\Source\Modules\Moog15\Moog15Voice.cpp', moog15_content)
write_file_shared(r'c:\axelF\Source\Modules\JX3P\JX3PProcessor.cpp', jx3p_content)
