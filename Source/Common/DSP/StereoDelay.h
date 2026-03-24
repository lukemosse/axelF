#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace axelf::dsp
{

/**
 *  Stereo delay with independent L/R times, feedback highcut,
 *  ping-pong mode, and tempo-sync support.
 */
class StereoDelay
{
public:
    /** Tempo-sync note values. */
    enum class SyncMode
    {
        Off = 0,
        Quarter,      // 1/4
        Eighth,       // 1/8
        DottedEighth, // dotted 1/8
        Sixteenth,    // 1/16
        Triplet       // 1/8 triplet
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setTimeL     (float ms);      // 1–2000 ms
    void setTimeR     (float ms);
    void setFeedback  (float fb);      // 0–0.95
    void setMix       (float v);       // 0–1
    void setHighCut   (float freqHz);  // 1k–20k
    void setPingPong  (bool on);
    void setSyncMode  (SyncMode mode);
    void setTempoBpm  (float bpm);     // for tempo-sync

    /** Process a stereo buffer in-place. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    float syncModeToMs (SyncMode mode, float bpm) const;
    void  updateHighCutCoeff();

    double currentSampleRate = 44100.0;

    // Circular delay buffer (stereo interleaved: L then R)
    std::vector<float> delayBufL, delayBufR;
    int writePos = 0;
    int maxDelaySamples = 0;

    // Parameters
    float timeL_ms    = 375.0f;
    float timeR_ms    = 375.0f;
    float feedback    = 0.3f;
    float mixLevel    = 0.5f;
    float highCutFreq = 12000.0f;
    bool  pingPong    = false;
    SyncMode syncMode = SyncMode::Off;
    float tempoBpm    = 120.0f;

    // Smoothed delay times (in samples) to avoid transient clicks
    float smoothedDelaySamplesL = 0.0f;
    float smoothedDelaySamplesR = 0.0f;
    float delaySmoothCoeff      = 0.999f;  // one-pole smooth (~50ms at 44.1k)
    bool  delayTimeInitialised  = false;

    // Smoothed feedback & mix to avoid discontinuity pops
    float smoothedFeedback = 0.3f;
    float smoothedMix      = 0.5f;
    float paramSmoothCoeff = 0.999f;  // ~50ms one-pole for params

    // One-pole lowpass in feedback path (per channel)
    float lpStateL = 0.0f;
    float lpStateR = 0.0f;
    float lpCoeff  = 0.5f;  // coefficient for one-pole LP

    // DC blocker state (one-pole HPF at ~5 Hz per channel)
    float dcBlockL_x1 = 0.0f;
    float dcBlockL_y1 = 0.0f;
    float dcBlockR_x1 = 0.0f;
    float dcBlockR_y1 = 0.0f;
    float dcBlockCoeff = 0.995f;  // R ≈ 1 - (2π * 5Hz / fs)
};

} // namespace axelf::dsp
