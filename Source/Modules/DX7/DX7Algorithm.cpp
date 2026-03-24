#include "DX7Algorithm.h"

namespace axelf::dx7
{

void DX7Algorithm::setAlgorithm(int algorithmNumber)
{
    currentAlgorithm = algorithmNumber;
    updateTopology();
}

bool DX7Algorithm::isCarrier(int op) const
{
    if (op >= 0 && op < 6)
        return (topology.carrierMask & (1 << op)) != 0;
    return false;
}

// DX7 algorithm table.  Operators numbered 1-6 in DX7 docs, stored as index 0-5.
// Each algorithm defines: which ops modulate which, which are carriers, feedback op.
// Notation: Op1=idx0, Op2=idx1, Op3=idx2, Op4=idx3, Op5=idx4, Op6=idx5
void DX7Algorithm::updateTopology()
{
    topology = {};  // reset

    // Helper: set that opSrc (1-based) modulates opDst (1-based)
    auto mod = [&](int src, int dst) {
        topology.modulators[static_cast<size_t>(dst - 1)] |= static_cast<uint8_t>(1 << (src - 1));
    };
    auto carrier = [&](int op) {
        topology.carrierMask |= static_cast<uint8_t>(1 << (op - 1));
    };

    // Default feedback on op6
    topology.feedbackOp = 5;

    switch (currentAlgorithm)
    {
        case 1: // 6→5→4→3→2→1*
            mod(6,5); mod(5,4); mod(4,3); mod(3,2); mod(2,1);
            carrier(1);
            break;
        case 2: // 6→5→4→3→2→1* (fb on op2)
            mod(6,5); mod(5,4); mod(4,3); mod(3,2); mod(2,1);
            carrier(1);
            topology.feedbackOp = 1;
            break;
        case 3: // 6→5→4→3* + 2→1*  (fb on op6)
            mod(6,5); mod(5,4); mod(4,3);
            mod(2,1);
            carrier(1); carrier(3);
            break;
        case 4: // Same as 3 but fb on op4
            mod(6,5); mod(5,4); mod(4,3);
            mod(2,1);
            carrier(1); carrier(3);
            topology.feedbackOp = 3;
            break;
        case 5: // 6→5→4* + 3→2→1* (fb on op6)
            mod(6,5); mod(5,4);
            mod(3,2); mod(2,1);
            carrier(1); carrier(4);
            break;
        case 6: // 6→5→4* + 3→2→1* (fb on op5)
            mod(6,5); mod(5,4);
            mod(3,2); mod(2,1);
            carrier(1); carrier(4);
            topology.feedbackOp = 4;
            break;
        case 7: // 6→5 + 4→3→2→1* (carriers: 1)
            mod(6,5); mod(5,1); mod(4,3); mod(3,2); mod(2,1);
            carrier(1);
            break;
        case 8: // 4→3→2→1* (fb op4, 6→5→1)
            mod(4,3); mod(3,2); mod(2,1); mod(6,5); mod(5,1);
            carrier(1);
            topology.feedbackOp = 3;
            break;
        case 9: // 6→5→4→3* + 2→1* (fb on op2)
            mod(6,5); mod(5,4); mod(4,3);
            mod(2,1);
            carrier(1); carrier(3);
            topology.feedbackOp = 1;
            break;
        case 10: // 6→5→4→3* + 2→1* (fb on op3)
            mod(6,5); mod(5,4); mod(4,3);
            mod(2,1);
            carrier(1); carrier(3);
            topology.feedbackOp = 2;
            break;
        case 11: // 6→5→4→3* +  2→1*  (fb op6)
            mod(6,5); mod(5,4); mod(4,3);
            mod(2,1);
            carrier(1); carrier(3);
            break;
        case 12: // 6→5→4→3* + 2→1* (fb on op2, variant)
            mod(6,5); mod(5,4); mod(4,3);
            mod(2,1);
            carrier(1); carrier(3);
            topology.feedbackOp = 1;
            break;
        case 13: // 6→5→4* + 3→2→1*
            mod(6,5); mod(5,4);
            mod(3,2); mod(2,1);
            carrier(1); carrier(4);
            break;
        case 14: // 6→5→4* + 3→2→1*
            mod(6,5); mod(5,4);
            mod(3,2); mod(2,1);
            carrier(1); carrier(4);
            break;
        case 15: // 6→5→4* + 3→2→1* (fb on op2)
            mod(6,5); mod(5,4);
            mod(3,2); mod(2,1);
            carrier(1); carrier(4);
            topology.feedbackOp = 1;
            break;
        case 16: // 6→5→4→3→2→1* (complex variant)
            mod(6,5); mod(5,4); mod(4,3); mod(3,2); mod(2,1);
            carrier(1);
            break;
        case 17: // Same as 16 with fb op2
            mod(6,5); mod(5,4); mod(4,3); mod(3,2); mod(2,1);
            carrier(1);
            topology.feedbackOp = 1;
            break;
        case 18: // 6→5→4→3* + 2* + 1*  (3 carriers)
            mod(6,5); mod(5,4); mod(4,3);
            carrier(1); carrier(2); carrier(3);
            break;
        case 19: // 6→5*, 6→4*, 3→2→1*
            mod(6,5); mod(6,4); mod(3,2); mod(2,1);
            carrier(1); carrier(4); carrier(5);
            break;
        case 20: // 6→5*, 4→3*, 2→1* (3 pairs)
            mod(6,5); mod(4,3); mod(2,1);
            carrier(1); carrier(3); carrier(5);
            topology.feedbackOp = 2;
            break;
        case 21: // 6→5*, 6→4*, 3→2*, 3→1*
            mod(6,5); mod(6,4); mod(3,2); mod(3,1);
            carrier(1); carrier(2); carrier(4); carrier(5);
            break;
        case 22: // 6→5*, 4*, 3→2→1*
            mod(6,5); mod(3,2); mod(2,1);
            carrier(1); carrier(4); carrier(5);
            break;
        case 23: // 6→5*, 6→4*, 3*, 2→1*
            mod(6,5); mod(6,4); mod(2,1);
            carrier(1); carrier(3); carrier(4); carrier(5);
            break;
        case 24: // 6→5*, 6→4*, 6→3*, 2→1*
            mod(6,5); mod(6,4); mod(6,3); mod(2,1);
            carrier(1); carrier(3); carrier(4); carrier(5);
            break;
        case 25: // 6→5*, 6→4*, 3*, 2*, 1*
            mod(6,5); mod(6,4);
            carrier(1); carrier(2); carrier(3); carrier(4); carrier(5);
            break;
        case 26: // 6→5*, 4→3*, 2*, 1*
            mod(6,5); mod(4,3);
            carrier(1); carrier(2); carrier(3); carrier(5);
            break;
        case 27: // 6→5*, 4*, 3*, 2→1*
            mod(6,5); mod(2,1);
            carrier(1); carrier(3); carrier(4); carrier(5);
            break;
        case 28: // 6→5→4*, 3*, 2*, 1*
            mod(6,5); mod(5,4);
            carrier(1); carrier(2); carrier(3); carrier(4);
            break;
        case 29: // 6→5*, 4*, 3*, 2*, 1*
            mod(6,5);
            carrier(1); carrier(2); carrier(3); carrier(4); carrier(5);
            break;
        case 30: // 6→5→4*, 3*, 2*, 1*
            mod(6,5); mod(5,4);
            carrier(1); carrier(2); carrier(3); carrier(4);
            break;
        case 31: // 6*, 5*, 4*, 3*, 2*, 1* (all carriers, pure additive except fb)
            carrier(1); carrier(2); carrier(3); carrier(4); carrier(5); carrier(6);
            break;
        case 32: // 6*, 5*, 4*, 3*, 2*, 1* (all carriers)
            carrier(1); carrier(2); carrier(3); carrier(4); carrier(5); carrier(6);
            break;
        default: // Default to algorithm 1
            mod(6,5); mod(5,4); mod(4,3); mod(3,2); mod(2,1);
            carrier(1);
            break;
    }
}

} // namespace axelf::dx7
