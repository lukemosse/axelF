#include "Jupiter8DCO.h"
#include <cmath>
#include <algorithm>

namespace axelf::jupiter8
{

void Jupiter8DCO::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    updatePhaseIncrement();
}

void Jupiter8DCO::setFrequency(float hz)
{
    frequency = hz;
    updatePhaseIncrement();
}

void Jupiter8DCO::setWaveform(int waveformIndex)
{
    waveform = waveformIndex;
}

void Jupiter8DCO::setRange(float r)
{
    range = r;
    updatePhaseIncrement();
}

void Jupiter8DCO::setPulseWidth(float pw)
{
    pulseWidth = std::clamp(pw, 0.05f, 0.95f);
}

void Jupiter8DCO::resetPhase()
{
    phase = 0.0;
}

void Jupiter8DCO::reset()
{
    phase = 0.0;
    phaseWrapped = false;
}

float Jupiter8DCO::getNextSample()
{
    float out = 0.0f;
    phaseWrapped = false;

    switch (waveform)
    {
        case 0:  // Sawtooth
        {
            out = static_cast<float>(2.0 * phase - 1.0);
            out -= polyBlepResidual(phase);
            break;
        }
        case 1:  // Pulse (variable width)
        {
            double pw = static_cast<double>(pulseWidth);
            out = phase < pw ? 1.0f : -1.0f;
            out += polyBlepResidual(phase);
            out -= polyBlepResidual(std::fmod(phase + (1.0 - pw), 1.0));
            break;
        }
        case 2:  // Sub-oscillator (square one octave down)
        {
            double subPhase = std::fmod(phase * 0.5, 1.0);
            out = subPhase < 0.5 ? 1.0f : -1.0f;
            break;
        }
    }

    phase += phaseIncrement;
    if (phase >= 1.0)
    {
        phase -= 1.0;
        phaseWrapped = true;
    }

    return out;
}

void Jupiter8DCO::updatePhaseIncrement()
{
    // Range maps feet: 16'=0.5x, 8'=1x, 4'=2x, 2'=4x
    float rangeMultiplier = 8.0f / range;
    phaseIncrement = static_cast<double>(frequency * rangeMultiplier) / currentSampleRate;
}

float Jupiter8DCO::polyBlepResidual(double t) const
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

} // namespace axelf::jupiter8
