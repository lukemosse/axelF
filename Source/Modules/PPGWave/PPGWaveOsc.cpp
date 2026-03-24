#include "PPGWaveOsc.h"
#include <algorithm>

namespace axelf::ppgwave
{

// Static member definitions
std::array<Wavetable, kNumTables> PPGWaveOsc::factoryTables{};
std::array<WavetableMipmap, kNumTables> PPGWaveOsc::factoryMipmaps{};
bool PPGWaveOsc::tablesInitialised = false;

void PPGWaveOsc::initFactoryTables()
{
    if (tablesInitialised) return;

    WavetableFactory::generateAll(factoryTables);

    for (int t = 0; t < kNumTables; ++t)
        WavetableFactory::generateMipmap(factoryTables[static_cast<size_t>(t)],
                                          factoryMipmaps[static_cast<size_t>(t)]);

    tablesInitialised = true;
}

void PPGWaveOsc::setFrequency(float freqHz)
{
    frequency = freqHz;
    phaseIncrement = static_cast<double>(freqHz) / currentSampleRate;
}

void PPGWaveOsc::setWavetable(int tableIndex)
{
    currentTable = std::clamp(tableIndex, 0, kNumTables - 1);
}

void PPGWaveOsc::setWavePosition(float position)
{
    wavePosition = std::clamp(position, 0.0f, 63.0f);
}

void PPGWaveOsc::reset()
{
    phase = 0.0;
}

float PPGWaveOsc::processSample()
{
    int mipLevel = getMipLevel(frequency);

    // Crossfade between adjacent wave positions
    int posLow = static_cast<int>(wavePosition);
    int posHigh = std::min(posLow + 1, 63);
    float frac = wavePosition - static_cast<float>(posLow);

    float ph = static_cast<float>(phase);
    float sampleLow  = readWaveform(currentTable, posLow,  ph, mipLevel);
    float sampleHigh = readWaveform(currentTable, posHigh, ph, mipLevel);

    float output = sampleLow + frac * (sampleHigh - sampleLow);

    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0)
        phase -= 1.0;

    return output;
}

int PPGWaveOsc::getMipLevel(float freq) const
{
    // Select mipmap level: higher frequencies use versions with fewer harmonics
    float nyquist = static_cast<float>(currentSampleRate) * 0.5f;
    float maxHarmonics = nyquist / std::max(freq, 20.0f);
    int level = 0;
    int availHarmonics = kSamplesPerWave / 2;
    while (level < kNumMipLevels - 1 && availHarmonics > static_cast<int>(maxHarmonics * 1.5f))
    {
        ++level;
        availHarmonics /= 2;
    }
    return level;
}

float PPGWaveOsc::readWaveform(int tableIdx, int waveIdx, float ph, int mipLevel) const
{
    const auto& wf = factoryMipmaps[static_cast<size_t>(tableIdx)]
                                    [static_cast<size_t>(mipLevel)]
                                    [static_cast<size_t>(waveIdx)];

    // Linear interpolation within the waveform
    float pos = ph * static_cast<float>(kSamplesPerWave);
    int idx0 = static_cast<int>(pos) & (kSamplesPerWave - 1);
    int idx1 = (idx0 + 1) & (kSamplesPerWave - 1);
    float f = pos - std::floor(pos);

    return wf[static_cast<size_t>(idx0)] + f * (wf[static_cast<size_t>(idx1)] - wf[static_cast<size_t>(idx0)]);
}

} // namespace axelf::ppgwave
