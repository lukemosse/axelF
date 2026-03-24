#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace axelf::dsp
{

class LadderFilter
{
public:
    void setSampleRate(double sampleRate);
    void setParameters(float cutoffHz, float resonance, float drive);
    void reset();

    float processSample(float input);

private:
    double currentSampleRate = 44100.0;
    float cutoff    = 1000.0f;
    float resonance = 0.0f;
    float drive     = 1.0f;

    // 4-pole state (4 cascaded one-pole sections)
    float stage[4] = {};
    float stageZ1[4] = {};
    float feedback = 0.0f;
};

} // namespace axelf::dsp
