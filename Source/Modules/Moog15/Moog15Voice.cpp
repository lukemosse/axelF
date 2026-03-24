#include "Moog15Voice.h"
#include <cmath>
#include <algorithm>

namespace axelf::moog15
{

static inline float softClip(float x)
{
    if (x > 1.5f)  return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - (x * x * x) / 6.75f;
}

bool Moog15Voice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<Moog15Sound*>(sound) != nullptr;
}

void Moog15Voice::startNote(int midiNoteNumber, float vel,
                             juce::SynthesiserSound*, int pitchWheel)
{
    velocity = vel;

    float freq = static_cast<float>(
        juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

    glideTarget = freq;
    pitchBendSemitones = ((static_cast<float>(pitchWheel) - 8192.0f) / 8192.0f) * 2.0f;

    // If glide is off or this is the first note, jump immediately
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

    // On the very first note the SmoothedValues have never been initialised,
    // so we must set the smoothing time once.  On subsequent retriggers we
    // just update the target so the value glides smoothly — calling reset()
    // would jump currentValue to the old target, causing a parameter
    // discontinuity in the filter.
    if (!smoothingInitialised)
    {
        cutoffSmoothed.reset(getSampleRate(), 0.02);
        resoSmoothed.reset(getSampleRate(), 0.02);
        driveSmoothed.reset(getSampleRate(), 0.02);
        mixVco1Smoothed.reset(getSampleRate(), 0.005);
        mixVco2Smoothed.reset(getSampleRate(), 0.005);
        mixVco3Smoothed.reset(getSampleRate(), 0.005);
        cutoffSmoothed.setCurrentAndTargetValue(vcfBaseCutoff);
        resoSmoothed.setCurrentAndTargetValue(vcfResonance);
        driveSmoothed.setCurrentAndTargetValue(vcfDrive);
        mixVco1Smoothed.setCurrentAndTargetValue(mixVco1Level);
        mixVco2Smoothed.setCurrentAndTargetValue(mixVco2Level);
        mixVco3Smoothed.setCurrentAndTargetValue(mixVco3Level);
        smoothingInitialised = true;
    }
    else
    {
        cutoffSmoothed.setTargetValue(vcfBaseCutoff);
        resoSmoothed.setTargetValue(vcfResonance);
        driveSmoothed.setTargetValue(vcfDrive);
        mixVco1Smoothed.setTargetValue(mixVco1Level);
        mixVco2Smoothed.setTargetValue(mixVco2Level);
        mixVco3Smoothed.setTargetValue(mixVco3Level);
    }
}

void Moog15Voice::stopNote(float /*vel*/, bool allowTailOff)
{
    env1.noteOff();
    env2.noteOff();

    if (!allowTailOff)
    {
        // Don't reset envelopes to zero — this is a mono synth, so voice steal
        // is the normal retrigger path.  Keeping the current envelope level
        // avoids the hard amplitude discontinuity (click) at note boundaries.
        clearCurrentNote();
    }
}

void Moog15Voice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchBendSemitones = ((static_cast<float>(newPitchWheelValue) - 8192.0f) / 8192.0f) * 2.0f;
}

void Moog15Voice::controllerMoved(int controllerNumber, int newControllerValue)
{
    if (controllerNumber == 1)  // mod wheel
        modWheelValue = static_cast<float>(newControllerValue) / 127.0f;
}

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

        // Glide: smoothly approach target frequency
        if (glideRate > 0.0f && std::abs(currentFreq - glideTarget) > 0.01f)
        {
            float glideSpeed = 1.0f / (glideRate * static_cast<float>(getSampleRate()));
            currentFreq += (glideTarget - currentFreq) * glideSpeed;
        }
        else
        {
            currentFreq = glideTarget;
        }

        // LFO modulation
        float lfoVal = lfo.getNextSample();
        float totalLfoDepth = lfoDepthAmount + modWheelValue * (1.0f - lfoDepthAmount);
        float lfoPitchMod = totalLfoDepth * lfoVal * 0.02f;

        // Apply pitch bend + LFO
        float bendMul = std::pow(2.0f, pitchBendSemitones / 12.0f);
        float pitchMod = bendMul * (1.0f + lfoPitchMod);
        float detune2 = std::pow(2.0f, vco2DetuneCents / 1200.0f);
        float detune3 = std::pow(2.0f, vco3DetuneCents / 1200.0f);

        // LFO pitch modulation (Phase 2)
        float lfoPitchMod2 = lfoPitchAmt * lfoVal * 0.02f;
        float pitchMod2 = pitchMod * (1.0f + lfoPitchMod2);
        vco1.setFrequency(currentFreq * pitchMod2);
        vco2.setFrequency(currentFreq * pitchMod2 * detune2);
        vco3.setFrequency(currentFreq * pitchMod2 * detune3);

        float oscOut = vco1.getNextSample() * mixVco1Smoothed.getNextValue()
                     + vco2.getNextSample() * mixVco2Smoothed.getNextValue()
                     + vco3.getNextSample() * mixVco3Smoothed.getNextValue();

        // Noise generator (Phase 2)
        if (noiseLevel > 0.0f)
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            float noise = static_cast<float>(static_cast<int32_t>(noiseState))
                        / static_cast<float>(INT32_MAX);
            oscOut += noise * noiseLevel;
        }

        // VCF key tracking (Phase 2)
        float keyTrackOffset = 0.0f;
        if (vcfKeyTrackMode == 1)
            keyTrackOffset = (static_cast<float>(midiNoteNum) - 60.0f) * 50.0f;
        else if (vcfKeyTrackMode == 2)
            keyTrackOffset = (static_cast<float>(midiNoteNum) - 60.0f) * 100.0f;

        // Filter with envelope + LFO modulation + VCF LFO + key tracking
        float lfoFilterMod = lfoDepthAmount * lfoVal * 3000.0f;
        float vcfLfoMod = vcfLfoAmount * lfoVal * 2000.0f;
        float smCutoff = cutoffSmoothed.getNextValue();
        float smReso = resoSmoothed.getNextValue();
        float smDrive = driveSmoothed.getNextValue();
        float modCutoff = smCutoff + vcfEnvDepth * env1Val * 10000.0f
                        + lfoFilterMod + vcfLfoMod + keyTrackOffset;
        modCutoff = std::clamp(modCutoff, 20.0f, 20000.0f);
        vcf.setParameters(modCutoff, smReso, smDrive);

        float filtered = vcf.processSample(oscOut);
        float output = softClip(filtered * env2Val * velocity);

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
                                 float v1PW, float v2PW, float v3PW,
                                 float noiseLvl, int keyTrack,
                                 float vcfLfo, float lfoPitch)
{
    vco1.setWaveform(vco1Wave);
    vco1.setRange(vco1Range);
    vco1.setPulseWidth(v1PW);
    vco2.setWaveform(vco2Wave);
    vco2.setPulseWidth(v2PW);
    vco3.setWaveform(vco3Wave);
    vco3.setPulseWidth(v3PW);

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

    cutoffSmoothed.setTargetValue(cutoff);
    resoSmoothed.setTargetValue(reso);
    driveSmoothed.setTargetValue(drive);
    mixVco1Smoothed.setTargetValue(mixV1);
    mixVco2Smoothed.setTargetValue(mixV2);
    mixVco3Smoothed.setTargetValue(mixV3);

    env1.setParameters(e1A, e1D, e1S, e1R);
    env2.setParameters(e2A, e2D, e2S, e2R);

    glideRate = glideTime;

    lfo.setFrequency(lfoRate);
    lfoDepthAmount = lfoDepth;
    lfo.setWaveform(static_cast<axelf::dsp::LFOWaveform>(lfoWaveform));

    // Phase 2 params
    noiseLevel = noiseLvl;
    vcfKeyTrackMode = keyTrack;
    vcfLfoAmount = vcfLfo;
    lfoPitchAmt = lfoPitch;
}

} // namespace axelf::moog15
