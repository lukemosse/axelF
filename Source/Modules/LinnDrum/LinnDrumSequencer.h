#pragma once

#include <array>

namespace axelf::linndrum
{

class LinnDrumSequencer
{
public:
    static constexpr int kNumSteps = 16;
    static constexpr int kNumTracks = 15;

    void setTempo(float bpm);
    void setSwing(float swingPercent);  // 50 = straight, >50 = swung
    void reset();

    // Call per sample
    void advance(double sampleRate);

    // Pattern editing
    void setStep(int trackIndex, int stepIndex, bool active);
    bool getStep(int trackIndex, int stepIndex) const;

    // Check if a trigger happened this sample
    bool hasTrigger(int trackIndex) const;
    int getCurrentStep() const { return currentStep; }
    bool isRunning() const { return running; }

    void start() { running = true; }
    void stop() { running = false; currentStep = 0; sampleCounter = 0.0; }

private:
    float tempo = 120.0f;
    float swing = 50.0f;
    int currentStep = 0;
    double sampleCounter = 0.0;
    double samplesPerStep = 0.0;
    bool running = false;

    // Pattern: [track][step]
    std::array<std::array<bool, kNumSteps>, kNumTracks> pattern = {};

    // Trigger flags (reset each sample)
    std::array<bool, kNumTracks> triggers = {};
};

} // namespace axelf::linndrum
