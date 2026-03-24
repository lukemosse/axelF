#include "LinnDrumSequencer.h"

namespace axelf::linndrum
{

void LinnDrumSequencer::setTempo(float bpm)
{
    tempo = bpm;
}

void LinnDrumSequencer::setSwing(float swingPercent)
{
    swing = swingPercent;
}

void LinnDrumSequencer::reset()
{
    currentStep = 0;
    sampleCounter = 0.0;
    triggers.fill(false);
}

void LinnDrumSequencer::advance(double sampleRate)
{
    triggers.fill(false);

    if (!running)
        return;

    // Calculate samples per 16th note
    // 1 beat = 60/tempo seconds; 1 sixteenth = beat/4
    double secondsPerSixteenth = 60.0 / static_cast<double>(tempo) / 4.0;

    // Apply swing to off-beat steps (odd-numbered)
    if (currentStep % 2 == 1)
    {
        double swingRatio = static_cast<double>(swing) / 100.0;
        secondsPerSixteenth *= (2.0 * swingRatio);
    }
    else if (currentStep % 2 == 0 && currentStep > 0)
    {
        double swingRatio = static_cast<double>(swing) / 100.0;
        secondsPerSixteenth *= (2.0 * (1.0 - swingRatio));
    }

    samplesPerStep = secondsPerSixteenth * sampleRate;

    sampleCounter += 1.0;
    if (sampleCounter >= samplesPerStep)
    {
        sampleCounter -= samplesPerStep;
        currentStep = (currentStep + 1) % kNumSteps;

        // Fire triggers for active steps
        for (int track = 0; track < kNumTracks; ++track)
        {
            if (pattern[static_cast<size_t>(track)][static_cast<size_t>(currentStep)])
                triggers[static_cast<size_t>(track)] = true;
        }
    }
}

void LinnDrumSequencer::setStep(int trackIndex, int stepIndex, bool active)
{
    if (trackIndex >= 0 && trackIndex < kNumTracks &&
        stepIndex >= 0 && stepIndex < kNumSteps)
    {
        pattern[static_cast<size_t>(trackIndex)][static_cast<size_t>(stepIndex)] = active;
    }
}

bool LinnDrumSequencer::getStep(int trackIndex, int stepIndex) const
{
    if (trackIndex >= 0 && trackIndex < kNumTracks &&
        stepIndex >= 0 && stepIndex < kNumSteps)
    {
        return pattern[static_cast<size_t>(trackIndex)][static_cast<size_t>(stepIndex)];
    }
    return false;
}

bool LinnDrumSequencer::hasTrigger(int trackIndex) const
{
    if (trackIndex >= 0 && trackIndex < kNumTracks)
        return triggers[static_cast<size_t>(trackIndex)];
    return false;
}

} // namespace axelf::linndrum
