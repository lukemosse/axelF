#include "JX3PVoice.h"
#include <cmath>
#include <algorithm>

namespace axelf::jx3p
{

static inline float softClip(float x)
{
    if (x > 1.5f)  return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - (x * x * x) / 6.75f;
}

bool JX3PVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<JX3PSound*>(sound) != nullptr;
}

void JX3PVoice::startNote(int midiNoteNumber, float vel,
                           juce::SynthesiserSound*, int pitchWheel)
{
    velocity = vel;
    midiNoteNum = midiNoteNumber;

    noteFrequency = static_cast<float>(
        juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

    // Portamento: if rate is zero, snap immediately
    if (portaRate <= 0.001f)
        portaFreq = noteFrequency;

    pitchBendSemitones = ((static_cast<float>(pitchWheel) - 8192.0f) / 8192.0f) * 2.0f;
    float bendMul = std::pow(2.0f, pitchBendSemitones / 12.0f);
    float detuneMul = std::pow(2.0f, dco2DetuneCents / 1200.0f);

    dco1.setSampleRate(getSampleRate());
    dco2.setSampleRate(getSampleRate());
    vcf.setSampleRate(getSampleRate());
    env1.setSampleRate(getSampleRate());
    env2.setSampleRate(getSampleRate());
    lfo.setSampleRate(getSampleRate());

    dco1.setFrequency(portaFreq * bendMul);
    dco2.setFrequency(portaFreq * bendMul * detuneMul);

    // LFO delay: reset ramp
    lfoDelayRamp = (lfoDelayTime <= 0.001f) ? 1.0f : 0.0f;
    lfoDelaySamples = 0.0;
    lfoDelayTarget = static_cast<double>(lfoDelayTime) * getSampleRate();

    env1.noteOn();
    env2.noteOn();

    // Reset filter + DSP state to prevent stale transient clicks
    vcf.reset();
    prevDco2Out = 0.0f;
    antiClickCounter = 0;

    // Reset smoothed values at voice start
    cutoffSmoothed.reset(getSampleRate(), 0.02);
    resoSmoothed.reset(getSampleRate(), 0.02);
    mix1Smoothed.reset(getSampleRate(), 0.005);
    mix2Smoothed.reset(getSampleRate(), 0.005);
    cutoffSmoothed.setCurrentAndTargetValue(vcfBaseCutoff);
    resoSmoothed.setCurrentAndTargetValue(vcfResonance);
    mix1Smoothed.setCurrentAndTargetValue(mixDco1Level);
    mix2Smoothed.setCurrentAndTargetValue(mixDco2Level);
}

void JX3PVoice::stopNote(float /*vel*/, bool allowTailOff)
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

void JX3PVoice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchBendSemitones = ((static_cast<float>(newPitchWheelValue) - 8192.0f) / 8192.0f) * 2.0f;
}

void JX3PVoice::controllerMoved(int controllerNumber, int newControllerValue)
{
    if (controllerNumber == 1)  // mod wheel
        modWheelValue = static_cast<float>(newControllerValue) / 127.0f;
}

void JX3PVoice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                 int startSample, int numSamples)
{
    const float sr = static_cast<float>(getSampleRate());

    // Portamento smoothing coefficient
    float portaCoeff = 1.0f;
    if (portaRate > 0.001f)
        portaCoeff = 1.0f - std::exp(-1.0f / (portaRate * sr));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float env1Val = env1.getNextSample();
        float env2Val = env2.getNextSample();

        if (!env2.isActive())
        {
            clearCurrentNote();
            break;
        }

        // Portamento: glide portaFreq toward noteFrequency
        portaFreq += (noteFrequency - portaFreq) * portaCoeff;

        // LFO with delay ramp
        float lfoVal = lfo.getNextSample();
        if (lfoDelayRamp < 1.0f && lfoDelayTarget > 0.0)
        {
            lfoDelaySamples += 1.0;
            lfoDelayRamp = static_cast<float>(
                std::min(lfoDelaySamples / lfoDelayTarget, 1.0));
        }
        float effectiveLfo = lfoVal * lfoDelayRamp;

        float totalLfoDepth = lfoDepthAmount + modWheelValue * (1.0f - lfoDepthAmount);
        float lfoPitchMod = totalLfoDepth * effectiveLfo * 0.02f;

        float bendMul = std::pow(2.0f, pitchBendSemitones / 12.0f);
        float pitchMod = bendMul * (1.0f + lfoPitchMod);
        float detuneMul = std::pow(2.0f, dco2DetuneCents / 1200.0f);

        // Cross-modulation: DCO-2 FMs DCO-1
        float crossModMul = 1.0f + crossMod * prevDco2Out;

        dco1.setFrequency(portaFreq * pitchMod * crossModMul);
        dco2.setFrequency(portaFreq * pitchMod * detuneMul);

        float dco1Out = dco1.getNextSample();

        // Hard sync: reset DCO-2 when DCO-1 wraps
        if (syncEnabled && dco1.didPhaseWrap())
            dco2.resetPhase();

        float dco2Out = dco2.getNextSample();
        prevDco2Out = dco2Out;

        float oscOut = dco1Out * mix1Smoothed.getNextValue() + dco2Out * mix2Smoothed.getNextValue();

        // Noise generator
        if (noiseLevel > 0.001f)
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            float noiseOut = static_cast<float>(static_cast<int32_t>(noiseState)) / 2147483648.0f;
            oscOut += noiseOut * noiseLevel;
        }

        // VCF key tracking
        float keyTrackOffset = 0.0f;
        if (vcfKeyTrackMode == 1)
            keyTrackOffset = (static_cast<float>(midiNoteNum) - 60.0f) * 50.0f;
        else if (vcfKeyTrackMode == 2)
            keyTrackOffset = (static_cast<float>(midiNoteNum) - 60.0f) * 100.0f;

        // Filter with env1 + multiplicative VCF LFO + key tracking
        float lfoFilterMul = std::pow(2.0f, vcfLfoAmount * effectiveLfo * 2.0f); // ±2 octaves
        float smCutoff = cutoffSmoothed.getNextValue();
        float smReso = resoSmoothed.getNextValue();
        float modCutoff = (smCutoff * lfoFilterMul)
                        + vcfEnvDepth * env1Val * 10000.0f
                        + keyTrackOffset;
        modCutoff = std::clamp(modCutoff, 20.0f, 20000.0f);
        vcf.setParameters(modCutoff, smReso);

        float filtered = vcf.processSample(oscOut);
        float output = softClip(filtered * env2Val * velocity);

        // Anti-click fade-in over 32 samples
        if (antiClickCounter >= 0 && antiClickCounter < 32)
        {
            output *= static_cast<float>(antiClickCounter) / 32.0f;
            ++antiClickCounter;
            if (antiClickCounter >= 32)
                antiClickCounter = -1;
        }

        int pos = startSample + sample;
        buffer.addSample(0, pos, output);
        buffer.addSample(1, pos, output);
    }
}

void JX3PVoice::setParameters(int dco1Wave, int dco2Wave, float dco2Detune,
                               float cutoff, float reso, float envDepth,
                               float e1A, float e1D, float e1S, float e1R,
                               float e2A, float e2D, float e2S, float e2R,
                               float lfoRate, float lfoDepth, int lfoWaveform,
                               float dco1Range, float mixD1, float mixD2,
                               float vcfLfo, int vcfKeyTrk,
                               float lfoDly, float crossModD, int dcoSyncOn,
                               float pw, float dco2PW,
                               float noiseLvl, float porta)
{
    dco1.setWaveform(dco1Wave);
    dco1.setRange(dco1Range);
    dco1.setPulseWidth(pw);
    dco2.setWaveform(dco2Wave);
    dco2.setPulseWidth(dco2PW);
    dco2DetuneCents = dco2Detune;

    vcfBaseCutoff = cutoff;
    vcfResonance = reso;
    vcfEnvDepth = envDepth;

    cutoffSmoothed.setTargetValue(cutoff);
    resoSmoothed.setTargetValue(reso);
    mix1Smoothed.setTargetValue(mixD1);
    mix2Smoothed.setTargetValue(mixD2);

    env1.setParameters(e1A, e1D, e1S, e1R);
    env2.setParameters(e2A, e2D, e2S, e2R);

    lfo.setFrequency(lfoRate);
    lfoDepthAmount = lfoDepth;
    lfo.setWaveform(static_cast<axelf::dsp::LFOWaveform>(lfoWaveform));

    // Phase 2 params
    mixDco1Level = mixD1;
    mixDco2Level = mixD2;
    vcfLfoAmount = vcfLfo;
    vcfKeyTrackMode = vcfKeyTrk;
    lfoDelayTime = lfoDly;
    crossMod = crossModD;
    syncEnabled = (dcoSyncOn != 0);
    noiseLevel = noiseLvl;
    portaRate = porta;
}

} // namespace axelf::jx3p
