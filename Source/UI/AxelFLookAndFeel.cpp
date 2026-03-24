#include "AxelFLookAndFeel.h"

namespace axelf::ui
{

AxelFLookAndFeel::AxelFLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(Colours::bgDark));
    setColour(juce::Label::textColourId, juce::Colour(Colours::textLabel));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(Colours::bgControl));
    setColour(juce::ComboBox::textColourId, juce::Colour(Colours::textPrimary));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(Colours::borderSubtle));
    setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
    setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
    setColour(juce::TextButton::textColourOnId, juce::Colour(Colours::textPrimary));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(Colours::accentBlue));
    setColour(juce::Slider::thumbColourId, juce::Colour(Colours::textPrimary));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(Colours::textSecondary));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(Colours::bgPanel));
    setColour(juce::PopupMenu::textColourId, juce::Colour(Colours::textPrimary));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(Colours::accentBlue));
    setColour(juce::TabbedComponent::backgroundColourId, juce::Colour(0xFF16162a));
    setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colour(0x00000000));
    setColour(juce::TabbedButtonBar::frontOutlineColourId, juce::Colour(0x00000000));
}

void AxelFLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto arcRadius = radius - 2.0f;

    // Background arc (dim track)
    {
        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(Colours::knobTrack).withAlpha(0.5f));
        g.strokePath(bgArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // Value arc (accent colour)
    if (sliderPos > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, angle, true);
        auto arcColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
        if (arcColour.isTransparent())
            arcColour = juce::Colour(Colours::accentBlue);
        g.setColour(arcColour);
        g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }

    // Knob body
    auto knobRadius = radius - 8.0f;
    auto knobBounds = juce::Rectangle<float>(centreX - knobRadius, centreY - knobRadius,
                                              knobRadius * 2.0f, knobRadius * 2.0f);

    // Drop shadow
    g.setColour(juce::Colour(0x40000000));
    g.fillEllipse(knobBounds.translated(1.0f, 2.0f));

    // Knob body — radial gradient
    juce::ColourGradient bodyGrad(juce::Colour(0xFF5a5a80), centreX - knobRadius * 0.3f, centreY - knobRadius * 0.3f,
                                   juce::Colour(0xFF2a2a48), centreX + knobRadius * 0.5f, centreY + knobRadius * 0.5f, true);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(knobBounds);

    // Inner highlight (subtle shine)
    g.setColour(juce::Colour(0x10FFFFFF));
    g.fillEllipse(centreX - knobRadius * 0.6f, centreY - knobRadius * 0.7f,
                  knobRadius * 0.8f, knobRadius * 0.6f);

    // Knob border
    g.setColour(juce::Colour(0xFF666688));
    g.drawEllipse(knobBounds, 1.5f);

    // Indicator pointer
    juce::Path indicator;
    auto lineLength = knobRadius * 0.65f;
    indicator.addRoundedRectangle(-1.5f, -lineLength, 3.0f, lineLength, 1.5f);
    g.setColour(juce::Colour(Colours::textPrimary));
    g.fillPath(indicator, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
}

void AxelFLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                         juce::Slider::SliderStyle style, juce::Slider&)
{
    if (style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical)
    {
        auto trackX = (float)x + (float)width * 0.5f;
        auto trackW = 6.0f;

        // Track
        g.setColour(juce::Colour(Colours::knobTrack));
        g.fillRoundedRectangle(trackX - trackW * 0.5f, (float)y, trackW, (float)height, 3.0f);
        g.setColour(juce::Colour(0xFF444466));
        g.drawRoundedRectangle(trackX - trackW * 0.5f, (float)y, trackW, (float)height, 3.0f, 1.0f);

        // Thumb
        auto thumbW = 20.0f;
        auto thumbH = 8.0f;
        juce::ColourGradient thumbGrad(juce::Colour(0xFFaaaaaa), 0, sliderPos - thumbH * 0.5f,
                                        juce::Colour(0xFF777777), 0, sliderPos + thumbH * 0.5f, false);
        g.setGradientFill(thumbGrad);
        g.fillRoundedRectangle(trackX - thumbW * 0.5f, sliderPos - thumbH * 0.5f, thumbW, thumbH, 2.0f);
    }
    else
    {
        // Horizontal fallback
        auto trackY = (float)y + (float)height * 0.5f;
        g.setColour(juce::Colour(Colours::knobTrack));
        g.fillRoundedRectangle((float)x, trackY - 3.0f, (float)width, 6.0f, 3.0f);

        g.setColour(juce::Colour(Colours::textPrimary));
        g.fillEllipse(sliderPos - 6.0f, trackY - 6.0f, 12.0f, 12.0f);
    }
}

void AxelFLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                     int /*btnX*/, int /*btnY*/, int /*btnW*/, int /*btnH*/,
                                     juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);

    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    // Chevron arrow (thin V-stroke)
    auto arrowX = (float)width - 16.0f;
    auto arrowY = (float)height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath(arrowX - 4.0f, arrowY - 2.5f);
    chevron.lineTo(arrowX, arrowY + 2.5f);
    chevron.lineTo(arrowX + 4.0f, arrowY - 2.5f);
    g.setColour(juce::Colour(Colours::textSecondary));
    g.strokePath(chevron, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

void AxelFLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& bgColour,
                                              bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto colour = isDown ? bgColour.brighter(0.15f)
                         : isHighlighted ? bgColour.brighter(0.08f) : bgColour;

    // Subtle shadow
    g.setColour(juce::Colour(0x20000000));
    g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), 6.0f);

    // Body
    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 6.0f);

    // Top-edge highlight for depth
    g.setColour(juce::Colours::white.withAlpha(isDown ? 0.02f : 0.06f));
    g.drawHorizontalLine((int)bounds.getY(), bounds.getX() + 6.0f, bounds.getRight() - 6.0f);

    // Border
    g.setColour(juce::Colour(Colours::borderSubtle));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}

void AxelFLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
    auto textColour = label.findColour(juce::Label::textColourId).withMultipliedAlpha(label.isEnabled() ? 1.0f : 0.5f);

    g.setColour(textColour);
    g.setFont(label.getFont());
    g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                     juce::jmax(1, (int)((float)textArea.getHeight() / label.getFont().getHeight())),
                     label.getMinimumHorizontalScale());

    // Outline for editable labels
    if (label.isBeingEdited())
    {
        g.setColour(juce::Colour(Colours::accentBlue).withAlpha(0.6f));
        g.drawRoundedRectangle(label.getLocalBounds().toFloat(), 4.0f, 1.5f);
    }
}

void AxelFLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);
    g.setColour(juce::Colour(Colours::bgPanel));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(Colours::borderSubtle));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
}

void AxelFLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive, bool isHighlighted,
                                          bool isTicked, bool hasSubMenu,
                                          const juce::String& text, const juce::String& shortcutKeyText,
                                          const juce::Drawable* /*icon*/, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        auto sepArea = area.reduced(8, 0);
        g.setColour(juce::Colour(Colours::borderSubtle).withAlpha(0.5f));
        g.fillRect(sepArea.getX(), sepArea.getCentreY(), sepArea.getWidth(), 1);
        return;
    }

    auto r = area.reduced(4, 1);

    if (isHighlighted && isActive)
    {
        g.setColour(juce::Colour(Colours::accentBlue).withAlpha(0.25f));
        g.fillRoundedRectangle(r.toFloat(), 4.0f);
    }

    auto colour = textColour != nullptr ? *textColour
                : isActive ? juce::Colour(Colours::textPrimary)
                           : juce::Colour(Colours::textSecondary);
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(14.0f)));

    auto textBounds = r.reduced(8, 0);

    if (isTicked)
    {
        auto tickArea = textBounds.removeFromLeft(18);
        juce::Path tick;
        tick.startNewSubPath((float)tickArea.getX() + 3.0f, (float)tickArea.getCentreY());
        tick.lineTo((float)tickArea.getX() + 7.0f, (float)tickArea.getCentreY() + 4.0f);
        tick.lineTo((float)tickArea.getX() + 14.0f, (float)tickArea.getCentreY() - 4.0f);
        g.strokePath(tick, juce::PathStrokeType(1.5f));
    }

    g.drawFittedText(text, textBounds, juce::Justification::centredLeft, 1);

    if (hasSubMenu)
    {
        auto arrowX = (float)r.getRight() - 10.0f;
        auto arrowY = (float)r.getCentreY();
        juce::Path arrow;
        arrow.startNewSubPath(arrowX - 3.0f, arrowY - 4.0f);
        arrow.lineTo(arrowX + 3.0f, arrowY);
        arrow.lineTo(arrowX - 3.0f, arrowY + 4.0f);
        g.strokePath(arrow, juce::PathStrokeType(1.5f));
    }

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour(juce::Colour(Colours::textSecondary));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(shortcutKeyText, r.reduced(8, 0), juce::Justification::centredRight, true);
    }
}

void AxelFLookAndFeel::drawTabButton(juce::TabBarButton& button, juce::Graphics& g,
                                      bool isMouseOver, bool /*isMouseDown*/)
{
    auto area = button.getActiveArea();
    auto isActive = button.isFrontTab();

    g.setColour(isActive ? juce::Colour(0xFF1e1e38) : juce::Colour(0xFF16162a));
    g.fillRect(area);

    if (isMouseOver && !isActive)
    {
        g.setColour(juce::Colour(0x08FFFFFF));
        g.fillRect(area);
    }

    // Module accent colours for bottom bar and active text
    static const juce::uint32 tabAccents[] = {
        Colours::jupiter, Colours::moog, Colours::jx3p,
        Colours::dx7, Colours::linn, Colours::mixer
    };
    // Active text colours per module (brighter than accent)
    static const juce::uint32 tabTextActive[] = {
        0xFFff8888, 0xFFd4a843, 0xFF5bc4e8,
        0xFFa0c850, 0xFFd49060, 0xFF9a8aed
    };
    // MIDI channel labels
    static const char* midiChLabels[] = {
        "CH 1", "CH 2", "CH 3", "CH 4", "CH 10", "MASTER"
    };

    auto idx = button.getIndex();

    if (isActive && idx >= 0 && idx < 6)
    {
        auto accentColour = juce::Colour(tabAccents[idx]);
        g.setColour(accentColour);
        g.fillRect(area.getX(), area.getBottom() - 4, area.getWidth(), 4);
    }

    // Active tab highlight background
    if (isActive && idx >= 0 && idx < 6)
    {
        g.setColour(juce::Colour(tabAccents[idx]).withAlpha(0.08f));
        g.fillRect(area);
    }

    // Tab name text
    auto textColour = (isActive && idx >= 0 && idx < 6)
                        ? juce::Colour(tabTextActive[idx])
                        : (isActive ? juce::Colour(Colours::textPrimary)
                                    : juce::Colour(Colours::textSecondary));
    g.setColour(textColour);
    g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
    g.drawText(button.getButtonText(), area.withTrimmedBottom(16), juce::Justification::centred, false);

    // MIDI channel sub-label
    if (idx >= 0 && idx < 6)
    {
        g.setColour(juce::Colour(Colours::textSecondary));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText(midiChLabels[idx], area.withTrimmedTop(area.getHeight() - 16),
                   juce::Justification::centred, false);
    }
}

juce::Font AxelFLookAndFeel::getTabButtonFont(juce::TabBarButton&, float)
{
    return juce::Font(juce::FontOptions(17.0f, juce::Font::bold));
}

int AxelFLookAndFeel::getTabButtonBestWidth(juce::TabBarButton&, int)
{
    return 200;
}

} // namespace axelf::ui
