#include "Jupiter8Voice.h"
#include <cmath>
#include <algorithm>

namespace axelf::jupiter8
{

// Soft-clip to ±1.0 using fast tanh approximation (no harsh digital clipping)
static inline float softClip(float x)
{
    if (x > 1.5f)  return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - (x * x * x) / 6.75f;   // cubic soft-knee, unity at ±1.0
}

bool Jupiter8Voice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<Jupiter8Sound*>(sound) != nullptr;
}

void Jupiter8Voice::startNote(int midiNoteNumber, float vel,
                               juce::SynthesiserSound*, int pitchWheel)
{
    velocity = vel;
    midiNoteNum = midiNoteNumber;

    noteFrequency = static_cast<float>(
        juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

    // Portamento: if rate is zero, snap immediately
    if (portaRate <= 0.001f)
        portaFreq = noteFrequency;

    // Apply initial pitch bend (0x2000 = 8192 = centre, no bend)
    pitchBendSemitones = ((static_cast<float>(pitchWheel) - 8192.0f) / 8192.0f) * 2.0f;

    float bendMultiplier = std::pow(2.0f, pitchBendSemitones / 12.0f);
    float detuneMultiplier = std::pow(2.0f, dco2DetuneCents / 1200.0f);

    dco1.setSampleRate(getSampleRate());
    dco2.setSampleRate(getSampleRate());
    vcf.setSampleRate(getSampleRate());
    env1.setSampleRate(getSampleRate());
    env2.setSampleRate(getSampleRate());
    lfo.setSampleRate(getSampleRate());

    dco1.setFrequency(portaFreq * bendMultiplier);
    dco2.setFrequency(portaFreq * bendMultiplier * detuneMultiplier);

    // LFO delay: reset ramp
    lfoDelayRamp = (lfoDelayTime <= 0.001f) ? 1.0f : 0.0f;
    lfoDelaySamples = 0.0;
    lfoDelayTarget = static_cast<double>(lfoDelayTime) * getSampleRate();

    env1.noteOn();
    env2.noteOn();

    // Reset filter + DSP state to prevent stale transient clicks
    vcf.reset();
    subPhase = 0.0f;
    hpfPrevIn = 0.0f;
    hpfPrevOut = 0.0f;
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

void Jupiter8Voice::stopNote(float /*vel*/, bool allowTailOff)
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

void Jupiter8Voice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchBendSemitones = ((static_cast<float>(newPitchWheelValue) - 8192.0f) / 8192.0f) * 2.0f;
}

void Jupiter8Voice::controllerMoved(int controllerNumber, int newControllerValue)
{
    if (controllerNumber == 1)  // mod wheel
        modWheelValue = static_cast<float>(newControllerValue) / 127.0f;
}

void Jupiter8Voice::renderNextBlock(juce::AudioBuffer<float>& buffer,
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

        // Mod wheel (CC1) adds vibrato depth on top of LFO panel depth
        float totalLfoDepth = lfoDepthAmount + modWheelValue * (1.0f - lfoDepthAmount);
        float lfoPitchMod = totalLfoDepth * effectiveLfo * 0.02f;

        // Pitch bend + LFO pitch modulation + unison detune
        float bendMultiplier = std::pow(2.0f, pitchBendSemitones / 12.0f);
        float unisonMul = std::pow(2.0f, unisonDetuneCents / 1200.0f);
        float pitchMod = bendMultiplier * unisonMul * (1.0f + lfoPitchMod);
        float detuneMultiplier = std::pow(2.0f, dco2DetuneCents / 1200.0f);

        // Cross-modulation: DCO-2 output FMs DCO-1 frequency
        float crossModMul = 1.0f + crossMod * prevDco2Out;

        dco1.setFrequency(portaFreq * pitchMod * crossModMul);
        dco2.setFrequency(portaFreq * pitchMod * detuneMultiplier);

        // Get DCO-1 sample (may set phaseWrapped flag for sync)
        float dco1Out = dco1.getNextSample();

        // Hard sync: reset DCO-2 when DCO-1 wraps
        if (syncEnabled && dco1.didPhaseWrap())
            dco2.resetPhase();

        // DCO-2 output — noise substitution for waveform index 2
        float dco2Out;
        if (dco2Waveform == 2)
        {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            dco2Out = static_cast<float>(static_cast<int32_t>(noiseState)) / 2147483648.0f;
        }
        else
        {
            dco2Out = dco2.getNextSample();
        }

        prevDco2Out = dco2Out;

        // Sub-oscillator: square wave one octave below DCO-1 with PolyBLEP
        float subOut = 0.0f;
        if (subLevel > 0.001f)
        {
            float subFreq = portaFreq * pitchMod * crossModMul * 0.5f;
            double subInc = static_cast<double>(subFreq) / static_cast<double>(sr);
            subPhase += static_cast<float>(subInc);
            if (subPhase >= 1.0f) subPhase -= 1.0f;

            // PolyBLEP anti-aliased square at sub frequency
            subOut = (subPhase < 0.5f) ? 1.0f : -1.0f;
            double dt = subInc;
            double t1 = static_cast<double>(subPhase);
            if (t1 < dt)      { double n = t1 / dt; subOut += static_cast<float>(n + n - n * n - 1.0); }
            else if (t1 > 1.0 - dt) { double n = (t1 - 1.0) / dt; subOut += static_cast<float>(n * n + n + n + 1.0); }
            double t2 = std::fmod(t1 + 0.5, 1.0);
            if (t2 < dt)      { double n = t2 / dt; subOut -= static_cast<float>(n + n - n * n - 1.0); }
            else if (t2 > 1.0 - dt) { double n = (t2 - 1.0) / dt; subOut -= static_cast<float>(n * n + n + n + 1.0); }
        }

        // White noise generator for noise mix (separate PRNG from DCO-2 noise)
        float noiseOut = 0.0f;
        if (noiseLevel > 0.001f)
        {
            noiseState2 ^= noiseState2 << 13;
            noiseState2 ^= noiseState2 >> 17;
            noiseState2 ^= noiseState2 << 5;
            noiseOut = static_cast<float>(static_cast<int32_t>(noiseState2)) / 2147483648.0f;
        }

        // Oscillator mix
        float smMix1 = mix1Smoothed.getNextValue();
        float smMix2 = mix2Smoothed.getNextValue();
        float oscOut = dco1Out * smMix1
                     + dco2Out * smMix2
                     + subOut  * subLevel
                     + noiseOut * noiseLevel;

        // HPF (1-pole high-pass) before VCF — Jupiter-8 Off/1/2/3
        if (hpfMode > 0)
        {
            // Corner frequencies: mode 1=250Hz, 2=500Hz, 3=1000Hz
            static constexpr float hpfFreqs[] = { 0.0f, 250.0f, 500.0f, 1000.0f };
            float fc = hpfFreqs[hpfMode];
            float rc = 1.0f / (2.0f * 3.14159265f * fc);
            float alpha = rc / (rc + 1.0f / sr);
            hpfPrevOut = alpha * (hpfPrevOut + oscOut - hpfPrevIn);
            hpfPrevIn = oscOut;
            oscOut = hpfPrevOut;
        }

        // VCF key tracking
        float keyTrackOffset = 0.0f;
        if (vcfKeyTrackMode == 1) // 50%
            keyTrackOffset = (static_cast<float>(midiNoteNum) - 60.0f) * 50.0f;
        else if (vcfKeyTrackMode == 2) // 100%
            keyTrackOffset = (static_cast<float>(midiNoteNum) - 60.0f) * 100.0f;

        // Filter with envelope + separate VCF LFO (multiplicative) + keyboard tracking
        float smCutoff = cutoffSmoothed.getNextValue();
        float smReso = resoSmoothed.getNextValue();
        float lfoFilterMul = std::pow(2.0f, vcfLfoAmount * effectiveLfo * 2.0f); // ±2 octaves
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

void Jupiter8Voice::setParameters(float dco1Wave, float dco1Range,
                                   float dco2Wave, float dco2Range, float dco2Detune,
                                   float mixD1, float mixD2,
                                   float cutoff, float reso, float envDepth,
                                   float e1A, float e1D, float e1S, float e1R,
                                   float e2A, float e2D, float e2S, float e2R,
                                   float lfoRate, float lfoDepth, int lfoWaveform,
                                   float pulseWidth, float subLvl, float noiseLvl,
                                   int vcfKeyTrk, float vcfLfo, float lfoDly,
                                   float porta, float crossModD, int dcoSyncOn,
                                   int voiceMode, int hpf)
{
    dco1.setWaveform(static_cast<int>(dco1Wave));
    dco1.setRange(dco1Range);
    dco1.setPulseWidth(pulseWidth);
    dco2.setWaveform(static_cast<int>(dco2Wave));
    dco2Waveform = static_cast<int>(dco2Wave);
    dco2.setRange(dco2Range);
    dco2.setPulseWidth(pulseWidth);
    dco2DetuneCents = dco2Detune;

    mixDco1Level = mixD1;
    mixDco2Level = mixD2;

    vcfBaseCutoff = cutoff;
    vcfResonance = reso;
    vcf.setParameters(cutoff, reso);
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
    subLevel = subLvl;
    noiseLevel = noiseLvl;
    vcfKeyTrackMode = vcfKeyTrk;
    vcfLfoAmount = vcfLfo;
    lfoDelayTime = lfoDly;
    portaRate = porta;
    crossMod = crossModD;
    syncEnabled = (dcoSyncOn != 0);
    hpfMode = hpf;
    juce::ignoreUnused(voiceMode);
}

} // namespace axelf::jupiter8
