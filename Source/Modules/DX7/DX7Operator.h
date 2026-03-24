#pragma once

namespace axelf::dx7
{

class DX7Operator
{
public:
    void setSampleRate(double sampleRate);
    void setFrequency(float hz);
    void setRatio(float ratio);
    void setLevel(float level);    // 0-99
    void setDetune(float detune);  // -7 to +7
    void reset();

    // Process a single sample with phase modulation input
    float processSample(float phaseModInput);

    void noteOn();
    void noteOff();
    bool isActive() const;

    // Set EG rates and levels (R1-R4, L1-L4)
    void setEGParameters(float r1, float r2, float r3, float r4,
                         float l1, float l2, float l3, float l4);
    void setVelocitySensitivity(float vs);  // 0-7
    void setVelocity(float vel);             // 0-1

private:
    double currentSampleRate = 44100.0;
    float baseFrequency = 440.0f;
    float ratio = 1.0f;
    float outputLevel = 1.0f;
    float detune = 0.0f;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float velocitySensitivity = 0.0f;  // 0-7
    float velocityScale = 1.0f;        // computed from sensitivity + velocity

    // DX7-style 4-rate/4-level envelope generator
    enum class EGState { Idle, Rate1, Rate2, Rate3, Rate4 };
    EGState egState = EGState::Idle;
    float egOutput = 0.0f;
    float egRates[4] = { 50.0f, 50.0f, 50.0f, 50.0f };
    float egLevels[4] = { 99.0f, 99.0f, 99.0f, 0.0f };

    void updatePhaseIncrement();
    float advanceEG();
};

} // namespace axelf::dx7
