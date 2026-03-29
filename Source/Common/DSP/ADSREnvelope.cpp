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
    const float minTime = 0.001f;
    const float sr = static_cast<float>(currentSampleRate);

    // Attack stays linear (ramp from 0→1 in attackTime seconds)
    attackRate = 1.0f / (std::max(attack, minTime) * sr);

    // Decay & release use true exponential coefficients.
    // Multiplier per sample: exp(-6.908 / (time * sampleRate))
    // gives ~60 dB attenuation (factor 0.001) in the specified time.
    decayRate    = std::exp(-6.908f / (std::max(decay, minTime) * sr));
    sustainLevel = std::clamp(sustain, 0.0f, 1.0f);
    releaseRate  = std::exp(-6.908f / (std::max(release, minTime) * sr));
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
            // True exponential decay toward sustainLevel
            output = sustainLevel + (output - sustainLevel) * decayRate;
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
            // True exponential release toward zero
            output *= releaseRate;
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
