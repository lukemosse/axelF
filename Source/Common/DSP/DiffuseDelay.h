#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>

namespace axelf::dsp
{

/**
 *  Feedback Delay Network (FDN) delay with allpass diffusion,
 *  modulated delay lines, and Hadamard feedback matrix.
 *
 *  Inspired by Valhalla Supermassive architecture:
 *    - 4-line FDN prevents resonant buildup (no peaking)
 *    - Allpass pre-diffusion smears transients into wash
 *    - Modulated read positions decorrelate feedback
 *    - Dual-band damping (LP + HP) in feedback path
 *    - Density and Warp controls for character shaping
 *
 *  Drop-in replacement for StereoDelay on Aux Send 2.
 *  Processes stereo buffer in-place (wet-only, send/return topology).
 */
class DiffuseDelay
{
public:
    /** Tempo-sync note values (same as StereoDelay for compatibility). */
    enum class SyncMode
    {
        Off = 0,
        Quarter,
        Eighth,
        DottedEighth,
        Sixteenth,
        Triplet
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // ── Parameters ───────────────────────────────────────────
    void setTime      (float ms);         // 1–2000 ms  (base delay time)
    void setFeedback  (float fb);         // 0–1.0  (safe: FDN can't blow up)
    void setMix       (float v);          // 0–1
    void setDensity   (float d);          // 0–1  (allpass diffusion amount)
    void setWarp      (float w);          // 0–1  (delay tap spacing warp)
    void setModDepth  (float d);          // 0–1  (pitch mod depth in delay lines)
    void setModRate   (float hz);         // 0.01–10 Hz
    void setDampLow   (float freqHz);     // 20–2000 Hz  (HP in feedback removes lows)
    void setDampHigh  (float freqHz);     // 1000–20000 Hz  (LP in feedback removes highs)
    void setWidth     (float w);          // 0–1  (stereo spread)
    void setSyncMode  (SyncMode mode);
    void setTempoBpm  (float bpm);

    /** Process a stereo buffer in-place (wet-only output). */
    void process (juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kNumLines = 4;   // FDN order
    static constexpr int kNumAllpass = 2; // pre-diffusion stages per channel

    float syncModeToMs (SyncMode mode, float bpm) const;

    // ── Per-line state ──────────────────────────────────────
    struct DelayLine
    {
        std::vector<float> buffer;
        int writePos = 0;
        int length   = 0;   // current allocated length
    };

    struct AllpassStage
    {
        std::vector<float> buffer;
        int writePos = 0;
        int length   = 0;
    };

    // ── Core state ──────────────────────────────────────────
    double currentSampleRate = 44100.0;
    int maxDelaySamples = 0;

    std::array<DelayLine, kNumLines> lines;

    // Pre-diffusion allpass chains (stereo: L uses [0..1], R uses [2..3])
    std::array<AllpassStage, kNumAllpass * 2> allpassStages;

    // FDN feedback state (one value per line, persists across blocks)
    std::array<float, kNumLines> fdnState {};

    // Damping filter state (per line: LP and HP one-pole)
    std::array<float, kNumLines> lpState {};
    std::array<float, kNumLines> hpState {};      // HP filter output state
    std::array<float, kNumLines> hpPrevInput {};  // HP filter previous input (LP output)
    float lpCoeff = 0.5f;    // for high-frequency damping
    float hpCoeff = 0.001f;  // for low-frequency damping

    // Modulation LFOs (one per line, phase-offset 90° apart)
    std::array<double, kNumLines> modPhase {};
    float modPhaseInc = 0.0f;

    // DC blocker state (stereo output)
    float dcBlockL_x1 = 0.0f, dcBlockL_y1 = 0.0f;
    float dcBlockR_x1 = 0.0f, dcBlockR_y1 = 0.0f;
    float dcBlockCoeff = 0.995f;

    // Smoothed parameters
    float targetTimeMs    = 375.0f;
    float smoothedTimeMs  = 375.0f;
    float feedback        = 0.5f;
    float smoothedFeedback = 0.5f;
    float mixLevel        = 0.5f;
    float smoothedMix     = 0.5f;
    float density         = 0.5f;
    float warp            = 0.0f;
    float modDepth        = 0.3f;
    float modRateHz       = 0.5f;
    float dampHighFreq    = 12000.0f;
    float dampLowFreq     = 80.0f;
    float width           = 1.0f;
    float smoothedWidth   = 1.0f;
    SyncMode syncMode     = SyncMode::Off;
    float tempoBpm        = 120.0f;

    // Smoothing coefficients
    float timeSmoothCoeff  = 0.999f;
    float paramSmoothCoeff = 0.999f;

    bool initialised = false;

    // ── Helpers ─────────────────────────────────────────────
    void updateDampingCoeffs();
    void recalcAllpassLengths();

    /** Prime-number delay lengths relative to base time — ensures no common resonances. */
    static constexpr std::array<float, kNumLines> kLineRatios = { 1.0f, 0.7937f, 0.6300f, 0.5f };

    /** Allpass time ratios (short, prime-friendly). */
    static constexpr std::array<float, kNumAllpass * 2> kAllpassRatios =
        { 0.0293f, 0.0415f, 0.0317f, 0.0449f };

    /** Allpass diffusion coefficient — scaled by density. */
    static constexpr float kAllpassBaseCoeff = 0.7f;
};

} // namespace axelf::dsp
