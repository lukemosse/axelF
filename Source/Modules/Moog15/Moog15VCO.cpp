#include "Moog15VCO.h"
#include <cmath>
#include <algorithm>

namespace axelf::moog15
{

void Moog15VCO::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    updatePhaseIncrement();
}

void Moog15VCO::setFrequency(float hz)
{
    frequency = hz;
    updatePhaseIncrement();
}

void Moog15VCO::setWaveform(int waveformIndex)
{
    waveform = waveformIndex;
}

void Moog15VCO::setRange(float r)
{
    range = r;
    updatePhaseIncrement();
}

void Moog15VCO::setPulseWidth(float pw)
{
    pulseWidth = std::clamp(pw, 0.05f, 0.95f);
}

void Moog15VCO::reset()
{
    phase = 0.0;
}

float Moog15VCO::getNextSample()
{
    float out = 0.0f;

    switch (waveform)
    {
        case 0:  // Sawtooth
        {
            out = static_cast<float>(2.0 * phase - 1.0);
            out -= polyBlepResidual(phase);
            break;
        }
        case 1:  // Triangle (integrated square with PolyBLAMP)
        {
            out = static_cast<float>(2.0 * std::abs(2.0 * phase - 1.0) - 1.0);
            break;
        }
        case 2:  // Pulse (variable width)
        {
            double pw = static_cast<double>(pulseWidth);
            out = phase < pw ? 1.0f : -1.0f;
            out += polyBlepResidual(phase);
            out -= polyBlepResidual(std::fmod(phase + (1.0 - pw), 1.0));
            break;
        }
        case 3:  // Square
        {
            out = phase < 0.5 ? 1.0f : -1.0f;
            out += polyBlepResidual(phase);
            out -= polyBlepResidual(std::fmod(phase + 0.5, 1.0));
            break;
        }
    }

    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    return out;
}

void Moog15VCO::updatePhaseIncrement()
{
    float rangeMultiplier = 16.0f / range;
    phaseIncrement = static_cast<double>(frequency * rangeMultiplier) / currentSampleRate;
}

float Moog15VCO::polyBlepResidual(double t) const
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

} // namespace axelf::moog15
