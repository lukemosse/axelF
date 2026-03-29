#include "MoogContour.h"
#include <cmath>
#include <algorithm>

namespace axelf::moog15
{

static constexpr float kAntiDenormal = 1e-20f;

void MoogContour::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
}

void MoogContour::setParameters(float attackTime, float decayTime, float sustain)
{
    // Attack: linear ramp from current level to 1.0
    // Rate = samples needed to traverse full 0→1 range
    float attackSamples = std::max(attackTime, 0.001f) * static_cast<float>(currentSampleRate);
    attackRate = 1.0f / attackSamples;

    // Decay/Release: exponential toward target
    // Time constant so that ~99.3% of the transition completes in decayTime seconds
    float decaySamples = std::max(decayTime, 0.001f) * static_cast<float>(currentSampleRate);
    decayCoeff = 1.0f - std::exp(-5.0f / decaySamples);

    sustainLevel = sustain;
}

void MoogContour::noteOn()
{
    // Retrigger from current level — never zero the output
    state = Attack;
}

void MoogContour::noteOff()
{
    state = Release;
}

float MoogContour::getNextSample()
{
    switch (state)
    {
        case Idle:
            return 0.0f;

        case Attack:
            output += attackRate;
            if (output >= 1.0f)
            {
                output = 1.0f;
                state = Decay;
            }
            break;

        case Decay:
            output += (sustainLevel - output) * decayCoeff;
            if (std::abs(output - sustainLevel) < 0.0001f)
            {
                output = sustainLevel;
                state = Sustain;
            }
            break;

        case Sustain:
            output = sustainLevel;
            break;

        case Release:
            output *= (1.0f - decayCoeff);
            output += kAntiDenormal;  // prevent denormal stall
            output -= kAntiDenormal;
            if (output < 0.0001f)
            {
                output = 0.0f;
                state = Idle;
            }
            break;
    }
    return output;
}

} // namespace axelf::moog15
