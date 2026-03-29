#include "JX3PChorus.h"
#include <cmath>
#include <algorithm>

namespace axelf::jx3p
{

void JX3PChorus::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    delayBufferL.resize(kMaxDelaySamples, 0.0f);
    delayBufferR.resize(kMaxDelaySamples, 0.0f);
    lfoIncrement = static_cast<double>(rate) / sampleRate;
}

void JX3PChorus::setParameters(int mode, float r, float d)
{
    chorusMode = mode;
    rate = r;
    depth = d;
    lfoIncrement = static_cast<double>(rate) / currentSampleRate;
}

void JX3PChorus::setSpread(float spread)
{
    stereoSpread = std::clamp(spread, 0.0f, 1.0f);
}

void JX3PChorus::reset()
{
    std::fill(delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill(delayBufferR.begin(), delayBufferR.end(), 0.0f);
    writeIndex = 0;
    lfoPhase = 0.0;
}

void JX3PChorus::processStereo(float& left, float& right)
{
    if (chorusMode == 0 || delayBufferL.empty() || delayBufferR.empty())
        return;

    if (writeIndex < 0 || writeIndex >= kMaxDelaySamples) 
        writeIndex = 0; // Guard against corrupted state
        
    if (delayBufferL.size() <= static_cast<size_t>(writeIndex) || delayBufferR.size() <= static_cast<size_t>(writeIndex))
        return; // Absolute safety against out-of-bounds

    // Write into delay line
    delayBufferL[static_cast<size_t>(writeIndex)] = left;
    delayBufferR[static_cast<size_t>(writeIndex)] = right;

    // LFO modulates delay time
    float lfoVal = static_cast<float>(std::sin(lfoPhase * 2.0 * 3.14159265358979323846));
    float baseDelay = (chorusMode == 1) ? 7.0f : 12.0f;  // ms
    float delayMs = baseDelay + depth * 3.0f * lfoVal;
    float delaySamples = delayMs * 0.001f * static_cast<float>(currentSampleRate);
    delaySamples = std::clamp(delaySamples, 1.0f, static_cast<float>(kMaxDelaySamples - 1));

    float wetL = readInterpolated(delayBufferL, delaySamples);
    // Right channel uses slightly offset delay for stereo width
    float spreadOffset = stereoSpread * 2.0f;  // up to 2ms offset
    float delaySamplesR = (baseDelay + spreadOffset) + depth * 3.0f * (-lfoVal);
    delaySamplesR = delaySamplesR * 0.001f * static_cast<float>(currentSampleRate);
    delaySamplesR = std::clamp(delaySamplesR, 1.0f, static_cast<float>(kMaxDelaySamples - 1));
    float wetR = readInterpolated(delayBufferR, delaySamplesR);

    // Mix
    float wetAmount = (chorusMode == 1) ? 0.5f : 0.7f;
    left  = left  * (1.0f - wetAmount) + wetL * wetAmount;
    right = right * (1.0f - wetAmount) + wetR * wetAmount;

    // Advance write position and LFO
    writeIndex = (writeIndex + 1) % kMaxDelaySamples;
    lfoPhase += lfoIncrement;
    if (lfoPhase >= 1.0)
        lfoPhase -= 1.0;
}

float JX3PChorus::readInterpolated(const std::vector<float>& buffer, float delaySamples) const
{
    float readPos = static_cast<float>(writeIndex) - delaySamples;
    if (readPos < 0.0f)
        readPos += static_cast<float>(kMaxDelaySamples);

    int idx1 = static_cast<int>(readPos);
    
    // Safety guard against NaN or wild values
    if (idx1 < 0 || idx1 >= kMaxDelaySamples) idx1 = 0;

    int idx0 = (idx1 - 1 + kMaxDelaySamples) % kMaxDelaySamples;
    int idx2 = (idx1 + 1) % kMaxDelaySamples;
    int idx3 = (idx1 + 2) % kMaxDelaySamples;
    float frac = readPos - static_cast<float>(idx1);

    // Bounds check
    auto sz = static_cast<int>(buffer.size());
    if (sz <= idx0 || sz <= idx1 || sz <= idx2 || sz <= idx3)
        return 0.0f;

    // Hermite (cubic) interpolation for smoother chorus modulation
    float y0 = buffer[static_cast<size_t>(idx0)];
    float y1 = buffer[static_cast<size_t>(idx1)];
    float y2 = buffer[static_cast<size_t>(idx2)];
    float y3 = buffer[static_cast<size_t>(idx3)];

    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

} // namespace axelf::jx3p
