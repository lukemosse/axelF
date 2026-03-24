#include "DX7Processor.h"
#include "DX7Voice.h"
#include "DX7Editor.h"

namespace axelf::dx7
{

DX7Processor::DX7Processor()
    : apvts(dummyProcessor, nullptr, "DX7", createParameterLayout())
{
    // 16-voice polyphonic
    for (int i = 0; i < 16; ++i)
        synth.addVoice(new DX7Voice());

    synth.addSound(new DX7Sound());
}

void DX7Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    oversampler.initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampledRate = sampleRate * static_cast<double>(oversampler.getOversamplingFactor());
    synth.setCurrentPlaybackSampleRate(oversampledRate);

    // DC blocker coefficient (~5 Hz HPF)
    dcR = 1.0f - (2.0f * juce::MathConstants<float>::pi * 5.0f / static_cast<float>(sampleRate));
    dcX[0] = dcX[1] = 0.0f;
    dcY[0] = dcY[1] = 0.0f;
}

void DX7Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                 juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    updateVoiceParameters();

    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampler.processSamplesUp(block);

    // Wrap oversampled block as AudioBuffer for juce::Synthesiser
    float* channels[] = { oversampledBlock.getChannelPointer(0),
                          oversampledBlock.getChannelPointer(1) };
    juce::AudioBuffer<float> osBuffer(channels, 2,
                                       static_cast<int>(oversampledBlock.getNumSamples()));
    synth.renderNextBlock(osBuffer, midiMessages, 0, static_cast<int>(oversampledBlock.getNumSamples()));

    oversampler.processSamplesDown(block);

    // DC blocker (single-pole HPF at ~5 Hz)
    for (int ch = 0; ch < buffer.getNumChannels() && ch < 2; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = data[i];
            dcY[ch] = x - dcX[ch] + dcR * dcY[ch];
            dcX[ch] = x;
            data[i] = dcY[ch];
        }
    }
}

void DX7Processor::releaseResources() {}

juce::Component* DX7Processor::createEditor()
{
    return nullptr;  // Editors now created in PluginEditor with PatternEngine refs
}

void DX7Processor::updateVoiceParameters()
{
    auto safeLoad = [&](const juce::String& id, float fallback = 0.0f) -> float
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return p->load();
        return fallback;
    };

    int alg = static_cast<int>(safeLoad("dx7_algorithm", 1.0f));
    int fb  = static_cast<int>(safeLoad("dx7_feedback", 0.0f));

    for (int v = 0; v < synth.getNumVoices(); ++v)
    {
        if (auto* voice = dynamic_cast<DX7Voice*>(synth.getVoice(v)))
        {
            voice->setAlgorithm(alg);
            voice->setFeedback(fb);

            // LFO
            voice->setLFOParameters(
                safeLoad("dx7_lfo_speed", 35.0f),
                safeLoad("dx7_lfo_pmd", 0.0f),
                safeLoad("dx7_lfo_amd", 0.0f),
                static_cast<int>(safeLoad("dx7_lfo_wave", 0.0f)));

            for (int op = 0; op < 6; ++op)
            {
                auto prefix = "dx7_op" + juce::String(op + 1) + "_";
                auto& oper = voice->getOperator(op);

                oper.setLevel(safeLoad(prefix + "level", 99.0f));
                oper.setRatio(safeLoad(prefix + "ratio", 1.0f));
                oper.setDetune(safeLoad(prefix + "detune", 0.0f));

                oper.setEGParameters(
                    safeLoad(prefix + "eg_r1", 50.0f),
                    safeLoad(prefix + "eg_r2", 50.0f),
                    safeLoad(prefix + "eg_r3", 50.0f),
                    safeLoad(prefix + "eg_r4", 50.0f),
                    safeLoad(prefix + "eg_l1", 99.0f),
                    safeLoad(prefix + "eg_l2", 99.0f),
                    safeLoad(prefix + "eg_l3", 99.0f),
                    safeLoad(prefix + "eg_l4", 0.0f)
                );
            }
        }
    }
}

} // namespace axelf::dx7

