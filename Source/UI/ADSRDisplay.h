#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AxelFColours.h"

namespace axelf::ui
{

// Visual ADSR envelope curve. Listens to 4 slider value changes and redraws.
class ADSRDisplay : public juce::Component, private juce::Slider::Listener
{
public:
    ADSRDisplay(juce::Colour accent = juce::Colour(Colours::accentBlue))
        : colour(accent) {}

    ~ADSRDisplay() override { detach(); }

    void attach(juce::Slider& a, juce::Slider& d, juce::Slider& s, juce::Slider& r)
    {
        detach();
        atkSlider = &a; decSlider = &d; susSlider = &s; relSlider = &r;
        a.addListener(this); d.addListener(this); s.addListener(this); r.addListener(this);
        repaint();
    }

    void detach()
    {
        if (atkSlider) atkSlider->removeListener(this);
        if (decSlider) decSlider->removeListener(this);
        if (susSlider) susSlider->removeListener(this);
        if (relSlider) relSlider->removeListener(this);
        atkSlider = decSlider = susSlider = relSlider = nullptr;
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.0f);

        // Background
        g.setColour(juce::Colour(Colours::bgSection));
        g.fillRoundedRectangle(b, 4.0f);

        // Normalise values
        float atk = atkSlider ? getNorm(*atkSlider) : 0.1f;
        float dec = decSlider ? getNorm(*decSlider) : 0.3f;
        float sus = susSlider ? getNorm(*susSlider) : 0.6f;
        float rel = relSlider ? getNorm(*relSlider) : 0.3f;

        float segTotal = atk + dec + rel + 0.001f;
        float atkW = (atk / segTotal) * b.getWidth() * 0.75f;
        float decW = (dec / segTotal) * b.getWidth() * 0.75f;
        float susW = b.getWidth() * 0.25f;
        float relW = (rel / segTotal) * b.getWidth() * 0.75f;

        // Clamp total to fit
        float total = atkW + decW + susW + relW;
        float scale = b.getWidth() / total;
        atkW *= scale; decW *= scale; susW *= scale; relW *= scale;

        float x0 = b.getX();
        float top = b.getY() + 2.0f;
        float bottom = b.getBottom() - 2.0f;
        float range = bottom - top;
        float susY = top + range * (1.0f - sus);

        juce::Path path;
        path.startNewSubPath(x0, bottom);                       // origin
        path.lineTo(x0 + atkW, top);                            // attack peak
        path.lineTo(x0 + atkW + decW, susY);                    // decay to sustain
        path.lineTo(x0 + atkW + decW + susW, susY);             // sustain hold
        path.lineTo(x0 + atkW + decW + susW + relW, bottom);    // release

        // Filled area
        juce::Path filled(path);
        filled.lineTo(x0, bottom);
        filled.closeSubPath();
        g.setColour(colour.withAlpha(0.15f));
        g.fillPath(filled);

        // Stroke
        g.setColour(colour.withAlpha(0.9f));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

private:
    juce::Colour colour;
    juce::Slider* atkSlider = nullptr;
    juce::Slider* decSlider = nullptr;
    juce::Slider* susSlider = nullptr;
    juce::Slider* relSlider = nullptr;

    void sliderValueChanged(juce::Slider*) override { repaint(); }

    static float getNorm(juce::Slider& s)
    {
        auto range = s.getRange();
        if (range.getLength() < 0.0001) return 0.5f;
        return static_cast<float>((s.getValue() - range.getStart()) / range.getLength());
    }
};

} // namespace axelf::ui
