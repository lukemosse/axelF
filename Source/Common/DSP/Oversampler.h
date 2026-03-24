#pragma once

#include <juce_dsp/juce_dsp.h>

namespace axelf::dsp
{

class Oversampler
{
public:
    explicit Oversampler(int factor = 2);

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();

    juce::dsp::AudioBlock<float> getOversampledBlock(juce::dsp::AudioBlock<float>& inputBlock);
    void downsampleBack(juce::dsp::AudioBlock<float>& outputBlock);

    int getFactor() const { return oversamplingFactor; }

private:
    int oversamplingFactor;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
};

} // namespace axelf::dsp
