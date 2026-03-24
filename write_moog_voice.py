import os

content = r'''#include "Moog15Voice.h"
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

with open(r'c:\axelF\Source\Modules\Moog15\Moog15Voice.cpp', 'w', newline='\n') as f:
    f.write(content)
print(f"Written {os.path.getsize(r'c:\\axelF\\Source\\Modules\\Moog15\\Moog15Voice.cpp')} bytes")
