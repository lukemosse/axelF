#include "Moog15VCO.h"
#include <cmath>
#include <algorithm>

namespace axelf::moog15
{

void Moog15VCO::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    phaseIncrement = static_cast<double>(frequency) / currentSampleRate;
}

void Moog15VCO::setFrequency(float hz)
{
    frequency = hz;
    phaseIncrement = static_cast<double>(hz) / currentSampleRate;
}

void Moog15VCO::setWaveform(int waveformIndex)
{
    waveform = std::clamp(waveformIndex, 0, 5);
}

void Moog15VCO::setPulseWidth(float pw)
{
    pulseWidth = std::clamp(pw, 0.05f, 0.95f);
}

float Moog15VCO::getNextSample()
{
    float out = 0.0f;
    const double dt = phaseIncrement;

    switch (waveform)
    {
        case Triangle:
        {
            // Band-limited triangle via integrated square + polyBLAMP
            float raw = static_cast<float>(2.0 * std::abs(2.0 * phase - 1.0) - 1.0);
            // PolyBLAMP correction at the two slope discontinuities (phase=0 and phase=0.5)
            raw += polyBlampResidual(phase);
            raw += polyBlampResidual(std::fmod(phase + 0.5, 1.0));
            out = raw;
            break;
        }
        case SharkTooth:
        {
            // Asymmetric triangle: fast rise (0→0.3), slow fall (0.3→1.0)
            // Signature Minimoog waveform
            constexpr double kRise = 0.3;
            double p = phase;
            if (p < kRise)
                out = static_cast<float>(p / kRise);           // 0→1
            else
                out = static_cast<float>(1.0 - (p - kRise) / (1.0 - kRise));  // 1→0
            out = out * 2.0f - 1.0f;  // scale to ±1
            // PolyBLAMP at slope discontinuities
            out += polyBlampResidual(phase);
            out += polyBlampResidual(std::fmod(phase + (1.0 - kRise), 1.0));
            break;
        }
        case Sawtooth:
        {
            // Falling saw: 1→-1, polyBLEP at reset
            out = static_cast<float>(1.0 - 2.0 * phase);
            out += polyBlepResidual(phase);
            break;
        }
        case ReverseSaw:
        {
            // Rising saw: -1→1, polyBLEP at reset
            out = static_cast<float>(2.0 * phase - 1.0);
            out -= polyBlepResidual(phase);
            break;
        }
        case Square:
        {
            // 50% duty cycle
            out = phase < 0.5 ? 1.0f : -1.0f;
            out += polyBlepResidual(phase);
            out -= polyBlepResidual(std::fmod(phase + 0.5, 1.0));
            break;
        }
        case Pulse:
        {
            // Variable pulse width
            double pw = static_cast<double>(pulseWidth);
            out = phase < pw ? 1.0f : -1.0f;
            out += polyBlepResidual(phase);
            out -= polyBlepResidual(std::fmod(phase + (1.0 - pw), 1.0));
            break;
        }
    }

    // Advance phase (free-running — never reset)
    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    lastOutput = out;
    return out;
}

float Moog15VCO::polyBlepResidual(double t) const
{
    double dt = phaseIncrement;
    if (dt <= 0.0) return 0.0f;

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

float Moog15VCO::polyBlampResidual(double t) const
{
    double dt = phaseIncrement;
    if (dt <= 0.0) return 0.0f;

    // PolyBLAMP: correction for slope discontinuities (triangle/shark)
    // 3rd-order polynomial residual
    if (t < dt)
    {
        t /= dt;
        // -t^3/3 + t^2 - t + 1/3, scaled by dt for integration
        return static_cast<float>(dt * (-t * t * t / 3.0 + t * t - t + 1.0 / 3.0));
    }
    else if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        // t^3/3 + t^2 + t + 1/3, scaled by dt
        return static_cast<float>(dt * (t * t * t / 3.0 + t * t + t + 1.0 / 3.0));
    }
    return 0.0f;
}

} // namespace axelf::moog15
