#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf::linndrum
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Drum names used for parameter IDs
inline const juce::StringArray& getDrumNames()
{
    static const juce::StringArray names {
        "kick", "snare", "hihat_cl", "hihat_op", "ride",
        "crash", "tom_hi", "tom_mid", "tom_lo",
        "conga_hi", "conga_lo", "clap", "cowbell",
        "tambourine", "cabasa"
    };
    return names;
}

// Sample bank display names (index 0 = Synthetic fallback)
inline const juce::StringArray& getBankNames()
{
    static const juce::StringArray names {
        "Synthetic",
        "OG Clean",
        "OG Color",
        "Clean N Lean",
        "Colored Harmonix",
        "Hibrow",
        "Lowbrow",
        "Hi Hat",
        "Roger Gone Wild",
        "Tite N Lite",
        "Chromatic Congas"
    };
    return names;
}

// Folder names corresponding to each disk bank (indices 1-10)
inline const juce::StringArray& getBankFolderNames()
{
    static const juce::StringArray names {
        "OG Clean Kit - Lindrum From Mars Samples",
        "OG Color Kit - Lindrum From Mars Samples",
        "Clean N Lean Kit - Lindrum From Mars Samples",
        "Colored Harmonix Kit - Lindrum From Mars Samples",
        "Hibrow Kit - Lindrum From Mars Samples",
        "Lowbrow Kit - Lindrum From Mars Samples",
        "Hi Hat Kit - Lindrum From Mars Samples",
        "Roger Gone Wild Kit - Lindrum From Mars Samples",
        "Tite N Lite Kit - Lindrum From Mars Samples",
        "Chromatic Congas Kit - Lindrum From Mars Samples"
    };
    return names;
}

} // namespace axelf::linndrum
