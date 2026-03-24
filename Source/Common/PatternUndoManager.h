#pragma once

#include "Pattern.h"
#include <vector>

namespace axelf
{

class PatternUndoManager
{
public:
    static constexpr int kMaxUndoDepth = 32;

    // Capture current state before a mutation
    void pushSynth(int moduleIndex, const SynthPattern& pat)
    {
        auto& stack = synthUndo[static_cast<size_t>(moduleIndex)];
        stack.erase(stack.begin() + synthPos[static_cast<size_t>(moduleIndex)], stack.end());
        stack.push_back(pat);
        if (static_cast<int>(stack.size()) > kMaxUndoDepth)
            stack.erase(stack.begin());
        synthPos[static_cast<size_t>(moduleIndex)] = static_cast<int>(stack.size());
    }

    void pushDrum(const DrumPattern& pat)
    {
        drumStack.erase(drumStack.begin() + drumPos, drumStack.end());
        drumStack.push_back(pat);
        if (static_cast<int>(drumStack.size()) > kMaxUndoDepth)
            drumStack.erase(drumStack.begin());
        drumPos = static_cast<int>(drumStack.size());
    }

    // Undo: restore previous state, return true if applied
    bool undoSynth(int moduleIndex, SynthPattern& pat)
    {
        auto& stack = synthUndo[static_cast<size_t>(moduleIndex)];
        auto& pos = synthPos[static_cast<size_t>(moduleIndex)];
        if (pos <= 0 || stack.empty())
            return false;

        // Save current state for redo before overwriting
        if (pos == static_cast<int>(stack.size()))
            pushSynthRedo(moduleIndex, pat);

        --pos;
        pat = stack[static_cast<size_t>(pos)];
        return true;
    }

    bool undoDrum(DrumPattern& pat)
    {
        if (drumPos <= 0 || drumStack.empty())
            return false;

        if (drumPos == static_cast<int>(drumStack.size()))
            pushDrumRedo(pat);

        --drumPos;
        pat = drumStack[static_cast<size_t>(drumPos)];
        return true;
    }

    // Redo: restore next state
    bool redoSynth(int moduleIndex, SynthPattern& pat)
    {
        auto& stack = synthUndo[static_cast<size_t>(moduleIndex)];
        auto& pos = synthPos[static_cast<size_t>(moduleIndex)];
        if (pos >= static_cast<int>(stack.size()) - 1)
        {
            // Check redo stack
            auto& rStack = synthRedo[static_cast<size_t>(moduleIndex)];
            if (!rStack.empty())
            {
                pat = rStack.back();
                rStack.pop_back();
                return true;
            }
            return false;
        }
        ++pos;
        pat = stack[static_cast<size_t>(pos)];
        return true;
    }

    bool redoDrum(DrumPattern& pat)
    {
        if (drumPos >= static_cast<int>(drumStack.size()) - 1)
        {
            if (!drumRedo.empty())
            {
                pat = drumRedo.back();
                drumRedo.pop_back();
                return true;
            }
            return false;
        }
        ++drumPos;
        pat = drumStack[static_cast<size_t>(drumPos)];
        return true;
    }

    bool canUndo(int moduleIndex) const
    {
        if (moduleIndex == 4) // LinnDrum
            return drumPos > 0;
        return synthPos[static_cast<size_t>(moduleIndex)] > 0;
    }

    bool canRedo(int moduleIndex) const
    {
        if (moduleIndex == 4)
            return drumPos < static_cast<int>(drumStack.size()) - 1 || !drumRedo.empty();
        return synthPos[static_cast<size_t>(moduleIndex)] < static_cast<int>(synthUndo[static_cast<size_t>(moduleIndex)].size()) - 1
            || !synthRedo[static_cast<size_t>(moduleIndex)].empty();
    }

private:
    std::vector<SynthPattern> synthUndo[4];
    int synthPos[4] = {};
    std::vector<SynthPattern> synthRedo[4];

    std::vector<DrumPattern> drumStack;
    int drumPos = 0;
    std::vector<DrumPattern> drumRedo;

    void pushSynthRedo(int moduleIndex, const SynthPattern& pat)
    {
        synthRedo[static_cast<size_t>(moduleIndex)].push_back(pat);
    }

    void pushDrumRedo(const DrumPattern& pat)
    {
        drumRedo.push_back(pat);
    }
};

} // namespace axelf
