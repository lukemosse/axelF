#include "PPGWaveVoice.h"
#include <cmath>
#include <algorithm>

namespace axelf::ppgwave
{

static constexpr float kTwoPi = 6.2831853071795864f;

bool PPGWaveVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PPGWaveSound*>(sound) != nullptr;
}

void PPGWaveVoice::startNote(int midiNoteNumber, float vel,
                              juce::SynthesiserSound*, int pitchWheel)
{
    currentVelocity = vel;
    currentNote = midiNoteNumber;
    noteFrequency = static_cast<float>(
        juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
    pitchBendSemitones = ((static_cast<float>(pitchWheel) - 8192.0f) / 8192.0f) * bendRangeSemitones;

    float sr = static_cast<float>(getSampleRate());

    oscA.setSampleRate(getSampleRate());
    oscB.setSampleRate(getSampleRate());
    oscA.reset();
    oscB.reset();
    filter.setSampleRate(getSampleRate());
    if (!voiceWasStolen)
        filter.reset();
    lfo.setSampleRate(getSampleRate());

    for (auto& env : envelopes)
    {
        env.setSampleRate(getSampleRate());
        env.noteOn();
    }

    // Anti-click: fade in if voice was stolen, instant start if fresh
    antiClickGain = voiceWasStolen ? 0.0f : 1.0f;
    voiceWasStolen = false;

    samplesSinceNoteOn = 0;
    subPhase = 0.0;
    noiseLPState = 0.0f;
    smoothWavePosA = 0.0f;
    smoothWavePosB = 0.0f;

    if (lfoSync)
        lfo.reset();

    // Portamento
    if (portaMode > 0 && portaTime > 0.001f)
    {
        portaActive = true;
        float portaSamples = portaTime * sr;
        portaRate = 1.0f / std::max(1.0f, portaSamples);
    }
    else
    {
        portaActive = false;
        portaFreq = noteFrequency;
    }
}

void PPGWaveVoice::stopNote(float /*vel*/, bool allowTailOff)
{
    for (auto& env : envelopes)
        env.noteOff();

    if (!allowTailOff)
    {
        voiceWasStolen = true;
        for (auto& env : envelopes)
            env.reset();
        clearCurrentNote();
    }
}

void PPGWaveVoice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchBendSemitones = ((static_cast<float>(newPitchWheelValue) - 8192.0f) / 8192.0f) * bendRangeSemitones;
}

void PPGWaveVoice::controllerMoved(int, int) {}

void PPGWaveVoice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                    int startSample, int numSamples)
{
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Check if VCA envelope is still active
        if (!envelopes[1].isActive())
        {
            clearCurrentNote();
            break;
        }

        // Envelope values: 0=VCF, 1=VCA, 2=Wave
        float env0 = envelopes[0].getNextSample();
        float env1 = envelopes[1].getNextSample();
        float env2 = envelopes[2].getNextSample();

        // LFO with delay fade-in
        float lfoVal = lfo.getNextSample();
        if (lfoNegate) lfoVal = -lfoVal;
        float lfoFade = (lfoDelaySamples > 0)
            ? std::min(1.0f, static_cast<float>(samplesSinceNoteOn) / static_cast<float>(lfoDelaySamples))
            : 1.0f;
        float fadedLfo = lfoVal * lfoFade;
        ++samplesSinceNoteOn;

        // Portamento smoothing
        if (portaActive)
        {
            portaFreq += (noteFrequency - portaFreq) * portaRate;
            if (std::abs(noteFrequency - portaFreq) < 0.01f)
            {
                portaFreq = noteFrequency;
                portaActive = false;
            }
        }
        else
        {
            portaFreq = noteFrequency;
        }

        // Pitch: bend + LFO pitch mod
        float pitchMod = std::pow(2.0f, (pitchBendSemitones + fadedLfo * lfoPitchAmt * 12.0f) / 12.0f);
        float baseFreq = portaFreq * pitchMod;

        // Update oscillator frequencies
        // Range idx: 0="64'" (-3 oct), 1="32'" (-2), 2="16'" (-1), 3="8'" (0), 4="4'" (+1), 5="2'" (+2)
        float freqA = baseFreq * std::pow(2.0f, (static_cast<float>(oscARangeIdx - 3) * 12.0f
            + static_cast<float>(oscASemi) + oscADetuneCents / 100.0f) / 12.0f);
        float freqB = baseFreq * std::pow(2.0f, (static_cast<float>(oscBRangeIdx - 3) * 12.0f
            + static_cast<float>(oscBSemi) + oscBDetuneCents / 100.0f) / 12.0f);
        oscA.setFrequency(freqA);
        oscB.setFrequency(freqB);

        // Wave position modulation: env3 + LFO — set BEFORE processSample
        float wavePosModA = oscAEnvAmt * env2 * 63.0f + oscALfoAmt * fadedLfo * 32.0f;
        float wavePosModB = oscBEnvAmt * env2 * 63.0f + oscBLfoAmt * fadedLfo * 32.0f;

        // Slew-rate-limited wave position to prevent >2 position jumps per sample
        float targetPosA = std::clamp(oscABasePos + wavePosModA, 0.0f, 63.0f);
        float targetPosB = std::clamp(oscBBasePos + wavePosModB, 0.0f, 63.0f);
        smoothWavePosA += std::clamp(targetPosA - smoothWavePosA, -2.0f, 2.0f);
        smoothWavePosB += std::clamp(targetPosB - smoothWavePosB, -2.0f, 2.0f);
        oscA.setWavePosition(smoothWavePosA);
        oscB.setWavePosition(smoothWavePosB);

        // Generate oscillators
        float sampleA = oscA.processSample() * oscALevel;
        float sampleB = oscB.processSample() * oscBLevel;

        // Sub oscillator (relative to WaveA frequency)
        subPhaseInc = static_cast<double>(freqA * std::pow(2.0f, static_cast<float>(subOctave))) / getSampleRate();
        float subSample = generateSubSample() * subLevel;

        // Noise
        float noiseSample = generateNoise() * noiseLevel;

        // Ring modulator: A * B
        float ringMod = sampleA * sampleB;

        // Mix sources
        float mixed = sampleA * mixA + sampleB * mixB + subSample * mixSub
                     + noiseSample * mixNoise + ringMod * mixRing;

        // Filter modulation
        float filterFreq = filterCutoff;
        filterFreq *= std::pow(2.0f, filterEnvAmt * env0 * 4.0f);       // env +-4 octaves
        filterFreq *= std::pow(2.0f, filterLfoAmt * fadedLfo * 2.0f);   // lfo +-2 octaves
        // Keytrack: scale from middle C (note 60)
        filterFreq *= std::pow(2.0f, filterKeytrack * (static_cast<float>(currentNote) - 60.0f) / 12.0f);
        filterFreq = std::clamp(filterFreq, 20.0f, 20000.0f);

        filter.setParameters(filterFreq, filter.getResonance(), filter.getMode());
        float filtered = filter.processSample(mixed);

        // VCA: envelope * level * velocity * LFO amp mod
        float ampMod = 1.0f - lfoAmpAmt * lfoFade * (1.0f - lfoVal) * 0.5f;
        float velGain = 1.0f - vcaVelSens * (1.0f - currentVelocity);
        float output = filtered * env1 * vcaLevel * velGain * ampMod;

        // Soft clip to prevent digital overs
        output = std::tanh(output);

        // Anti-click fade-in (ramp over ~32 samples on voice steal)
        if (antiClickGain < 1.0f)
        {
            output *= antiClickGain;
            antiClickGain = std::min(1.0f, antiClickGain + (1.0f / 32.0f));
        }

        int pos = startSample + sample;
        buffer.addSample(0, pos, output);
        buffer.addSample(1, pos, output);
    }
}

float PPGWaveVoice::generateSubSample()
{
    subPhase += subPhaseInc;
    if (subPhase >= 1.0) subPhase -= 1.0;

    if (subWave == 0)  // square
        return subPhase < 0.5 ? 1.0f : -1.0f;
    else  // sine
        return std::sin(static_cast<float>(subPhase) * kTwoPi);
}

float PPGWaveVoice::generateNoise()
{
    // White noise via JUCE Random
    float white = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);
    // Simple LP filter for color control (0=dark, 1=bright)
    noiseLPState += (white - noiseLPState) * (0.1f + noiseColor * 0.9f);
    return noiseLPState;
}

// --- Parameter setters ---

void PPGWaveVoice::setOscAParams(int table, float pos, float envAmt, float lfoAmt,
                                  int range, int semi, float detune, bool kt, float level)
{
    oscA.setWavetable(table);
    oscABasePos = pos;
    oscA.setWavePosition(pos);
    oscAEnvAmt = envAmt;
    oscALfoAmt = lfoAmt;
    oscA.setKeytrack(kt);
    oscALevel = level;
    oscARangeIdx = range;
    oscASemi = semi;
    oscADetuneCents = detune;
}

void PPGWaveVoice::setOscBParams(int table, float pos, float envAmt, float lfoAmt,
                                  int range, int semi, float detune, bool kt, float level)
{
    oscB.setWavetable(table);
    oscBBasePos = pos;
    oscB.setWavePosition(pos);
    oscBEnvAmt = envAmt;
    oscBLfoAmt = lfoAmt;
    oscB.setKeytrack(kt);
    oscBLevel = level;
    oscBRangeIdx = range;
    oscBSemi = semi;
    oscBDetuneCents = detune;
}

void PPGWaveVoice::setSubOsc(int wave, int octave, float level)
{
    subWave = wave;
    subOctave = octave;
    subLevel = level;
}

void PPGWaveVoice::setNoise(float level, float color)
{
    noiseLevel = level;
    noiseColor = color;
}

void PPGWaveVoice::setMixer(float waveA, float waveB, float sub, float noise, float ringmod)
{
    mixA = waveA;
    mixB = waveB;
    mixSub = sub;
    mixNoise = noise;
    mixRing = ringmod;
}

void PPGWaveVoice::setFilter(float cutoffHz, float resonance, float envAmt, float lfoAmt,
                              float keytrk, SSM2044Filter::Mode filterMode)
{
    filterCutoff = cutoffHz;
    filterEnvAmt = envAmt;
    filterLfoAmt = lfoAmt;
    filterKeytrack = keytrk;
    filter.setParameters(cutoffHz, resonance, filterMode);
}

void PPGWaveVoice::setVCA(float level, float velSens)
{
    vcaLevel = level;
    vcaVelSens = velSens;
}

void PPGWaveVoice::setEnvelope(int envIdx, float a, float d, float s, float r)
{
    if (envIdx >= 0 && envIdx < 3)
        envelopes[static_cast<size_t>(envIdx)].setParameters(a, d, s, r);
}

void PPGWaveVoice::setLFO(int waveform, float rate, float delay,
                           float pitchAmt, float cutoffAmt, float ampAmt, bool sync)
{
    // PPG params: 0=Tri, 1=Saw, 2=RevSaw, 3=Square, 4=Sine, 5=S&H
    static constexpr axelf::dsp::LFOWaveform waveMap[] = {
        axelf::dsp::LFOWaveform::Triangle,
        axelf::dsp::LFOWaveform::Sawtooth,
        axelf::dsp::LFOWaveform::Sawtooth,      // RevSaw — negate output in render
        axelf::dsp::LFOWaveform::Square,
        axelf::dsp::LFOWaveform::Sine,
        axelf::dsp::LFOWaveform::SampleAndHold,
    };
    int idx = std::clamp(waveform, 0, 5);
    lfo.setWaveform(waveMap[idx]);
    lfoNegate = (idx == 2);
    lfo.setFrequency(rate);
    lfoPitchAmt = pitchAmt;
    lfoCutoffAmt = cutoffAmt;
    lfoAmpAmt = ampAmt;
    lfoSync = sync;
    lfoDelaySamples = static_cast<int>(delay * static_cast<float>(getSampleRate()));
}

void PPGWaveVoice::setBendRange(float semitones)
{
    bendRangeSemitones = semitones;
}

void PPGWaveVoice::setPortamento(float time, int mode)
{
    portaTime = time;
    portaMode = mode;
}

} // namespace axelf::ppgwave
