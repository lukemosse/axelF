#include "LinnDrumVoice.h"
#include <cmath>
#include <algorithm>

namespace axelf::linndrum
{

static inline float softClip(float x)
{
    if (x > 1.5f)  return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - (x * x * x) / 6.75f;
}

class LinnDrumSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

bool LinnDrumVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<LinnDrumSound*>(sound) != nullptr;
}

void LinnDrumVoice::startNote(int midiNoteNumber, float velocity,
                               juce::SynthesiserSound*, int /*pitchWheel*/)
{
    // Look up sample from bank based on MIDI note
    if (sampleBank != nullptr)
    {
        int idx = sampleBank->noteToIndex(midiNoteNumber);
        if (idx >= 0 && !sampleBank->samples[static_cast<size_t>(idx)].data.empty())
        {
            const auto& s = sampleBank->samples[static_cast<size_t>(idx)];
            sampleData = s.data.data();
            sampleLength = static_cast<int>(s.data.size());
            originalRate = s.sampleRate;

            // Apply per-drum params from the bank
            const auto& dp = sampleBank->drumParams[static_cast<size_t>(idx)];
            currentLevel = dp.level;
            currentDecay = dp.decay;
            currentPan   = dp.pan;
            tuneSemitones = sampleBank->masterTune + dp.tune;
        }
        else
        {
            return; // Unknown note, don't trigger
        }
    }

    if (sampleData == nullptr)
        return;

    currentVelocity = velocity;
    playbackPosition = 0.0;
    playbackIncrement = (originalRate / getSampleRate()) *
                        std::pow(2.0f, tuneSemitones / 12.0f);
    playing = true;
}

void LinnDrumVoice::stopNote(float /*velocity*/, bool /*allowTailOff*/)
{
    playing = false;
    clearCurrentNote();
}

void LinnDrumVoice::pitchWheelMoved(int /*newPitchWheelValue*/) {}
void LinnDrumVoice::controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/) {}

void LinnDrumVoice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                     int startSample, int numSamples)
{
    if (!playing || sampleData == nullptr || sampleLength < 2)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        int idx = static_cast<int>(playbackPosition);
        if (idx >= sampleLength - 1)
        {
            playing = false;
            clearCurrentNote();
            break;
        }

        // Linear interpolation
        float frac = static_cast<float>(playbackPosition - static_cast<double>(idx));
        float s0 = sampleData[idx];
        float s1 = sampleData[idx + 1];
        float value = s0 + frac * (s1 - s0);

        // Apply level, velocity, and simple decay envelope
        float progress = static_cast<float>(idx) / static_cast<float>(sampleLength);
        float decayEnv = 1.0f - progress * (1.0f - currentDecay);
        value *= currentLevel * currentVelocity * decayEnv;
        value = softClip(value);

        // Pan (equal-power)
        float theta = (currentPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        float gainL = std::cos(theta);
        float gainR = std::sin(theta);

        int pos = startSample + sample;
        buffer.addSample(0, pos, value * gainL);
        buffer.addSample(1, pos, value * gainR);

        playbackPosition += playbackIncrement;
    }
}

void LinnDrumVoice::setTune(float semitones)
{
    tuneSemitones = semitones;
    if (playing)
        playbackIncrement = (originalRate / getSampleRate()) *
                            std::pow(2.0f, tuneSemitones / 12.0f);
}

void LinnDrumVoice::setDecay(float decay)
{
    currentDecay = decay;
}

void LinnDrumVoice::setLevel(float level)
{
    currentLevel = level;
}

void LinnDrumVoice::setPan(float pan)
{
    currentPan = pan;
}

} // namespace axelf::linndrum
