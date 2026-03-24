#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace axelf::linndrum
{

struct DrumVoiceParams
{
    float level = 0.8f;
    float tune  = 0.0f;   // semitones added to master tune
    float decay = 1.0f;
    float pan   = 0.0f;
};

struct DrumSampleBank
{
    struct Sample
    {
        std::vector<float> data;
        double sampleRate = 44100.0;
    };

    std::array<Sample, 15> samples;

    // Per-drum params updated each processBlock by the processor
    std::array<DrumVoiceParams, 15> drumParams;
    float masterTune = 0.0f;

    // GM drum note map: kick=36, snare=38, closedHH=42, openHH=46, ride=51,
    // crash=49, hiTom=50, midTom=47, lowTom=45, hiConga=62, lowConga=63,
    // clap=39, cowbell=56, tambourine=54, cabasa=69
    static constexpr int kNoteMap[15] = {
        36, 38, 42, 46, 51, 49, 50, 47, 45, 62, 63, 39, 56, 54, 69
    };

    int noteToIndex(int midiNote) const
    {
        for (int i = 0; i < 15; ++i)
            if (kNoteMap[i] == midiNote) return i;
        return -1;
    }
};

class LinnDrumVoice : public juce::SynthesiserVoice
{
public:
    void setSampleBank(const DrumSampleBank* bank) { sampleBank = bank; }

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         int startSample, int numSamples) override;

    void setTune(float semitones);
    void setDecay(float decay);
    void setLevel(float level);
    void setPan(float pan);

private:
    const DrumSampleBank* sampleBank = nullptr;
    const float* sampleData = nullptr;
    int sampleLength = 0;
    double originalRate = 44100.0;

    double playbackPosition = 0.0;
    double playbackIncrement = 1.0;
    float tuneSemitones = 0.0f;
    float currentLevel = 0.8f;
    float currentPan = 0.0f;
    float currentDecay = 1.0f;
    float currentVelocity = 0.0f;
    bool playing = false;
};

} // namespace axelf::linndrum
