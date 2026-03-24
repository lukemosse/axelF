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

    int idx0 = static_cast<int>(readPos);
    
    // Safety guard against NaN or wild values
    if (idx0 < 0 || idx0 >= kMaxDelaySamples) idx0 = 0;
    
    int idx1 = (idx0 + 1) % kMaxDelaySamples;
    float frac = readPos - static_cast<float>(idx0);

    // Final bounds check before accessing vectors
    idx0 = std::clamp(idx0, 0, kMaxDelaySamples - 1);
    idx1 = std::clamp(idx1, 0, kMaxDelaySamples - 1);

    if (buffer.size() <= static_cast<size_t>(idx0) || buffer.size() <= static_cast<size_t>(idx1))
        return 0.0f; // Absolute safety against out-of-bounds

    return buffer[static_cast<size_t>(idx0)] * (1.0f - frac)
         + buffer[static_cast<size_t>(idx1)] * frac;
}

} // namespace axelf::jx3p
