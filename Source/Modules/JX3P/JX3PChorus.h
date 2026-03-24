#pragma once

#include <vector>

namespace axelf::jx3p
{

class JX3PChorus
{
public:
    void setSampleRate(double sampleRate);
    void setParameters(int mode, float rate, float depth);
    void setSpread(float spread);
    void reset();

    void processStereo(float& left, float& right);

private:
    double currentSampleRate = 44100.0;
    int chorusMode = 1;
    float rate  = 0.8f;
    float depth = 0.5f;
    float stereoSpread = 0.5f;  // 0 = mono, 1 = wide

    // Delay line
    std::vector<float> delayBufferL = std::vector<float>(kMaxDelaySamples, 0.0f);
    std::vector<float> delayBufferR = std::vector<float>(kMaxDelaySamples, 0.0f);
    int writeIndex = 0;
    double lfoPhase = 0.0;
    double lfoIncrement = 0.0;

    static constexpr int kMaxDelaySamples = 4096;

    float readInterpolated(const std::vector<float>& buffer, float delaySamples) const;
};

} // namespace axelf::jx3p
