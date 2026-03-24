#include "StereoDelay.h"
#include <cmath>
#include <algorithm>

namespace axelf::dsp
{

void StereoDelay::prepare (double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;

    // Max 5 seconds of delay to support slow tempos (e.g. 20 BPM quarter note = 3s)
    maxDelaySamples = static_cast<int> (std::ceil (sampleRate * 5.0)) + 1;
    delayBufL.assign (static_cast<size_t> (maxDelaySamples), 0.0f);
    delayBufR.assign (static_cast<size_t> (maxDelaySamples), 0.0f);
    writePos = 0;

    lpStateL = 0.0f;
    lpStateR = 0.0f;

    // ~50ms one-pole smoothing for delay time changes
    delaySmoothCoeff = std::exp (-1.0f / static_cast<float> (sampleRate * 0.05));
    delayTimeInitialised = false;

    // ~5ms one-pole smoothing for feedback/mix parameter changes
    paramSmoothCoeff = std::exp (-1.0f / static_cast<float> (sampleRate * 0.005));
    smoothedFeedback = feedback;
    smoothedMix = mixLevel;

    // DC blocker: one-pole HPF at ~5 Hz
    // y[n] = R * (y[n-1] + x[n] - x[n-1])  where R = 1 - (2*pi*fc/fs)
    dcBlockCoeff = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f / static_cast<float> (sampleRate));
    dcBlockL_x1 = dcBlockL_y1 = 0.0f;
    dcBlockR_x1 = dcBlockR_y1 = 0.0f;

    updateHighCutCoeff();
}

void StereoDelay::reset()
{
    std::fill (delayBufL.begin(), delayBufL.end(), 0.0f);
    std::fill (delayBufR.begin(), delayBufR.end(), 0.0f);
    writePos = 0;
    lpStateL = 0.0f;
    lpStateR = 0.0f;
    dcBlockL_x1 = dcBlockL_y1 = 0.0f;
    dcBlockR_x1 = dcBlockR_y1 = 0.0f;
}

void StereoDelay::setTimeL    (float ms) { timeL_ms = ms; }
void StereoDelay::setTimeR    (float ms) { timeR_ms = ms; }
void StereoDelay::setFeedback (float fb) { feedback = std::min (fb, 0.95f); }
void StereoDelay::setMix      (float v)  { mixLevel = v; }
void StereoDelay::setPingPong (bool on)   { pingPong = on; }
void StereoDelay::setSyncMode (SyncMode m) { syncMode = m; }
void StereoDelay::setTempoBpm (float bpm) { tempoBpm = std::max (bpm, 20.0f); }

void StereoDelay::setHighCut (float freqHz)
{
    highCutFreq = freqHz;
    updateHighCutCoeff();
}

void StereoDelay::updateHighCutCoeff()
{
    // One-pole lowpass: y[n] = (1-a)*x[n] + a*y[n-1]
    //   a = exp(-2*pi*fc/fs)
    const float fc = std::clamp (highCutFreq, 200.0f, static_cast<float> (currentSampleRate * 0.49));
    lpCoeff = std::exp (-juce::MathConstants<float>::twoPi * fc / static_cast<float> (currentSampleRate));
}

float StereoDelay::syncModeToMs (SyncMode mode, float bpm) const
{
    if (bpm <= 0.0f) return 375.0f;
    const float beatMs = 60000.0f / bpm; // quarter-note duration

    switch (mode)
    {
        case SyncMode::Off:           return 0.0f;
        case SyncMode::Quarter:       return beatMs;
        case SyncMode::Eighth:        return beatMs * 0.5f;
        case SyncMode::DottedEighth:  return beatMs * 0.75f;
        case SyncMode::Sixteenth:     return beatMs * 0.25f;
        case SyncMode::Triplet:       return beatMs / 3.0f;
    }
    return beatMs * 0.5f;
}

void StereoDelay::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    // Resolve actual delay times
    float actualL = timeL_ms;
    float actualR = timeR_ms;

    if (syncMode != SyncMode::Off)
    {
        const float syncMs = syncModeToMs (syncMode, tempoBpm);
        actualL = syncMs;
        actualR = syncMs;
    }

    const int bufLen = maxDelaySamples;

    // Convert ms to fractional samples and clamp to buffer size
    const float maxSamples = static_cast<float>(bufLen - 2);
    const float targetDelaySamplesL = std::clamp(static_cast<float> (currentSampleRate) * actualL * 0.001f, 1.0f, maxSamples);
    const float targetDelaySamplesR = std::clamp(static_cast<float> (currentSampleRate) * actualR * 0.001f, 1.0f, maxSamples);

    // Initialise smoothed values on first use (snap, don't ramp from zero)
    if (!delayTimeInitialised)
    {
        smoothedDelaySamplesL = targetDelaySamplesL;
        smoothedDelaySamplesR = targetDelaySamplesR;
        delayTimeInitialised = true;
    }

    float* dataL = buffer.getWritePointer (0);
    float* dataR = buffer.getWritePointer (1);

    const float smoothCoeff = delaySmoothCoeff;

    for (int i = 0; i < numSamples; ++i)
    {
        // Smooth delay time per-sample to avoid transient clicks
        smoothedDelaySamplesL = smoothCoeff * smoothedDelaySamplesL + (1.0f - smoothCoeff) * targetDelaySamplesL;
        smoothedDelaySamplesR = smoothCoeff * smoothedDelaySamplesR + (1.0f - smoothCoeff) * targetDelaySamplesR;

        // Smooth feedback and mix to avoid discontinuity pops
        smoothedFeedback = paramSmoothCoeff * smoothedFeedback + (1.0f - paramSmoothCoeff) * feedback;
        smoothedMix      = paramSmoothCoeff * smoothedMix      + (1.0f - paramSmoothCoeff) * mixLevel;

        const float dryL = dataL[i];
        const float dryR = dataR[i];

        // ── Read from delay line (linear interpolation) ───────
        auto readInterp = [&] (const std::vector<float>& buf, float delaySamp) -> float
        {
            if (std::isnan(delaySamp)) delaySamp = 0.0f;
            float readPosF = static_cast<float> (writePos) - delaySamp;
            while (readPosF < 0.0f)
                readPosF += static_cast<float> (bufLen);

            int idx0 = static_cast<int> (readPosF);
            if (idx0 < 0) idx0 = 0;
            if (idx0 >= bufLen) idx0 = 0;

            int idx1 = idx0 + 1;
            if (idx1 >= bufLen) idx1 = 0;

            float frac = readPosF - static_cast<float> (idx0);
            return buf[static_cast<size_t> (idx0)] * (1.0f - frac)
                 + buf[static_cast<size_t> (idx1)] * frac;
        };

        const float delayedL = readInterp (delayBufL, smoothedDelaySamplesL);
        const float delayedR = readInterp (delayBufR, smoothedDelaySamplesR);

        // ── Feedback with highcut LP, DC blocker, and soft saturation ─────
        // A constantly added DC offset can stress the DC blocker.
        // To prevent denormal CPU spikes (which cause DAW dropouts/pops), 
        // we snap tiny values to zero manually since std::tanh often bypasses SSE DAZ flags.
        float fbL = delayedL * smoothedFeedback;
        float fbR = delayedR * smoothedFeedback;
        if (std::abs(fbL) < 1.0e-15f) fbL = 0.0f;
        if (std::abs(fbR) < 1.0e-15f) fbR = 0.0f;

        // One-pole lowpass in feedback path
        lpStateL = fbL + lpCoeff * (lpStateL - fbL);
        lpStateR = fbR + lpCoeff * (lpStateR - fbR);
        if (std::abs(lpStateL) < 1.0e-15f) lpStateL = 0.0f;
        if (std::abs(lpStateR) < 1.0e-15f) lpStateR = 0.0f;

        // DC blocker (one-pole HPF): removes accumulated DC offset
        // y[n] = R * (y[n-1] + x[n] - x[n-1])
        float dcOutL = dcBlockCoeff * (dcBlockL_y1 + lpStateL - dcBlockL_x1);
        float dcOutR = dcBlockCoeff * (dcBlockR_y1 + lpStateR - dcBlockR_x1);
        if (std::abs(dcOutL) < 1.0e-15f) dcOutL = 0.0f;
        if (std::abs(dcOutR) < 1.0e-15f) dcOutR = 0.0f;

        dcBlockL_x1 = lpStateL;
        dcBlockR_x1 = lpStateR;
        dcBlockL_y1 = dcOutL;
        dcBlockR_y1 = dcOutR;

        // Fast soft saturation prevents feedback loops from blowing up.
        // We use a custom cubic clipper because std::tanh triggers x87 FPU 
        // denormal exceptions on Windows, causing massive CPU spikes and audio pops.
        auto softClip = [](float x) -> float {
            if (x > 1.5f)  return 1.0f;
            if (x < -1.5f) return -1.0f;
            return x - (x * x * x) / 6.75f;
        };

        fbL = softClip(dcOutL);
        fbR = softClip(dcOutR);

        // ── Write to delay line (linear sum so we don't distort the dry signal) ──
        if (pingPong)
        {
            delayBufL[static_cast<size_t> (writePos)] = dryL + fbR;
            delayBufR[static_cast<size_t> (writePos)] = dryR + fbL;
        }
        else
        {
            delayBufL[static_cast<size_t> (writePos)] = dryL + fbL;
            delayBufR[static_cast<size_t> (writePos)] = dryR + fbR;
        }

        // ── Output: wet only (send/return topology) ───────────
        dataL[i] = delayedL * smoothedMix;
        dataR[i] = delayedR * smoothedMix;

        writePos = (writePos + 1) % bufLen;
    }
}

} // namespace axelf::dsp
