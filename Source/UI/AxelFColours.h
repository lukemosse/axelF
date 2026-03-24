#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace axelf::ui
{

namespace Colours
{
    // Base backgrounds
    inline constexpr juce::uint32 bgDark    = 0xFF1a1a2e;
    inline constexpr juce::uint32 bgPanel   = 0xFF232340;
    inline constexpr juce::uint32 bgSection = 0xFF2d2d4a;
    inline constexpr juce::uint32 bgControl = 0xFF3a3a5c;

    // Accents
    inline constexpr juce::uint32 accentGold   = 0xFFd4a843;
    inline constexpr juce::uint32 accentBlue   = 0xFF5b8bd4;
    inline constexpr juce::uint32 accentRed    = 0xFFd45b5b;
    inline constexpr juce::uint32 accentGreen  = 0xFF5bd47a;
    inline constexpr juce::uint32 accentOrange = 0xFFd4935b;

    // Text
    inline constexpr juce::uint32 textPrimary   = 0xFFe8e8f0;
    inline constexpr juce::uint32 textSecondary = 0xFF9898b0;
    inline constexpr juce::uint32 textLabel     = 0xFFb0b0cc;

    // Border
    inline constexpr juce::uint32 borderSubtle = 0xFF404060;
    inline constexpr juce::uint32 knobTrack    = 0xFF555578;

    // Module accents
    inline constexpr juce::uint32 jupiter = 0xFFcc4444;
    inline constexpr juce::uint32 moog    = 0xFF8b6914;
    inline constexpr juce::uint32 jx3p    = 0xFF2b7a9e;
    inline constexpr juce::uint32 dx7     = 0xFF6b8e23;
    inline constexpr juce::uint32 linn    = 0xFF8b4513;
    inline constexpr juce::uint32 mixer   = 0xFF6a5acd;
}

} // namespace axelf::ui
