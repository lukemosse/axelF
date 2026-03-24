#include "ADSREnvelope.h"
#include <algorithm>

namespace axelf::dsp
{

void ADSREnvelope::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
}

void ADSREnvelope::setParameters(float attack, float decay, float sustain, float release)
{
    // Convert times (seconds) to per-sample rates
    const float minTime = 0.001f;
    attackRate   = 1.0f / (std::max(attack, minTime) * static_cast<float>(currentSampleRate));
    decayRate    = 1.0f / (std::max(decay, minTime) * static_cast<float>(currentSampleRate));
    sustainLevel = std::clamp(sustain, 0.0f, 1.0f);
    releaseRate  = 1.0f / (std::max(release, minTime) * static_cast<float>(currentSampleRate));
}

void ADSREnvelope::noteOn()
{
    state = State::Attack;
}

void ADSREnvelope::noteOff()
{
    if (state != State::Idle)
        state = State::Release;
}

void ADSREnvelope::reset()
{
    state = State::Idle;
    output = 0.0f;
}

float ADSREnvelope::getNextSample()
{
    switch (state)
    {
        case State::Idle:
            return 0.0f;

        case State::Attack:
            output += attackRate;
            if (output >= 1.0f)
            {
                output = 1.0f;
                state = State::Decay;
            }
            break;

        case State::Decay:
            output -= decayRate * (output - sustainLevel + 0.0001f);  // exponential decay
            if (output <= sustainLevel + 0.0001f)
            {
                output = sustainLevel;
                state = State::Sustain;
            }
            break;

        case State::Sustain:
            output = sustainLevel;
            break;

        case State::Release:
            output -= releaseRate * (output + 0.0001f);  // exponential release
            if (output <= 0.0001f)
            {
                output = 0.0f;
                state = State::Idle;
            }
            break;
    }

    return output;
}

} // namespace axelf::dsp
