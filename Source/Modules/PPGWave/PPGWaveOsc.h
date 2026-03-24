#pragma once

#include "WavetableData.h"
#include <cmath>

namespace axelf::ppgwave
{

class PPGWaveOsc
{
public:
    void setSampleRate(double sampleRate) { currentSampleRate = sampleRate; }
    void setFrequency(float freqHz);
    void setWavetable(int tableIndex);
    void setWavePosition(float position);  // 0.0 – 63.0
    float getCurrentPosition() const { return wavePosition; }
    void setKeytrack(bool enabled) { keytrack = enabled; }
    void reset();

    float processSample();

    // Call once at startup to build all factory tables + mipmaps
    static void initFactoryTables();

    // Shared factory data (built once, read by all voices)
    static std::array<Wavetable, kNumTables> factoryTables;
    static std::array<WavetableMipmap, kNumTables> factoryMipmaps;
    static bool tablesInitialised;

private:
    double currentSampleRate = 44100.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float frequency = 440.0f;
    int currentTable = 0;
    float wavePosition = 0.0f;
    bool keytrack = true;

    int getMipLevel(float freq) const;
    float readWaveform(int tableIdx, int waveIdx, float ph, int mipLevel) const;
};

} // namespace axelf::ppgwave
