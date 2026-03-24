#include "MasterBus.h"
#include <cmath>

namespace axelf::dsp
{

void MasterBus::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (maxBlockSize);
    spec.numChannels      = 2;

    lowShelf.prepare (spec);
    midPeak.prepare (spec);
    highShelf.prepare (spec);
    compressor.prepare (spec);
    limiter.prepare (spec);

    eqDirty = true;
    compDirty = true;
    limiterDirty = true;

    updateEq();
    updateCompressor();
}

void MasterBus::reset()
{
    lowShelf.reset();
    midPeak.reset();
    highShelf.reset();
    compressor.reset();
    limiter.reset();
}

// ── EQ setters ───────────────────────────────────────────────
void MasterBus::setEqLow  (float dB) { if (eqLowDb  != dB) { eqLowDb  = dB; eqDirty = true; } }
void MasterBus::setEqMid  (float dB) { if (eqMidDb  != dB) { eqMidDb  = dB; eqDirty = true; } }
void MasterBus::setEqHigh (float dB) { if (eqHighDb != dB) { eqHighDb = dB; eqDirty = true; } }
void MasterBus::setEqMidQ (float q)  { if (eqMidQ   != q)  { eqMidQ   = q;  eqDirty = true; } }

// ── Compressor setters ──────────────────────────────────────
void MasterBus::setCompThreshold (float dB) { if (compThresholdDb != dB) { compThresholdDb = dB; compDirty = true; } }
void MasterBus::setCompRatio     (float r)  { if (compRatio       != r)  { compRatio       = r;  compDirty = true; } }
void MasterBus::setCompAttack    (float ms) { if (compAttackMs    != ms) { compAttackMs    = ms; compDirty = true; } }
void MasterBus::setCompRelease   (float ms) { if (compReleaseMs   != ms) { compReleaseMs   = ms; compDirty = true; } }

// ── Limiter setters ─────────────────────────────────────────
void MasterBus::setLimiterEnabled (bool on) { limiterOn = on; }
void MasterBus::setLimiterCeiling (float dB) { if (limiterCeiling != dB) { limiterCeiling = dB; limiterDirty = true; } }

// ── Update internals ────────────────────────────────────────
void MasterBus::updateEq()
{
    if (! eqDirty)
        return;
    eqDirty = false;

    const float sr = static_cast<float> (currentSampleRate);

    *lowShelf.state  = *IIRCoeffs::makeLowShelf (sr, 100.0f, 0.707f, juce::Decibels::decibelsToGain (eqLowDb));
    *midPeak.state   = *IIRCoeffs::makePeakFilter (sr, 1000.0f, eqMidQ, juce::Decibels::decibelsToGain (eqMidDb));
    *highShelf.state = *IIRCoeffs::makeHighShelf (sr, 8000.0f, 0.707f, juce::Decibels::decibelsToGain (eqHighDb));
}

void MasterBus::updateCompressor()
{
    if (! compDirty)
        return;
    compDirty = false;

    compressor.setThreshold (compThresholdDb);
    compressor.setRatio (compRatio);
    compressor.setAttack (compAttackMs);
    compressor.setRelease (compReleaseMs);
}

// ── Process ─────────────────────────────────────────────────
void MasterBus::process (juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    updateEq();
    updateCompressor();

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // EQ chain
    lowShelf.process (context);
    midPeak.process (context);
    highShelf.process (context);

    // Compressor
    compressor.process (context);

    // Limiter
    if (limiterOn)
    {
        if (limiterDirty)
        {
            limiter.setThreshold (limiterCeiling);
            limiter.setRelease (50.0f);
            limiterDirty = false;
        }
        limiter.process (context);
    }
}

} // namespace axelf::dsp
