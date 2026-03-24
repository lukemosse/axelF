#include "Oversampler.h"

namespace axelf::dsp
{

Oversampler::Oversampler(int factor)
    : oversamplingFactor(factor)
{
}

void Oversampler::prepare(double /*sampleRate*/, int samplesPerBlock, int numChannels)
{
    int order = 0;
    int f = oversamplingFactor;
    while (f > 1) { f >>= 1; ++order; }

    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels,
        static_cast<size_t>(order),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);

    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
}

void Oversampler::reset()
{
    if (oversampling)
        oversampling->reset();
}

juce::dsp::AudioBlock<float> Oversampler::getOversampledBlock(juce::dsp::AudioBlock<float>& inputBlock)
{
    return oversampling->processSamplesUp(inputBlock);
}

void Oversampler::downsampleBack(juce::dsp::AudioBlock<float>& outputBlock)
{
    oversampling->processSamplesDown(outputBlock);
}

} // namespace axelf::dsp
