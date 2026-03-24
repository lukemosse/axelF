#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace axelf::ppgwave
{

// 16 wavetables x 64 waveforms x 256 samples each
// Generated procedurally to approximate classic PPG character
static constexpr int kNumTables     = 16;
static constexpr int kWavesPerTable = 64;
static constexpr int kSamplesPerWave = 256;
static constexpr int kNumMipLevels  = 8;  // octave-spaced bandlimited versions

// Single wavetable: 64 waveforms x 256 samples
using Waveform = std::array<float, kSamplesPerWave>;
using Wavetable = std::array<Waveform, kWavesPerTable>;

// Mipmap set for one wavetable: 8 octave levels
using WavetableMipmap = std::array<Wavetable, kNumMipLevels>;

// Factory wavetable generator — called once at startup
class WavetableFactory
{
public:
    static void generateAll(std::array<Wavetable, kNumTables>& tables)
    {
        generateUpperWaves(tables[0]);
        generateResonant(tables[1]);
        generateResonant2(tables[2]);
        generateMalletWaves(tables[3]);
        generateAnharmonic(tables[4]);
        generateEPiano(tables[5]);
        generateBassWaves(tables[6]);
        generateOrganWaves(tables[7]);
        generateVocalWaves(tables[8]);
        generatePadWaves(tables[9]);
        generateDigitalHarsh(tables[10]);
        generateSyncSweep(tables[11]);
        generateClassicAnalog(tables[12]);
        generateAdditive(tables[13]);
        generateNoiseShapes(tables[14]);
        generateClassicAnalog(tables[15]);  // User slot fallback = Classic Analog
    }

    // Generate bandlimited mipmap for a single wavetable
    static void generateMipmap(const Wavetable& source, WavetableMipmap& mipmap)
    {
        mipmap[0] = source;
        for (int level = 1; level < kNumMipLevels; ++level)
        {
            // Each successive level halves the harmonic content
            int maxHarmonic = kSamplesPerWave / (2 << level);
            if (maxHarmonic < 1) maxHarmonic = 1;

            for (int w = 0; w < kWavesPerTable; ++w)
            {
                // Reconstruct from harmonics via additive synthesis with truncation
                auto& src = source[static_cast<size_t>(w)];
                auto& dst = mipmap[static_cast<size_t>(level)][static_cast<size_t>(w)];

                // DFT analysis (simplified — extract harmonics)
                std::array<float, kSamplesPerWave / 2> cosCoeff{};
                std::array<float, kSamplesPerWave / 2> sinCoeff{};
                const float N = static_cast<float>(kSamplesPerWave);

                for (int h = 1; h < kSamplesPerWave / 2 && h <= maxHarmonic; ++h)
                {
                    float cosSum = 0.0f, sinSum = 0.0f;
                    for (int s = 0; s < kSamplesPerWave; ++s)
                    {
                        float angle = 2.0f * 3.14159265f * static_cast<float>(h * s) / N;
                        cosSum += src[static_cast<size_t>(s)] * std::cos(angle);
                        sinSum += src[static_cast<size_t>(s)] * std::sin(angle);
                    }
                    cosCoeff[static_cast<size_t>(h)] = cosSum * 2.0f / N;
                    sinCoeff[static_cast<size_t>(h)] = sinSum * 2.0f / N;
                }

                // Reconstruct
                for (int s = 0; s < kSamplesPerWave; ++s)
                {
                    float val = 0.0f;
                    for (int h = 1; h <= maxHarmonic; ++h)
                    {
                        float angle = 2.0f * 3.14159265f * static_cast<float>(h * s) / N;
                        val += cosCoeff[static_cast<size_t>(h)] * std::cos(angle)
                             + sinCoeff[static_cast<size_t>(h)] * std::sin(angle);
                    }
                    dst[static_cast<size_t>(s)] = val;
                }
            }
        }
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kTwoPi = 2.0f * kPi;

    // Table 0: Upper Waves — saw-to-pulse morphing
    static void generateUpperWaves(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float morph = static_cast<float>(w) / 63.0f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                float saw = 2.0f * phase - 1.0f;
                float pulse = (phase < 0.1f + morph * 0.4f) ? 1.0f : -1.0f;
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = saw * (1.0f - morph) + pulse * morph;
            }
        }
    }

    // Table 1: Resonant — filter-swept peaks
    static void generateResonant(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            int numHarmonics = 2 + (w * 30) / 63;
            float peak = 1.0f + static_cast<float>(w) * 0.3f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float val = 0.0f;
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                for (int h = 1; h <= numHarmonics; ++h)
                {
                    float amp = 1.0f / static_cast<float>(h);
                    float dist = std::abs(static_cast<float>(h) - peak);
                    amp *= std::exp(-dist * 0.5f);
                    val += amp * std::sin(kTwoPi * static_cast<float>(h) * phase);
                }
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 2: Resonant 2 — more extreme
    static void generateResonant2(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float peak = 2.0f + static_cast<float>(w) * 0.6f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float val = 0.0f;
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                for (int h = 1; h <= 40; ++h)
                {
                    float dist = std::abs(static_cast<float>(h) - peak);
                    float amp = std::exp(-dist * dist * 0.1f);
                    val += amp * std::sin(kTwoPi * static_cast<float>(h) * phase);
                }
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 3: Mallet Waves — struck/plucked transient character
    static void generateMalletWaves(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float brightness = static_cast<float>(w) / 63.0f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float val = 0.0f;
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                for (int h = 1; h <= 32; ++h)
                {
                    float amp = std::exp(-static_cast<float>(h) * (0.3f - brightness * 0.25f));
                    val += amp * std::sin(kTwoPi * static_cast<float>(h) * phase);
                }
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 4: Anharmonic — bells, metallic
    static void generateAnharmonic(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float inharm = 0.01f + static_cast<float>(w) * 0.005f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float val = 0.0f;
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                for (int h = 1; h <= 16; ++h)
                {
                    float ratio = static_cast<float>(h) * (1.0f + inharm * static_cast<float>(h * h));
                    float amp = 1.0f / static_cast<float>(h);
                    val += amp * std::sin(kTwoPi * ratio * phase);
                }
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 5: Electric Piano
    static void generateEPiano(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float bark = static_cast<float>(w) / 63.0f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                float sine = std::sin(kTwoPi * phase);
                float h2 = std::sin(kTwoPi * 2.0f * phase) * 0.5f * bark;
                float h3 = std::sin(kTwoPi * 3.0f * phase) * 0.25f * bark;
                float h7 = std::sin(kTwoPi * 7.0f * phase) * 0.1f * bark;
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = sine + h2 + h3 + h7;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 6: Bass Waves
    static void generateBassWaves(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float grit = static_cast<float>(w) / 63.0f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                float val = std::sin(kTwoPi * phase);
                val += std::sin(kTwoPi * 2.0f * phase) * 0.5f * grit;
                val += (phase < 0.5f ? 1.0f : -1.0f) * 0.3f * grit;
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 7: Organ Waves — drawbar-like additive
    static void generateOrganWaves(Wavetable& table)
    {
        // Drawbar footages: 16', 5-1/3', 8', 4', 2-2/3', 2', 1-3/5', 1-1/3', 1'
        constexpr float drawbarRatios[] = { 0.5f, 1.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f };
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float val = 0.0f;
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                for (int d = 0; d < 9; ++d)
                {
                    float drawAmt = (d < static_cast<int>(1 + (w * 9) / 64)) ? 1.0f : 0.0f;
                    val += drawAmt * std::sin(kTwoPi * drawbarRatios[d] * phase) / 9.0f;
                }
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 8: Vocal Waves — formant-like
    static void generateVocalWaves(Wavetable& table)
    {
        // Formant frequencies scaled to harmonic indices for "ah"->"oh" morph
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float morph = static_cast<float>(w) / 63.0f;
            float f1 = 4.0f + morph * (-1.5f);    // ~800Hz->500Hz formant 1
            float f2 = 10.0f + morph * (-4.0f);   // ~2000Hz->1200Hz formant 2
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float val = 0.0f;
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                for (int h = 1; h <= 32; ++h)
                {
                    float fh = static_cast<float>(h);
                    float amp = std::exp(-(fh - f1) * (fh - f1) * 0.08f)
                              + std::exp(-(fh - f2) * (fh - f2) * 0.04f) * 0.5f;
                    val += amp * std::sin(kTwoPi * fh * phase);
                }
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 9: Pad Waves — soft, evolving
    static void generatePadWaves(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float complexity = static_cast<float>(w) / 63.0f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                float val = std::sin(kTwoPi * phase);
                int maxH = 2 + static_cast<int>(complexity * 12.0f);
                for (int h = 2; h <= maxH; ++h)
                {
                    float amp = 0.5f / static_cast<float>(h * h);
                    val += amp * std::sin(kTwoPi * static_cast<float>(h) * phase + complexity * static_cast<float>(h));
                }
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 10: Digital Harsh — aggressive
    static void generateDigitalHarsh(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            int bits = 2 + (w * 14) / 63;  // 2-bit to 16-bit quantization
            float levels = static_cast<float>(1 << bits);
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                float saw = 2.0f * phase - 1.0f;
                float quantized = std::round(saw * levels) / levels;
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = quantized;
            }
        }
    }

    // Table 11: Sync Sweep — hard-sync harmonics
    static void generateSyncSweep(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float syncRatio = 1.0f + static_cast<float>(w) * 0.12f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                float syncPhase = std::fmod(phase * syncRatio, 1.0f);
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = std::sin(kTwoPi * syncPhase);
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 12: Classic Analog — sine → saw → square → pulse
    static void generateClassicAnalog(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float morph = static_cast<float>(w) / 63.0f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                float sine = std::sin(kTwoPi * phase);
                float saw = 2.0f * phase - 1.0f;
                float square = phase < 0.5f ? 1.0f : -1.0f;

                float val;
                if (morph < 0.33f)
                    val = sine * (1.0f - morph * 3.0f) + saw * (morph * 3.0f);
                else if (morph < 0.66f)
                    val = saw * (1.0f - (morph - 0.33f) * 3.0f) + square * ((morph - 0.33f) * 3.0f);
                else
                    val = square;

                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
        }
    }

    // Table 13: Additive — pure overtone series
    static void generateAdditive(Wavetable& table)
    {
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            int numH = 1 + (w * 31) / 63;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float val = 0.0f;
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                for (int h = 1; h <= numH; ++h)
                {
                    val += (1.0f / static_cast<float>(h)) * std::sin(kTwoPi * static_cast<float>(h) * phase);
                }
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = val;
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    // Table 14: Noise Shapes — noisy to tonal
    static void generateNoiseShapes(Wavetable& table)
    {
        // Use deterministic pseudo-random based on position
        for (int w = 0; w < kWavesPerTable; ++w)
        {
            float tonal = static_cast<float>(w) / 63.0f;
            for (int s = 0; s < kSamplesPerWave; ++s)
            {
                float phase = static_cast<float>(s) / static_cast<float>(kSamplesPerWave);
                float sine = std::sin(kTwoPi * phase);
                // Simple hash-based noise
                uint32_t seed = static_cast<uint32_t>(w * 256 + s) * 2654435761u;
                float noise = static_cast<float>(seed & 0xFFFF) / 32768.0f - 1.0f;
                table[static_cast<size_t>(w)][static_cast<size_t>(s)] = sine * tonal + noise * (1.0f - tonal);
            }
            normalizeWaveform(table[static_cast<size_t>(w)]);
        }
    }

    static void normalizeWaveform(Waveform& wf)
    {
        float peak = 0.0f;
        for (float s : wf)
            peak = std::max(peak, std::abs(s));
        if (peak > 0.001f)
        {
            float scale = 1.0f / peak;
            for (float& s : wf)
                s *= scale;
        }
    }
};

} // namespace axelf::ppgwave
