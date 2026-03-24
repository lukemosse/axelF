#pragma once

#include <array>

namespace axelf::dx7
{

// Represents the modulation topology for one DX7 algorithm.
// For each operator (0-5), stores which operators modulate it,
// and which operators are carriers (output audio).
struct AlgorithmTopology
{
    // modulators[i] is a bitmask of which ops modulate op i (bit 0 = op1, etc.)
    std::array<uint8_t, 6> modulators = {};
    // carrierMask: bit set if op is a carrier
    uint8_t carrierMask = 0;
    // Which op has feedback (0-5), -1 = none
    int feedbackOp = 5;
};

class DX7Algorithm
{
public:
    void setAlgorithm(int algorithmNumber);
    int getAlgorithm() const { return currentAlgorithm; }

    bool isCarrier(int op) const;
    const AlgorithmTopology& getTopology() const { return topology; }
    int getFeedbackOperator() const { return topology.feedbackOp; }

private:
    int currentAlgorithm = 1;
    AlgorithmTopology topology;

    void updateTopology();
};

} // namespace axelf::dx7
