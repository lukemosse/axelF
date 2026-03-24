#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AxelFColours.h"

namespace axelf::ui
{

class AboutOverlay : public juce::Component
{
public:
    AboutOverlay()
    {
        setInterceptsMouseClicks(true, false);
    }

    void paint(juce::Graphics& g) override
    {
        // Semi-transparent backdrop
        g.fillAll(juce::Colour(0xCC000000));

        auto area = getLocalBounds().toFloat();
        auto box = area.withSizeKeepingCentre(360.0f, 280.0f);

        // Card background
        g.setColour(juce::Colour(0xFF1a1a30));
        g.fillRoundedRectangle(box, 8.0f);
        g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.5f));
        g.drawRoundedRectangle(box, 8.0f, 1.5f);

        auto inner = box.reduced(24.0f);

        // Title
        g.setColour(juce::Colour(Colours::accentGold));
        g.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
        g.drawText("AXEL F", inner.removeFromTop(36.0f), juce::Justification::centred);

        // Subtitle
        g.setColour(juce::Colour(Colours::textSecondary));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("Synth Workstation", inner.removeFromTop(20.0f), juce::Justification::centred);

        inner.removeFromTop(16.0f);

        // Version
        g.setColour(juce::Colour(Colours::textPrimary));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText("Version 1.0.0", inner.removeFromTop(20.0f), juce::Justification::centred);

        inner.removeFromTop(8.0f);

        // Description
        g.setColour(juce::Colour(Colours::textSecondary));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawFittedText(
            "Faithful recreation of the five instruments\n"
            "used to record \"Axel F\" (Harold Faltermeyer, 1984).\n\n"
            "Jupiter-8  \u2022  Moog Modular 15  \u2022  JX-3P\n"
            "DX7  \u2022  LinnDrum",
            inner.removeFromTop(80.0f).toNearestInt(),
            juce::Justification::centred, 5);

        inner.removeFromTop(12.0f);

        // Built with JUCE
        g.setColour(juce::Colour(Colours::textSecondary).withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("Built with JUCE", inner.removeFromTop(16.0f), juce::Justification::centred);

        // Dismiss hint
        g.drawText("Click anywhere to close", inner.removeFromBottom(16.0f), juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        setVisible(false);
    }
};

} // namespace axelf::ui
