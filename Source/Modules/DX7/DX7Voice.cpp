#include "DX7Voice.h"
#include <cmath>
#include <algorithm>

namespace axelf::dx7
{

static inline float softClip(float x)
{
    return std::tanh(x);
}

bool DX7Voice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<DX7Sound*>(sound) != nullptr;
}

void DX7Voice::startNote(int midiNoteNumber, float vel,
                          juce::SynthesiserSound*, int pitchWheel)
{
    velocity = vel;
    noteFrequency = static_cast<float>(
        juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
    pitchBendSemitones = ((static_cast<float>(pitchWheel) - 8192.0f) / 8192.0f) * 2.0f;

    float bendMul = std::pow(2.0f, pitchBendSemitones / 12.0f);
    float freq = noteFrequency * bendMul;

    lfo.setSampleRate(getSampleRate());
    samplesSinceNoteOn = 0;

    for (auto& op : operators)
    {
        op.setSampleRate(getSampleRate());
        op.setFrequency(freq);
        op.setVelocity(velocity);
        op.noteOn();
    }
}

void DX7Voice::stopNote(float /*vel*/, bool allowTailOff)
{
    for (auto& op : operators)
        op.noteOff();

    if (!allowTailOff)
    {
        for (auto& op : operators)
            op.reset();
        clearCurrentNote();
    }
}

void DX7Voice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchBendSemitones = ((static_cast<float>(newPitchWheelValue) - 8192.0f) / 8192.0f) * 2.0f;
    float bendMul = std::pow(2.0f, pitchBendSemitones / 12.0f);
    float freq = noteFrequency * bendMul;
    for (auto& op : operators)
        op.setFrequency(freq);
}
void DX7Voice::controllerMoved(int controllerNumber, int newControllerValue)
{
    if (controllerNumber == 1)  // mod wheel
        modWheelValue = static_cast<float>(newControllerValue) / 127.0f;
}

void DX7Voice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                int startSample, int numSamples)
{
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Check if any operator is still active
        bool anyActive = false;
        for (const auto& op : operators)
        {
            if (op.isActive())
            {
                anyActive = true;
                break;
            }
        }

        if (!anyActive)
        {
            clearCurrentNote();
            break;
        }

        const auto& topo = algorithm.getTopology();

        // LFO modulation with delay fade-in
        float lfoVal = lfo.getNextSample();
        float lfoFade = (lfoDelaySamples > 0)
            ? std::min(1.0f, static_cast<float>(samplesSinceNoteOn) / static_cast<float>(lfoDelaySamples))
            : 1.0f;
        float totalLfoPMD = (lfoPMD + modWheelValue * (1.0f - lfoPMD)) * lfoFade;
        float pitchMod = 1.0f + totalLfoPMD * lfoVal * 0.02f;
        float ampMod = 1.0f - (lfoAMD * lfoFade) * (1.0f - lfoVal) * 0.5f;
        ++samplesSinceNoteOn;

        // Update operator frequencies with pitch bend + LFO
        float bendMul = std::pow(2.0f, pitchBendSemitones / 12.0f);
        float freq = noteFrequency * bendMul * pitchMod;
        for (auto& op : operators)
            op.setFrequency(freq);

        // FM modulation depth: 2π max index gives proper FM character
        constexpr float kFMModScale = 6.2831853f;  // 2π

        // Process operators in reverse order (6→1) so modulators run before carriers
        std::array<float, 6> opOut = {};

        for (int i = 5; i >= 0; --i)
        {
            // Sum all modulation inputs for this operator
            float modInput = 0.0f;
            uint8_t mods = topo.modulators[static_cast<size_t>(i)];
            for (int j = 0; j < 6; ++j)
            {
                if (mods & (1 << j))
                    modInput += opOut[static_cast<size_t>(j)] * kFMModScale;
            }

            // Apply feedback (two-sample average for stability, matches DX7 OPS chip)
            if (i == topo.feedbackOp)
            {
                float fbAvg = (lastFeedbackSample + prevFeedbackSample) * 0.5f;
                modInput += fbAvg * (static_cast<float>(feedbackAmount) / 7.0f) * kFMModScale;
            }

            opOut[static_cast<size_t>(i)] = operators[static_cast<size_t>(i)].processSample(modInput);
        }

        // Store feedback history
        prevFeedbackSample = lastFeedbackSample;
        lastFeedbackSample = opOut[static_cast<size_t>(topo.feedbackOp)];

        // Sum carrier outputs (scaled by carrier count to prevent stacking)
        float output = 0.0f;
        int carrierCount = 0;
        for (int i = 0; i < 6; ++i)
        {
            if (topo.carrierMask & (1 << i))
            {
                output += opOut[static_cast<size_t>(i)];
                ++carrierCount;
            }
        }
        if (carrierCount > 1)
            output /= std::sqrt(static_cast<float>(carrierCount));

        output = softClip(output * ampMod);

        int pos = startSample + sample;
        buffer.addSample(0, pos, output);
        buffer.addSample(1, pos, output);
    }
}

void DX7Voice::setLFOParameters(float rate, float pmd, float amd, int waveform, float delay)
{
    // DX7 LFO speed 0-99 maps to ~0.1Hz - 50Hz
    float freq = 0.1f + (rate / 99.0f) * 49.9f;
    lfo.setFrequency(freq);
    lfoPMD = pmd / 99.0f;
    lfoAMD = amd / 99.0f;
    lfo.setWaveform(static_cast<axelf::dsp::LFOWaveform>(
        std::clamp(waveform, 0, 4)));

    // LFO delay 0-99 maps to 0-6 seconds fade-in
    lfoDelaySamples = static_cast<int>((delay / 99.0f) * 6.0f * static_cast<float>(getSampleRate()));
}

} // namespace axelf::dx7
