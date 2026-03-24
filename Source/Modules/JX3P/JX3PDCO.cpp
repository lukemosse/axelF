#include "JX3PDCO.h"
#include <cmath>
#include <algorithm>

namespace axelf::jx3p
{

void JX3PDCO::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    updatePhaseIncrement();
}

void JX3PDCO::setFrequency(float hz)
{
    frequency = hz;
    updatePhaseIncrement();
}

void JX3PDCO::setWaveform(int waveformIndex)
{
    waveform = waveformIndex;
}

void JX3PDCO::setRange(float r)
{
    range = r;
    updatePhaseIncrement();
}

void JX3PDCO::setPulseWidth(float pw)
{
    pulseWidth = std::clamp(pw, 0.05f, 0.95f);
}

void JX3PDCO::resetPhase()
{
    phase = 0.0;
}

bool JX3PDCO::didPhaseWrap() const
{
    return phaseWrapped;
}

void JX3PDCO::reset()
{
    phase = 0.0;
    phaseWrapped = false;
}

float JX3PDCO::getNextSample()
{
    phaseWrapped = false;
    float out = 0.0f;

    switch (waveform)
    {
        case 0:  // Sawtooth
            out = static_cast<float>(2.0 * phase - 1.0);
            out -= polyBlepResidual(phase);
            break;
        case 1:  // Pulse (variable width)
        {
            double pw = static_cast<double>(pulseWidth);
            out = phase < pw ? 1.0f : -1.0f;
            out += polyBlepResidual(phase);
            out -= polyBlepResidual(std::fmod(phase + (1.0 - pw), 1.0));
            break;
        }
        case 2:  // Square
            out = phase < 0.5 ? 1.0f : -1.0f;
            out += polyBlepResidual(phase);
            out -= polyBlepResidual(std::fmod(phase + 0.5, 1.0));
            break;
    }

    phase += phaseIncrement;
    if (phase >= 1.0)
    {
        phase -= 1.0;
        phaseWrapped = true;
    }

    return out;
}

void JX3PDCO::updatePhaseIncrement()
{
    float rangeMultiplier = 8.0f / range;
    phaseIncrement = static_cast<double>(frequency * rangeMultiplier) / currentSampleRate;
}

float JX3PDCO::polyBlepResidual(double t) const
{
    double dt = phaseIncrement;
    if (t < dt)
    {
        t /= dt;
        return static_cast<float>(t + t - t * t - 1.0);
    }
    else if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return static_cast<float>(t * t + t + t + 1.0);
    }
    return 0.0f;
}

} // namespace axelf::jx3p
