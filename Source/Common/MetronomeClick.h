#pragma once

#include <cmath>
#include <atomic>

namespace axelf
{

// Simple metronome click synthesizer.
// Generates short sine bursts — high pitch for downbeat, lower for other beats.
class MetronomeClick
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        samplesRemaining = 0;
        phase = 0.0;
    }

    // Trigger a click at sample-precise position.
    // isDownbeat: true for beat 1 of bar (higher pitch).
    void trigger(bool isDownbeat)
    {
        if (!enabled.load()) return;
        freq = isDownbeat ? 1500.0 : 1000.0;
        amplitude = isDownbeat ? 0.7 : 0.5;
        samplesRemaining = static_cast<int>(sr * kClickDuration);
        totalSamples = samplesRemaining;
        phase = 0.0;
    }

    // Render into output buffer (adds to existing audio). Returns samples written.
    void renderBlock(float* left, float* right, int numSamples)
    {
        if (!enabled.load() || samplesRemaining <= 0)
            return;

        const int toRender = std::min(samplesRemaining, numSamples);
        const double twoPi = 2.0 * 3.14159265358979323846;
        const double phaseInc = freq / sr;

        for (int i = 0; i < toRender; ++i)
        {
            float env = static_cast<float>(samplesRemaining - i) / static_cast<float>(totalSamples);
            env = env * env;  // quadratic decay
            float sample = static_cast<float>(std::sin(phase * twoPi)) * static_cast<float>(amplitude) * env;
            left[i]  += sample;
            right[i] += sample;
            phase += phaseInc;
        }
        samplesRemaining -= toRender;
    }

    void setEnabled(bool e) { enabled.store(e); }
    bool isEnabled() const { return enabled.load(); }

private:
    static constexpr double kClickDuration = 0.015; // 15ms click
    double sr = 44100.0;
    double freq = 1000.0;
    double amplitude = 0.5;
    double phase = 0.0;
    int samplesRemaining = 0;
    int totalSamples = 0;
    std::atomic<bool> enabled { true };
};

} // namespace axelf
