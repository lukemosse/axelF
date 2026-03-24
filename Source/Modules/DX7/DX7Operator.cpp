#include "DX7Operator.h"
#include <cmath>
#include <algorithm>

namespace axelf::dx7
{

void DX7Operator::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    updatePhaseIncrement();
}

void DX7Operator::setFrequency(float hz)
{
    baseFrequency = hz;
    updatePhaseIncrement();
}

void DX7Operator::setRatio(float r)
{
    ratio = r;
    updatePhaseIncrement();
}

void DX7Operator::setLevel(float level)
{
    // Convert DX7 0-99 to linear amplitude
    outputLevel = std::pow(10.0f, (level - 99.0f) * 0.025f);
}

void DX7Operator::setDetune(float d)
{
    detune = d;
    updatePhaseIncrement();
}

void DX7Operator::reset()
{
    phase = 0.0;
    egState = EGState::Idle;
    egOutput = 0.0f;
}

float DX7Operator::processSample(float phaseModInput)
{
    float egVal = advanceEG();

    // FM synthesis: output = sin(2π * phase + modulation)
    float out = std::sin(static_cast<float>(phase * 2.0 * 3.14159265358979323846) + phaseModInput);
    out *= egVal * outputLevel * velocityScale;

    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    return out;
}

void DX7Operator::noteOn()
{
    egState = EGState::Rate1;
    // Don't reset phase for DX7 (free-running operators)
}

void DX7Operator::noteOff()
{
    egState = EGState::Rate4;
}

bool DX7Operator::isActive() const
{
    return egState != EGState::Idle;
}

void DX7Operator::setEGParameters(float r1, float r2, float r3, float r4,
                                   float l1, float l2, float l3, float l4)
{
    egRates[0] = r1; egRates[1] = r2; egRates[2] = r3; egRates[3] = r4;
    egLevels[0] = l1; egLevels[1] = l2; egLevels[2] = l3; egLevels[3] = l4;
}

void DX7Operator::updatePhaseIncrement()
{
    float detuneHz = detune * 0.01f * baseFrequency;
    phaseIncrement = static_cast<double>((baseFrequency * ratio + detuneHz))
                     / currentSampleRate;
}

float DX7Operator::advanceEG()
{
    // Simplified DX7 envelope generator
    float targetLevel = 0.0f;
    float rateValue = 50.0f;

    switch (egState)
    {
        case EGState::Idle:
            return 0.0f;
        case EGState::Rate1:
            targetLevel = egLevels[0] / 99.0f;
            rateValue = egRates[0];
            break;
        case EGState::Rate2:
            targetLevel = egLevels[1] / 99.0f;
            rateValue = egRates[1];
            break;
        case EGState::Rate3:
            targetLevel = egLevels[2] / 99.0f;
            rateValue = egRates[2];
            break;
        case EGState::Rate4:
            targetLevel = egLevels[3] / 99.0f;
            rateValue = egRates[3];
            break;
    }

    // DX7 rate 0-99 → linear segment time (logarithmic mapping)
    // Rate 99 → ~2ms, Rate 50 → ~270ms, Rate 0 → ~40s
    float timeSeconds = 40.0f * std::exp(-0.1f * rateValue);
    float step = 1.0f / (timeSeconds * static_cast<float>(currentSampleRate));
    step = std::max(step, 1e-8f);

    float diff = targetLevel - egOutput;
    if (std::abs(diff) <= step)
    {
        egOutput = targetLevel;
        switch (egState)
        {
            case EGState::Rate1: egState = EGState::Rate2; break;
            case EGState::Rate2: egState = EGState::Rate3; break;
            case EGState::Rate3: break;  // sustain: stay in Rate3
            case EGState::Rate4:
                if (targetLevel < 0.001f)
                    egState = EGState::Idle;
                break;
            default: break;
        }
    }
    else
    {
        egOutput += (diff > 0.0f ? step : -step);
    }

    return std::clamp(egOutput, 0.0f, 1.0f);
}

void DX7Operator::setVelocitySensitivity(float vs)
{
    velocitySensitivity = std::clamp(vs, 0.0f, 7.0f);
}

void DX7Operator::setVelocity(float vel)
{
    // sens 0 → no velocity effect (scale=1), sens 7 → full velocity (scale=vel)
    float normSens = velocitySensitivity / 7.0f;
    velocityScale = 1.0f - normSens * (1.0f - vel);
}

} // namespace axelf::dx7
