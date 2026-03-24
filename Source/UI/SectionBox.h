#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AxelFColours.h"

namespace axelf::ui
{

class SectionBox : public juce::Component
{
public:
    SectionBox(const juce::String& title, juce::Colour accent)
        : titleText(title), accentColour(accent) {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Section background with 1px border
        g.setColour(juce::Colour(Colours::bgSection));
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(juce::Colour(Colours::borderSubtle));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

        // Title area with bottom separator
        auto titleArea = bounds.withHeight(28.0f).reduced(12.0f, 0.0f);
        g.setColour(accentColour);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(titleText.toUpperCase(), titleArea.toNearestInt(),
                   juce::Justification::centredLeft, false);

        // Title underline
        g.setColour(juce::Colour(Colours::borderSubtle));
        g.fillRect(bounds.getX() + 12.0f, 27.0f, bounds.getWidth() - 24.0f, 1.0f);
    }

    juce::Rectangle<int> getContentArea() const
    {
        return getLocalBounds().withTrimmedTop(30).reduced(8, 4);
    }

    void setTitle(const juce::String& t) { titleText = t; repaint(); }

private:
    juce::String titleText;
    juce::Colour accentColour;
};

} // namespace axelf::ui
