#include "DiffuseDelay.h"
#include <cmath>
#include <algorithm>

namespace axelf::dsp
{

// ── prepare / reset ─────────────────────────────────────────

void DiffuseDelay::prepare (double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;

    // Max 5 seconds to support slow tempo sync
    maxDelaySamples = static_cast<int> (std::ceil (sampleRate * 5.0)) + 1;

    for (auto& line : lines)
    {
        line.buffer.assign (static_cast<size_t> (maxDelaySamples), 0.0f);
        line.writePos = 0;
        line.length   = maxDelaySamples;
    }

    // Allpass buffers — shorter (max ~200 ms for diffusion)
    const int maxAPSamples = static_cast<int> (std::ceil (sampleRate * 0.2)) + 1;
    for (auto& ap : allpassStages)
    {
        ap.buffer.assign (static_cast<size_t> (maxAPSamples), 0.0f);
        ap.writePos = 0;
        ap.length   = maxAPSamples;
    }

    fdnState.fill (0.0f);
    lpState.fill (0.0f);
    hpState.fill (0.0f);
    hpPrevInput.fill (0.0f);

    // Stagger mod LFO phases evenly across the 4 lines
    for (int i = 0; i < kNumLines; ++i)
    {
        modPhase[static_cast<size_t> (i)] = static_cast<double> (i) / static_cast<double> (kNumLines);
    }

    modPhaseInc = static_cast<float> (modRateHz / sampleRate);

    // Smoothing: ~50 ms for time, ~5 ms for other params
    timeSmoothCoeff  = std::exp (-1.0f / static_cast<float> (sampleRate * 0.05));
    paramSmoothCoeff = std::exp (-1.0f / static_cast<float> (sampleRate * 0.005));

    smoothedTimeMs   = targetTimeMs;
    smoothedFeedback = feedback;
    smoothedMix      = mixLevel;
    smoothedWidth    = width;
    initialised      = false;

    // DC blocker at ~5 Hz
    dcBlockCoeff = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f / static_cast<float> (sampleRate));
    dcBlockL_x1 = dcBlockL_y1 = 0.0f;
    dcBlockR_x1 = dcBlockR_y1 = 0.0f;

    updateDampingCoeffs();
}

void DiffuseDelay::reset()
{
    for (auto& line : lines)
    {
        std::fill (line.buffer.begin(), line.buffer.end(), 0.0f);
        line.writePos = 0;
    }

    for (auto& ap : allpassStages)
    {
        std::fill (ap.buffer.begin(), ap.buffer.end(), 0.0f);
        ap.writePos = 0;
    }

    fdnState.fill (0.0f);
    lpState.fill (0.0f);
    hpState.fill (0.0f);
    hpPrevInput.fill (0.0f);
    dcBlockL_x1 = dcBlockL_y1 = 0.0f;
    dcBlockR_x1 = dcBlockR_y1 = 0.0f;
}

// ── setters ─────────────────────────────────────────────────

void DiffuseDelay::setTime     (float ms)     { targetTimeMs = ms; }
void DiffuseDelay::setFeedback (float fb)     { feedback = std::clamp (fb, 0.0f, 1.0f); }
void DiffuseDelay::setMix      (float v)      { mixLevel = v; }
void DiffuseDelay::setDensity  (float d)      { density = std::clamp (d, 0.0f, 1.0f); }
void DiffuseDelay::setWarp     (float w)      { warp = std::clamp (w, 0.0f, 1.0f); }
void DiffuseDelay::setModDepth (float d)      { modDepth = std::clamp (d, 0.0f, 1.0f); }
void DiffuseDelay::setModRate  (float hz)     { modRateHz = std::clamp (hz, 0.01f, 10.0f); }
void DiffuseDelay::setSyncMode (SyncMode m)   { syncMode = m; }
void DiffuseDelay::setTempoBpm (float bpm)    { tempoBpm = std::max (bpm, 20.0f); }
void DiffuseDelay::setWidth    (float w)      { width = std::clamp (w, 0.0f, 1.0f); }

void DiffuseDelay::setDampLow (float freqHz)
{
    dampLowFreq = std::clamp (freqHz, 20.0f, 2000.0f);
    updateDampingCoeffs();
}

void DiffuseDelay::setDampHigh (float freqHz)
{
    dampHighFreq = std::clamp (freqHz, 1000.0f, 20000.0f);
    updateDampingCoeffs();
}

void DiffuseDelay::updateDampingCoeffs()
{
    // One-pole LP:  y[n] = (1-a)*x + a*y   =>  a = exp(-2π fc/fs)
    const float fcHigh = std::clamp (dampHighFreq, 200.0f, static_cast<float> (currentSampleRate * 0.49));
    lpCoeff = std::exp (-juce::MathConstants<float>::twoPi * fcHigh / static_cast<float> (currentSampleRate));

    // One-pole HP coefficient:  R = 1 - 2π fc/fs
    const float fcLow = std::clamp (dampLowFreq, 5.0f, static_cast<float> (currentSampleRate * 0.1));
    hpCoeff = 1.0f - (juce::MathConstants<float>::twoPi * fcLow / static_cast<float> (currentSampleRate));
}

float DiffuseDelay::syncModeToMs (SyncMode mode, float bpm) const
{
    if (bpm <= 0.0f) return 375.0f;
    const float beatMs = 60000.0f / bpm;

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

// ── process ─────────────────────────────────────────────────

void DiffuseDelay::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    // Resolve actual delay time
    float actualTimeMs = targetTimeMs;
    if (syncMode != SyncMode::Off)
    {
        actualTimeMs = syncModeToMs (syncMode, tempoBpm);
    }

    // Snap smoothed time on first use (don't ramp from zero)
    if (!initialised)
    {
        smoothedTimeMs   = actualTimeMs;
        smoothedFeedback = feedback;
        smoothedMix      = mixLevel;
        smoothedWidth    = width;
        initialised      = true;
    }

    // LFO phase increment (updated once per block)
    modPhaseInc = static_cast<float> (modRateHz / currentSampleRate);

    // Allpass diffusion coefficient (density-scaled)
    const float apCoeff = kAllpassBaseCoeff * density;

    // Warp function: bends delay line ratios.
    // warp=0 → standard ratios, warp=1 → compressed toward unison
    auto warpRatio = [this] (float baseRatio) -> float
    {
        // Exponential interpolation between base ratio and 1.0
        return baseRatio + warp * (1.0f - baseRatio);
    };

    float* dataL = buffer.getWritePointer (0);
    float* dataR = buffer.getWritePointer (1);

    const float tSmooth = timeSmoothCoeff;
    const float pSmooth = paramSmoothCoeff;

    for (int i = 0; i < numSamples; ++i)
    {
        // ── Smooth parameters ──────────────────────────────
        smoothedTimeMs   = tSmooth * smoothedTimeMs   + (1.0f - tSmooth) * actualTimeMs;
        smoothedFeedback = pSmooth * smoothedFeedback + (1.0f - pSmooth) * feedback;
        smoothedMix      = pSmooth * smoothedMix      + (1.0f - pSmooth) * mixLevel;
        smoothedWidth    = pSmooth * smoothedWidth    + (1.0f - pSmooth) * width;

        const float dryL = dataL[i];
        const float dryR = dataR[i];

        // ── Pre-diffusion allpass chains ────────────────────
        // Run the input through allpass stages to smear transients.
        // Left channel: allpassStages[0], [1]
        // Right channel: allpassStages[2], [3]
        auto processAllpass = [&] (AllpassStage& ap, float input, float coeff, float delaySamples) -> float
        {
            const int apLen = ap.length;
            if (apLen <= 1) return input;

            // Interpolated read from allpass delay line
            const float clampedDelay = std::clamp (delaySamples, 1.0f, static_cast<float> (apLen - 2));
            float readPosF = static_cast<float> (ap.writePos) - clampedDelay;
            if (readPosF < 0.0f)
                readPosF += static_cast<float> (apLen);

            const int idx0 = static_cast<int> (readPosF) % apLen;
            const int idx1 = (idx0 + 1) % apLen;
            const float frac = readPosF - std::floor (readPosF);
            const float delayed = ap.buffer[static_cast<size_t> (idx0)] * (1.0f - frac)
                                + ap.buffer[static_cast<size_t> (idx1)] * frac;

            const float feedfwd = input + coeff * delayed;
            const float output  = delayed - coeff * feedfwd;

            ap.buffer[static_cast<size_t> (ap.writePos)] = feedfwd;
            ap.writePos = (ap.writePos + 1) % apLen;

            return output;
        };

        float diffL = dryL;
        float diffR = dryR;

        if (density > 0.001f)
        {
            // Fixed allpass lengths based on sample rate (not delay time)
            // — avoids clicks from dynamically changing allpass read positions
            const float apBase = static_cast<float> (currentSampleRate) * 0.05f; // 50ms base
            diffL = processAllpass (allpassStages[0], diffL, apCoeff, apBase * kAllpassRatios[0]);
            diffL = processAllpass (allpassStages[1], diffL, apCoeff, apBase * kAllpassRatios[1]);
            diffR = processAllpass (allpassStages[2], diffR, apCoeff, apBase * kAllpassRatios[2]);
            diffR = processAllpass (allpassStages[3], diffR, apCoeff, apBase * kAllpassRatios[3]);
        }

        // ── Write to FDN delay lines (input + feedback) ────
        // Distribute stereo input across 4 lines:
        //   Line 0,1 ← Left channel, Line 2,3 ← Right channel
        const float inputGain = 0.5f; // -6 dB per line to prevent summing overshoot
        std::array<float, kNumLines> lineInputs = {
            diffL * inputGain + fdnState[0],
            diffL * inputGain + fdnState[1],
            diffR * inputGain + fdnState[2],
            diffR * inputGain + fdnState[3]
        };

        for (int ln = 0; ln < kNumLines; ++ln)
        {
            auto& line = lines[static_cast<size_t> (ln)];
            line.buffer[static_cast<size_t> (line.writePos)] = lineInputs[static_cast<size_t> (ln)];
            line.writePos = (line.writePos + 1) % line.length;
        }

        // ── Read from FDN delay lines (modulated) ──────────
        std::array<float, kNumLines> lineOutputs {};

        for (int ln = 0; ln < kNumLines; ++ln)
        {
            auto& line = lines[static_cast<size_t> (ln)];
            const size_t lnIdx = static_cast<size_t> (ln);

            // Base delay in samples with warp applied
            const float baseDelaySamples = smoothedTimeMs * 0.001f
                * static_cast<float> (currentSampleRate) * warpRatio (kLineRatios[lnIdx]);
            const float maxSamp = static_cast<float> (line.length - 2);

            // LFO modulation: ±modDepth * 5ms worth of samples
            const float modExcursion = modDepth * static_cast<float> (currentSampleRate) * 0.005f;
            const float lfoVal = std::sin (static_cast<float> (modPhase[lnIdx])
                * juce::MathConstants<float>::twoPi);
            const float modOffset = lfoVal * modExcursion;

            const float delaySamples = std::clamp (baseDelaySamples + modOffset, 1.0f, maxSamp);

            // Linear-interpolated read
            float readPosF = static_cast<float> (line.writePos) - delaySamples;
            if (readPosF < 0.0f)
                readPosF += static_cast<float> (line.length);

            const int idx0 = static_cast<int> (readPosF) % line.length;
            const int idx1 = (idx0 + 1) % line.length;
            const float frac = readPosF - std::floor (readPosF);

            lineOutputs[lnIdx] = line.buffer[static_cast<size_t> (idx0)] * (1.0f - frac)
                               + line.buffer[static_cast<size_t> (idx1)] * frac;

            // Advance mod LFO phase
            modPhase[lnIdx] += static_cast<double> (modPhaseInc);
            if (modPhase[lnIdx] >= 1.0)
                modPhase[lnIdx] -= 1.0;
        }

        // ── Hadamard-4 feedback matrix ─────────────────────
        // Orthogonal mixing: distributes energy equally across all lines,
        // preventing resonant buildup at any single delay frequency.
        //
        //   H4 = (1/2) * [ +1 +1 +1 +1 ]
        //                 [ +1 -1 +1 -1 ]
        //                 [ +1 +1 -1 -1 ]
        //                 [ +1 -1 -1 +1 ]
        const float h = 0.5f;
        const float a = lineOutputs[0], b = lineOutputs[1];
        const float c = lineOutputs[2], d = lineOutputs[3];

        std::array<float, kNumLines> mixed = {
            h * (a + b + c + d),
            h * (a - b + c - d),
            h * (a + b - c - d),
            h * (a - b - c + d)
        };

        // ── Feedback path: damping + saturation ────────────
        for (int ln = 0; ln < kNumLines; ++ln)
        {
            const size_t lnIdx = static_cast<size_t> (ln);
            float sig = mixed[lnIdx] * smoothedFeedback;

            // Denormal snap
            if (std::abs (sig) < 1.0e-15f) sig = 0.0f;

            // One-pole lowpass (high-frequency damping in feedback loop)
            lpState[lnIdx] = sig + lpCoeff * (lpState[lnIdx] - sig);
            if (std::abs (lpState[lnIdx]) < 1.0e-15f) lpState[lnIdx] = 0.0f;

            // One-pole highpass (low-frequency / DC damping in feedback loop)
            // y[n] = R * (y[n-1] + x[n] - x[n-1])
            float lpOut = lpState[lnIdx];
            float hpOut = hpCoeff * (hpState[lnIdx] + lpOut - hpPrevInput[lnIdx]);
            if (std::abs (hpOut) < 1.0e-15f) hpOut = 0.0f;
            hpPrevInput[lnIdx] = lpOut;
            hpState[lnIdx] = hpOut;

            sig = hpOut;

            // Soft saturation (cubic clipper at ±1)
            if (sig > 1.0f)       sig = 0.666f;
            else if (sig < -1.0f) sig = -0.666f;
            else                  sig = sig - (sig * sig * sig) / 3.0f;

            fdnState[lnIdx] = sig;
        }

        // ── Sum FDN outputs to stereo ──────────────────────
        // Lines 0,1 → Left, Lines 2,3 → Right (with width crossfeed)
        const float rawL = lineOutputs[0] + lineOutputs[1];
        const float rawR = lineOutputs[2] + lineOutputs[3];

        // Width: 0 = mono, 1 = full stereo
        const float mid  = (rawL + rawR) * 0.5f;
        const float side = (rawL - rawR) * 0.5f;
        float outL = mid + side * smoothedWidth;
        float outR = mid - side * smoothedWidth;

        // DC blocker on output: y[n] = x[n] - x[n-1] + R * y[n-1]
        float dcOutL = outL - dcBlockL_x1 + dcBlockCoeff * dcBlockL_y1;
        float dcOutR = outR - dcBlockR_x1 + dcBlockCoeff * dcBlockR_y1;
        if (std::abs (dcOutL) < 1.0e-15f) dcOutL = 0.0f;
        if (std::abs (dcOutR) < 1.0e-15f) dcOutR = 0.0f;
        dcBlockL_x1 = outL;  dcBlockL_y1 = dcOutL;
        dcBlockR_x1 = outR;  dcBlockR_y1 = dcOutR;
        outL = dcOutL;
        outR = dcOutR;

        // Output: wet only (send/return topology)
        dataL[i] = outL * smoothedMix;
        dataR[i] = outR * smoothedMix;
    }
}

} // namespace axelf::dsp
