#pragma once

namespace axelf::moog15
{

/// Minimoog Model D VCO — free-running, 6 waveforms, polyBLEP/BLAMP anti-aliased.
/// Phase accumulator is never reset; oscillator runs continuously even between notes.
class Moog15VCO
{
public:
    /// Waveform indices matching the spec
    enum Waveform { Triangle = 0, SharkTooth, Sawtooth, ReverseSaw, Square, Pulse };

    void setSampleRate(double sampleRate);
    void setFrequency(float hz);
    void setWaveform(int waveformIndex);
    void setPulseWidth(float pw);

    /// Free-running: no reset(). Phase accumulates from construction forever.
    float getNextSample();

    /// Get raw output for Osc 3 LFO mode (unscaled)
    float getCurrentOutput() const { return lastOutput; }

private:
    double currentSampleRate = 44100.0;
    int waveform = Sawtooth;
    float frequency = 440.0f;
    float pulseWidth = 0.5f;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float lastOutput = 0.0f;

    // PolyBLEP residual for step discontinuities (saw, square, pulse)
    float polyBlepResidual(double t) const;
    // PolyBLAMP residual for slope discontinuities (triangle, shark tooth)
    float polyBlampResidual(double t) const;
};

} // namespace axelf::moog15
