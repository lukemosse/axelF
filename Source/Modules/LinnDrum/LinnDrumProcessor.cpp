#include "LinnDrumProcessor.h"
#include "LinnDrumVoice.h"
#include "LinnDrumEditor.h"
#include <cmath>
#include <random>

namespace axelf::linndrum
{

class LinnDrumSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

LinnDrumProcessor::LinnDrumProcessor()
    : apvts(dummyProcessor, nullptr, "LinnDrum", createParameterLayout())
{
    for (int i = 0; i < 15; ++i)
        synth.addVoice(new LinnDrumVoice());

    synth.addSound(new LinnDrumSound());

    formatManager.registerBasicFormats();
}

void LinnDrumProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    synth.setCurrentPlaybackSampleRate(sampleRate);

    loadAllBanks(sampleRate);

    // Default bank from parameter
    int bankIdx = 1; // OG Clean
    if (auto* p = apvts.getRawParameterValue("linn_bank"))
        bankIdx = juce::jlimit(0, static_cast<int>(allBanks.size()) - 1, static_cast<int>(p->load()));
    lastBankIndex = bankIdx;

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<LinnDrumVoice*>(synth.getVoice(i)))
            voice->setSampleBank(&allBanks[static_cast<size_t>(bankIdx)]);
    }
}

void LinnDrumProcessor::loadAllBanks(double sr)
{
    const auto& folderNames = getBankFolderNames();
    const int numDiskBanks = folderNames.size();

    allBanks.resize(static_cast<size_t>(1 + numDiskBanks)); // [0]=synthetic, [1..N]=disk

    // Bank 0: synthetic fallback
    generateSyntheticSamples(allBanks[0], sr);

    // Banks 1-N: load from disk, fill gaps with synthetic
    for (int b = 0; b < numDiskBanks; ++b)
    {
        if (!loadBankFromDisk(b + 1, folderNames[b], sr))
        {
            // If folder not found, clone synthetic bank
            allBanks[static_cast<size_t>(b + 1)] = allBanks[0];
        }
        else
        {
            // Fill any empty slots with synthetic samples
            auto& bank = allBanks[static_cast<size_t>(b + 1)];
            for (int s = 0; s < 15; ++s)
            {
                if (bank.samples[static_cast<size_t>(s)].data.empty())
                    bank.samples[static_cast<size_t>(s)] = allBanks[0].samples[static_cast<size_t>(s)];
            }
        }
    }
}

bool LinnDrumProcessor::loadBankFromDisk(int bankIndex, const juce::String& folderName, double sr)
{
    const juce::String basePath(
        "C:\\Users\\mosse\\OneDrive\\Documents\\Sampled Instruments\\"
        "Lindrum From Mars\\Maschine\\Lindrum From Mars");

    juce::File dir(basePath + "\\" + folderName);
    if (!dir.isDirectory())
        return false;

    auto& bank = allBanks[static_cast<size_t>(bankIndex)];

    // Collect all WAV files
    juce::Array<juce::File> allFiles;
    dir.findChildFiles(allFiles, juce::File::findFiles, false, "*.wav");

    // Prefix → drum slot mapping:
    // BD→kick(0), SD→snare(1), CH→closedHH(2), OH→openHH(3),
    // Ride→ride(4), Crash→crash(5), Tom→toms(6,7,8), Conga→congas(9,10),
    // Clap→clap(11), Cowbell→cowbell(12), Tamb→tambourine(13), Cabasa→cabasa(14)
    struct PrefixMapping { const char* prefix; int slot; };
    static const PrefixMapping singleSlots[] = {
        { "BD",      0 },
        { "SD",      1 },
        { "CH",      2 },
        { "OH",      3 },
        { "Ride",    4 },
        { "Crash",   5 },
        { "Clap",   11 },
        { "Cowbell", 12 },
        { "Tamb",   13 },
        { "Cabasa", 14 }
    };

    // For single-slot prefixes, pick the first alphabetically sorted match
    for (const auto& pm : singleSlots)
    {
        juce::Array<juce::File> matches;
        for (auto& f : allFiles)
            if (f.getFileName().startsWith(pm.prefix))
                matches.add(f);

        if (matches.isEmpty()) continue;
        matches.sort();

        auto& sample = bank.samples[static_cast<size_t>(pm.slot)];
        // Load the first match
        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(matches[0]));
        if (reader == nullptr) continue;

        const int numSrc = static_cast<int>(reader->lengthInSamples);
        juce::AudioBuffer<float> tempBuf(static_cast<int>(reader->numChannels), numSrc);
        reader->read(&tempBuf, 0, numSrc, 0, true, true);

        // Mix to mono
        std::vector<float> mono(static_cast<size_t>(numSrc));
        if (reader->numChannels >= 2)
        {
            const float* c0 = tempBuf.getReadPointer(0);
            const float* c1 = tempBuf.getReadPointer(1);
            for (int s = 0; s < numSrc; ++s)
                mono[static_cast<size_t>(s)] = (c0[s] + c1[s]) * 0.5f;
        }
        else
        {
            std::memcpy(mono.data(), tempBuf.getReadPointer(0),
                        static_cast<size_t>(numSrc) * sizeof(float));
        }

        // Resample if needed
        double ratio = reader->sampleRate / sr;
        if (std::abs(ratio - 1.0) < 0.001)
        {
            sample.data = std::move(mono);
            sample.sampleRate = sr;
        }
        else
        {
            int newLen = static_cast<int>(static_cast<double>(numSrc) / ratio);
            sample.data.resize(static_cast<size_t>(newLen));
            sample.sampleRate = sr;
            for (int s = 0; s < newLen; ++s)
            {
                double srcPos = static_cast<double>(s) * ratio;
                int i0 = static_cast<int>(srcPos);
                int i1 = std::min(i0 + 1, numSrc - 1);
                float frac = static_cast<float>(srcPos - static_cast<double>(i0));
                sample.data[static_cast<size_t>(s)] =
                    mono[static_cast<size_t>(i0)] * (1.0f - frac) +
                    mono[static_cast<size_t>(i1)] * frac;
            }
        }
    }

    // Multi-slot: Tom → indices 6 (hi), 7 (mid), 8 (lo)
    {
        juce::Array<juce::File> matches;
        for (auto& f : allFiles)
            if (f.getFileName().startsWith("Tom"))
                matches.add(f);
        matches.sort();
        int tomSlots[] = { 6, 7, 8 };
        for (int t = 0; t < juce::jmin(static_cast<int>(matches.size()), 3); ++t)
        {
            auto& sample = bank.samples[static_cast<size_t>(tomSlots[t])];
            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(matches[t]));
            if (reader == nullptr) continue;

            const int numSrc = static_cast<int>(reader->lengthInSamples);
            juce::AudioBuffer<float> tempBuf(static_cast<int>(reader->numChannels), numSrc);
            reader->read(&tempBuf, 0, numSrc, 0, true, true);

            std::vector<float> mono(static_cast<size_t>(numSrc));
            if (reader->numChannels >= 2)
            {
                const float* c0 = tempBuf.getReadPointer(0);
                const float* c1 = tempBuf.getReadPointer(1);
                for (int s = 0; s < numSrc; ++s)
                    mono[static_cast<size_t>(s)] = (c0[s] + c1[s]) * 0.5f;
            }
            else
            {
                std::memcpy(mono.data(), tempBuf.getReadPointer(0),
                            static_cast<size_t>(numSrc) * sizeof(float));
            }

            double ratio = reader->sampleRate / sr;
            if (std::abs(ratio - 1.0) < 0.001)
            {
                sample.data = std::move(mono);
                sample.sampleRate = sr;
            }
            else
            {
                int newLen = static_cast<int>(static_cast<double>(numSrc) / ratio);
                sample.data.resize(static_cast<size_t>(newLen));
                sample.sampleRate = sr;
                for (int s = 0; s < newLen; ++s)
                {
                    double srcPos = static_cast<double>(s) * ratio;
                    int i0 = static_cast<int>(srcPos);
                    int i1 = std::min(i0 + 1, numSrc - 1);
                    float frac = static_cast<float>(srcPos - static_cast<double>(i0));
                    sample.data[static_cast<size_t>(s)] =
                        mono[static_cast<size_t>(i0)] * (1.0f - frac) +
                        mono[static_cast<size_t>(i1)] * frac;
                }
            }
        }
    }

    // Multi-slot: Conga → indices 9 (hi), 10 (lo)
    {
        juce::Array<juce::File> matches;
        for (auto& f : allFiles)
            if (f.getFileName().startsWith("Conga"))
                matches.add(f);
        matches.sort();
        int congaSlots[] = { 9, 10 };
        int numToLoad = juce::jmin(static_cast<int>(matches.size()), 2);
        // If more than 2, pick first and last for hi/lo spread
        juce::Array<juce::File> picked;
        if (matches.size() >= 2)
        {
            picked.add(matches[0]);
            picked.add(matches[matches.size() - 1]);
        }
        else if (matches.size() == 1)
        {
            picked.add(matches[0]);
        }

        for (int c = 0; c < picked.size(); ++c)
        {
            auto& sample = bank.samples[static_cast<size_t>(congaSlots[c])];
            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(picked[c]));
            if (reader == nullptr) continue;

            const int numSrc = static_cast<int>(reader->lengthInSamples);
            juce::AudioBuffer<float> tempBuf(static_cast<int>(reader->numChannels), numSrc);
            reader->read(&tempBuf, 0, numSrc, 0, true, true);

            std::vector<float> mono(static_cast<size_t>(numSrc));
            if (reader->numChannels >= 2)
            {
                const float* c0 = tempBuf.getReadPointer(0);
                const float* c1 = tempBuf.getReadPointer(1);
                for (int s = 0; s < numSrc; ++s)
                    mono[static_cast<size_t>(s)] = (c0[s] + c1[s]) * 0.5f;
            }
            else
            {
                std::memcpy(mono.data(), tempBuf.getReadPointer(0),
                            static_cast<size_t>(numSrc) * sizeof(float));
            }

            double ratio = reader->sampleRate / sr;
            if (std::abs(ratio - 1.0) < 0.001)
            {
                sample.data = std::move(mono);
                sample.sampleRate = sr;
            }
            else
            {
                int newLen = static_cast<int>(static_cast<double>(numSrc) / ratio);
                sample.data.resize(static_cast<size_t>(newLen));
                sample.sampleRate = sr;
                for (int s = 0; s < newLen; ++s)
                {
                    double srcPos = static_cast<double>(s) * ratio;
                    int i0 = static_cast<int>(srcPos);
                    int i1 = std::min(i0 + 1, numSrc - 1);
                    float frac = static_cast<float>(srcPos - static_cast<double>(i0));
                    sample.data[static_cast<size_t>(s)] =
                        mono[static_cast<size_t>(i0)] * (1.0f - frac) +
                        mono[static_cast<size_t>(i1)] * frac;
                }
            }
        }
    }

    return true;
}

void LinnDrumProcessor::generateSyntheticSamples(DrumSampleBank& bank, double sr)
{
    constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
    std::mt19937 rng(42); // deterministic seed
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);

    auto makeSample = [&](int index, int lengthSamples, auto generator)
    {
        auto& s = bank.samples[static_cast<size_t>(index)];
        s.data.resize(static_cast<size_t>(lengthSamples));
        s.sampleRate = sr;
        for (int i = 0; i < lengthSamples; ++i)
            s.data[static_cast<size_t>(i)] = generator(i, sr);
    };

    // 0: Kick — sine sweep 150→50Hz with amplitude decay
    makeSample(0, static_cast<int>(sr * 0.4), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float freq = 50.0f + 100.0f * std::exp(-t * 30.0f);
        float env = std::exp(-t * 8.0f);
        static double kickPhase = 0.0;
        if (i == 0) kickPhase = 0.0;
        kickPhase += static_cast<double>(freq) / rate;
        return std::sin(static_cast<float>(kickPhase * kTwoPi)) * env * 0.9f;
    });

    // 1: Snare — sine body + noise burst
    makeSample(1, static_cast<int>(sr * 0.25), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float body = std::sin(180.0f * static_cast<float>(kTwoPi) * t) * std::exp(-t * 20.0f);
        float n = noise(rng) * std::exp(-t * 12.0f);
        return (body * 0.5f + n * 0.5f) * 0.8f;
    });

    // 2: Closed HH — filtered noise, very short
    makeSample(2, static_cast<int>(sr * 0.06), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        return noise(rng) * std::exp(-t * 40.0f) * 0.5f;
    });

    // 3: Open HH — noise, medium decay
    makeSample(3, static_cast<int>(sr * 0.3), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        return noise(rng) * std::exp(-t * 6.0f) * 0.45f;
    });

    // 4: Ride — metallic sines + noise, long decay
    makeSample(4, static_cast<int>(sr * 0.8), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float metal = std::sin(3200.0f * static_cast<float>(kTwoPi) * t) * 0.3f
                    + std::sin(5100.0f * static_cast<float>(kTwoPi) * t) * 0.2f;
        float n = noise(rng) * 0.15f;
        return (metal + n) * std::exp(-t * 3.0f) * 0.4f;
    });

    // 5: Crash — noise with long decay
    makeSample(5, static_cast<int>(sr * 1.2), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        return noise(rng) * std::exp(-t * 2.0f) * 0.5f;
    });

    // 6: Tom Hi (sine ~300Hz pitch sweep)
    makeSample(6, static_cast<int>(sr * 0.3), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float freq = 200.0f + 150.0f * std::exp(-t * 25.0f);
        static double tomHiPhase = 0.0;
        if (i == 0) tomHiPhase = 0.0;
        tomHiPhase += static_cast<double>(freq) / rate;
        return std::sin(static_cast<float>(tomHiPhase * kTwoPi)) * std::exp(-t * 10.0f) * 0.7f;
    });

    // 7: Tom Mid
    makeSample(7, static_cast<int>(sr * 0.35), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float freq = 150.0f + 120.0f * std::exp(-t * 25.0f);
        static double tomMidPhase = 0.0;
        if (i == 0) tomMidPhase = 0.0;
        tomMidPhase += static_cast<double>(freq) / rate;
        return std::sin(static_cast<float>(tomMidPhase * kTwoPi)) * std::exp(-t * 8.0f) * 0.7f;
    });

    // 8: Tom Lo
    makeSample(8, static_cast<int>(sr * 0.4), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float freq = 100.0f + 80.0f * std::exp(-t * 25.0f);
        static double tomLoPhase = 0.0;
        if (i == 0) tomLoPhase = 0.0;
        tomLoPhase += static_cast<double>(freq) / rate;
        return std::sin(static_cast<float>(tomLoPhase * kTwoPi)) * std::exp(-t * 7.0f) * 0.7f;
    });

    // 9: Conga Hi
    makeSample(9, static_cast<int>(sr * 0.2), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float freq = 350.0f + 100.0f * std::exp(-t * 40.0f);
        static double congaHiPhase = 0.0;
        if (i == 0) congaHiPhase = 0.0;
        congaHiPhase += static_cast<double>(freq) / rate;
        return std::sin(static_cast<float>(congaHiPhase * kTwoPi)) * std::exp(-t * 15.0f) * 0.6f;
    });

    // 10: Conga Lo
    makeSample(10, static_cast<int>(sr * 0.25), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float freq = 250.0f + 80.0f * std::exp(-t * 40.0f);
        static double congaLoPhase = 0.0;
        if (i == 0) congaLoPhase = 0.0;
        congaLoPhase += static_cast<double>(freq) / rate;
        return std::sin(static_cast<float>(congaLoPhase * kTwoPi)) * std::exp(-t * 12.0f) * 0.6f;
    });

    // 11: Clap — noise bursts
    makeSample(11, static_cast<int>(sr * 0.15), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        // Multiple short noise bursts
        float burst1 = (t < 0.01f) ? 1.0f : 0.0f;
        float burst2 = (t > 0.015f && t < 0.025f) ? 1.0f : 0.0f;
        float burst3 = (t > 0.03f && t < 0.04f) ? 1.0f : 0.0f;
        float tail = (t > 0.04f) ? std::exp(-(t - 0.04f) * 20.0f) : 0.0f;
        return noise(rng) * (burst1 + burst2 + burst3 + tail) * 0.7f;
    });

    // 12: Cowbell — two metallic sines
    makeSample(12, static_cast<int>(sr * 0.3), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        float s1 = std::sin(800.0f * static_cast<float>(kTwoPi) * t);
        float s2 = std::sin(540.0f * static_cast<float>(kTwoPi) * t);
        return (s1 * 0.5f + s2 * 0.5f) * std::exp(-t * 8.0f) * 0.5f;
    });

    // 13: Tambourine — high noise
    makeSample(13, static_cast<int>(sr * 0.15), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        // Simple high-pass approximation by differencing noise
        float n = noise(rng);
        return n * std::exp(-t * 15.0f) * 0.4f;
    });

    // 14: Cabasa — very high noise, very short
    makeSample(14, static_cast<int>(sr * 0.08), [&](int i, double rate) -> float
    {
        float t = static_cast<float>(i) / static_cast<float>(rate);
        return noise(rng) * std::exp(-t * 25.0f) * 0.35f;
    });
}

void LinnDrumProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    updateVoiceParameters();

    // Pattern triggers are now injected by PatternEngine via midiMessages.
    // The sequencer is kept as a data-only store for the UI grid.
    // No internal sequencer advance — just render whatever MIDI arrives.

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

void LinnDrumProcessor::updateVoiceParameters()
{
    auto safeLoad = [&](const juce::String& id, float fallback = 0.0f) -> float
    {
        if (auto* p = apvts.getRawParameterValue(id))
            return p->load();
        return fallback;
    };

    // Check for bank change
    int bankIdx = static_cast<int>(safeLoad("linn_bank", 1.0f));
    bankIdx = juce::jlimit(0, static_cast<int>(allBanks.size()) - 1, bankIdx);
    if (bankIdx != lastBankIndex)
    {
        lastBankIndex = bankIdx;
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<LinnDrumVoice*>(synth.getVoice(i)))
                voice->setSampleBank(&allBanks[static_cast<size_t>(bankIdx)]);
        }
    }

    auto& activeBank = allBanks[static_cast<size_t>(lastBankIndex)];
    activeBank.masterTune = safeLoad("linn_master_tune", 0.0f);

    const auto& names = getDrumNames();
    for (int d = 0; d < 15; ++d)
    {
        auto prefix = "linn_" + names[d] + "_";
        activeBank.drumParams[static_cast<size_t>(d)].level = safeLoad(prefix + "level", 0.8f);
        activeBank.drumParams[static_cast<size_t>(d)].tune  = safeLoad(prefix + "tune", 0.0f);
        activeBank.drumParams[static_cast<size_t>(d)].decay = safeLoad(prefix + "decay", 1.0f);
        activeBank.drumParams[static_cast<size_t>(d)].pan   = safeLoad(prefix + "pan", 0.0f);
    }
}

void LinnDrumProcessor::releaseResources() {}

juce::Component* LinnDrumProcessor::createEditor()
{
    return nullptr;  // Editors now created in PluginEditor with PatternEngine refs
}

} // namespace axelf::linndrum

