#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace axelf::ui
{

// Compact 4-rate/4-level EG display for DX7 operators.
// Path: L4 → L1 (R1) → L2 (R2) → L3 sustain (R3) → L4 (R4)
class DX7EGDisplay : public juce::Component, private juce::Slider::Listener
{
public:
    DX7EGDisplay(juce::Colour accent = juce::Colour(0xFF6b8e23))
        : accentColour(accent) {}

    ~DX7EGDisplay() override { detach(); }

    void attach(juce::Slider& r1, juce::Slider& r2, juce::Slider& r3, juce::Slider& r4,
                juce::Slider& l1, juce::Slider& l2, juce::Slider& l3, juce::Slider& l4)
    {
        detach();
        rates[0] = &r1; rates[1] = &r2; rates[2] = &r3; rates[3] = &r4;
        levels[0] = &l1; levels[1] = &l2; levels[2] = &l3; levels[3] = &l4;
        for (auto* s : rates)  s->addListener(this);
        for (auto* s : levels) s->addListener(this);
        repaint();
    }

    void detach()
    {
        for (auto* s : rates)  if (s) s->removeListener(this);
        for (auto* s : levels) if (s) s->removeListener(this);
        for (auto*& s : rates)  s = nullptr;
        for (auto*& s : levels) s = nullptr;
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.f);
        g.setColour(juce::Colour(0xFF1a1f2e));
        g.fillRoundedRectangle(b, 3.f);

        auto getNorm = [](juce::Slider* s) -> float {
            if (!s) return 0.5f;
            auto range = s->getRange();
            return range.getLength() > 0.0
                ? static_cast<float>((s->getValue() - range.getStart()) / range.getLength())
                : 0.5f;
        };

        // Levels normalised 0-1 (higher = up)
        float l4 = getNorm(levels[3]);
        float l1 = getNorm(levels[0]);
        float l2 = getNorm(levels[1]);
        float l3 = getNorm(levels[2]);

        // Rates normalised — higher rate = narrower time segment
        float rNorm[4];
        for (int i = 0; i < 4; ++i)
            rNorm[i] = 1.f - getNorm(rates[i]); // invert: high rate → short time

        // Widths proportional to inverted rates (add minimum width)
        float totalW = 0.f;
        float widths[4];
        for (int i = 0; i < 4; ++i) { widths[i] = 0.1f + rNorm[i]; totalW += widths[i]; }
        float wScale = b.getWidth() / totalW;

        auto yForLevel = [&](float lvl) { return b.getBottom() - lvl * b.getHeight(); };

        juce::Path path;
        float x = b.getX();
        path.startNewSubPath(x, yForLevel(l4));

        // Segment 1: L4 → L1
        x += widths[0] * wScale;
        path.lineTo(x, yForLevel(l1));
        // Segment 2: L1 → L2
        x += widths[1] * wScale;
        path.lineTo(x, yForLevel(l2));
        // Segment 3: L2 → L3 (sustain)
        x += widths[2] * wScale;
        path.lineTo(x, yForLevel(l3));
        // Segment 4: L3 → L4 (release)
        x += widths[3] * wScale;
        path.lineTo(x, yForLevel(l4));

        // Fill
        juce::Path fill(path);
        fill.lineTo(b.getRight(), b.getBottom());
        fill.lineTo(b.getX(), b.getBottom());
        fill.closeSubPath();
        g.setColour(accentColour.withAlpha(0.15f));
        g.fillPath(fill);

        // Stroke
        g.setColour(accentColour.withAlpha(0.9f));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

private:
    void sliderValueChanged(juce::Slider*) override { repaint(); }

    juce::Colour accentColour;
    juce::Slider* rates[4]  = {};
    juce::Slider* levels[4] = {};
};

} // namespace axelf::ui
