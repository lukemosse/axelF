#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "PPGWaveOsc.h"
#include "SSM2044Filter.h"
#include "../../Common/DSP/ADSREnvelope.h"
#include "../../Common/DSP/LFOGenerator.h"
#include <array>

namespace axelf::ppgwave
{

class PPGWaveSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class PPGWaveVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         int startSample, int numSamples) override;

    // Parameter updates from processor
    void setOscAParams(int table, float pos, float envAmt, float lfoAmt,
                       int range, int semi, float detune, bool keytrack, float level);
    void setOscBParams(int table, float pos, float envAmt, float lfoAmt,
                       int range, int semi, float detune, bool keytrack, float level);
    void setSubOsc(int wave, int octave, float level);
    void setNoise(float level, float color);
    void setMixer(float waveA, float waveB, float sub, float noise, float ringmod);
    void setFilter(float cutoffHz, float resonance, float envAmt, float lfoAmt,
                   float keytrack, SSM2044Filter::Mode filterMode);
    void setVCA(float level, float velSens);
    void setEnvelope(int envIdx, float a, float d, float s, float r);
    void setLFO(int waveform, float rate, float delay,
                float pitchAmt, float cutoffAmt, float ampAmt, bool sync);
    void setBendRange(float semitones);
    void setPortamento(float time, int portaMode);
    void setUnisonParams(float detuneOffsetCents, float pan);

private:
    // Two wavetable oscillators
    PPGWaveOsc oscA, oscB;

    // Sub oscillator (simple waveform)
    double subPhase = 0.0;
    double subPhaseInc = 0.0;
    int subWave = 0;      // 0=square, 1=sine
    int subOctave = -1;   // octave offset
    float subLevel = 0.0f;

    // Noise generator
    float noiseLevel = 0.0f;
    float noiseColor = 0.5f;  // 0=dark(filtered), 1=bright(white)

    // Paul Kellet pink noise filter state
    float pinkB0 = 0.0f, pinkB1 = 0.0f, pinkB2 = 0.0f;
    float pinkB3 = 0.0f, pinkB4 = 0.0f, pinkB5 = 0.0f;

    // Mixer levels
    float mixA = 1.0f, mixB = 0.0f, mixSub = 0.0f, mixNoise = 0.0f, mixRing = 0.0f;

    // Filter
    SSM2044Filter filter;
    float filterCutoff = 10000.0f;
    float filterEnvAmt = 0.0f;
    float filterLfoAmt = 0.0f;
    float filterKeytrack = 0.0f;

    // VCA
    float vcaLevel = 1.0f;
    float vcaVelSens = 0.5f;

    // Envelopes: 0=VCF, 1=VCA, 2=Wave
    std::array<axelf::dsp::ADSREnvelope, 3> envelopes;

    // LFO
    axelf::dsp::LFOGenerator lfo;
    float lfoPitchAmt = 0.0f;
    float lfoCutoffAmt = 0.0f;
    float lfoAmpAmt = 0.0f;
    int lfoDelaySamples = 0;
    int samplesSinceNoteOn = 0;
    bool lfoSync = false;

    // Performance
    float bendRangeSemitones = 2.0f;
    float pitchBendSemitones = 0.0f;
    float noteFrequency = 440.0f;
    float currentVelocity = 0.0f;
    int currentNote = 60;

    // Osc A/B envelope modulation amounts
    float oscAEnvAmt = 0.0f, oscALfoAmt = 0.0f;
    float oscBEnvAmt = 0.0f, oscBLfoAmt = 0.0f;
    float oscALevel = 1.0f, oscBLevel = 0.5f;
    float oscABasePos = 0.0f, oscBBasePos = 0.0f;

    // Osc pitch offsets
    int oscARangeIdx = 3;    // 0-5, default 8'
    int oscASemi = 0;
    float oscADetuneCents = 0.0f;
    int oscBRangeIdx = 3;
    int oscBSemi = 0;
    float oscBDetuneCents = 0.0f;

    // LFO reverse-saw flag
    bool lfoNegate = false;

    // Anti-click fade-in for voice stealing
    bool voiceWasStolen = false;
    float antiClickGain = 1.0f;

    // Stereo pan for unison spread (0=left, 0.5=center, 1=right)
    float stereoPan = 0.5f;

    // Unison detune offset in cents (set per-voice by processor)
    float unisonDetuneOffset = 0.0f;

    // Previous filter input for 2× oversampling
    float prevFilterInput = 0.0f;

    // Wave position smoothing (slew-rate limited)
    float smoothWavePosA = 0.0f;
    float smoothWavePosB = 0.0f;

    // Portamento
    float portaTime = 0.0f;
    int portaMode = 0;  // 0=off, 1=on, 2=legato
    float portaFreq = 440.0f;
    float portaRate = 0.0f;
    bool portaActive = false;

    float generateSubSample();
    float generateNoise();
    float polyBLEP(double t, double dt);
};

} // namespace axelf::ppgwave
