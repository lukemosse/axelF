#pragma once

#include <juce_dsp/juce_dsp.h>

namespace axelf::dsp
{

/**
 *  Master bus chain: 3-band EQ → Compressor → Brickwall limiter.
 *  All APVTS-friendly float parameters.
 */
class MasterBus
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // EQ
    void setEqLow  (float dB);   // ±12 shelf at 100 Hz
    void setEqMid  (float dB);   // ±12 parametric at 1 kHz
    void setEqHigh (float dB);   // ±12 shelf at 8 kHz
    void setEqMidQ (float q);    // Q for mid band (0.1–10)

    // Compressor
    void setCompThreshold (float dB);   // -40 to 0
    void setCompRatio     (float r);    // 1–20
    void setCompAttack    (float ms);   // 0.1–100
    void setCompRelease   (float ms);   // 10–1000

    // Limiter
    void setLimiterEnabled (bool on);
    void setLimiterCeiling (float dB);  // default -0.3

    /** Process a stereo buffer in-place. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    void updateEq();
    void updateCompressor();

    double currentSampleRate = 44100.0;

    // ── 3-band EQ ─────────────────────────────────────────────
    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using IIRCoeffs = juce::dsp::IIR::Coefficients<float>;

    juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoeffs> lowShelf, midPeak, highShelf;

    float eqLowDb  = 0.0f;
    float eqMidDb  = 0.0f;
    float eqHighDb = 0.0f;
    float eqMidQ   = 0.707f;
    bool  eqDirty  = true;

    // ── Compressor ────────────────────────────────────────────
    juce::dsp::Compressor<float> compressor;
    float compThresholdDb = 0.0f;
    float compRatio       = 1.0f;
    float compAttackMs    = 10.0f;
    float compReleaseMs   = 100.0f;
    bool  compDirty       = true;

    // ── Limiter ───────────────────────────────────────────────
    juce::dsp::Limiter<float> limiter;
    bool  limiterOn      = true;
    float limiterCeiling = -0.3f;
    bool  limiterDirty   = true;
};

} // namespace axelf::dsp
