#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DX7Algorithm.h"

namespace axelf::ui
{

// Draws a compact DX7 algorithm diagram showing operators, carrier/modulator
// connections, and the feedback operator.
class AlgorithmDisplay : public juce::Component, private juce::Slider::Listener
{
public:
    explicit AlgorithmDisplay(juce::Colour accent = juce::Colour(0xFF6b8e23))
        : accentColour(accent) {}

    ~AlgorithmDisplay() override { detach(); }

    void attach(juce::Slider& algorithmSlider)
    {
        detach();
        algSlider = &algorithmSlider;
        algSlider->addListener(this);
        updateAlgorithm();
    }

    void detach()
    {
        if (algSlider) algSlider->removeListener(this);
        algSlider = nullptr;
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.f);
        g.setColour(juce::Colour(0xFF1a1f2e));
        g.fillRoundedRectangle(b, 4.f);

        auto topo = algorithm.getTopology();
        int alg = algorithm.getAlgorithm();

        // Title
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.setColour(accentColour);
        g.drawText("ALG " + juce::String(alg), b.removeFromTop(16.f), juce::Justification::centred);

        // Draw operators as circles in a grid
        float opRadius = juce::jmin(b.getWidth() / 8.f, b.getHeight() / 5.f);
        if (opRadius < 6.f) opRadius = 6.f;
        float cellW = b.getWidth() / 6.f;
        float midY = b.getCentreY();

        // Determine operator positions: lay out carriers on bottom row,
        // modulators above their targets. Simple 2-row layout.
        struct OpPos { float x, y; };
        OpPos pos[6];
        float bottomY = b.getBottom() - opRadius - 4.f;
        float topY = b.getY() + opRadius + 2.f;

        // Count carriers
        int numCarriers = 0;
        for (int i = 0; i < 6; ++i)
            if (algorithm.isCarrier(i)) numCarriers++;

        // Place carriers evenly on bottom row
        int cIdx = 0;
        float carrierSpacing = b.getWidth() / (float)(numCarriers + 1);
        for (int i = 0; i < 6; ++i)
        {
            if (algorithm.isCarrier(i))
            {
                pos[i] = { b.getX() + carrierSpacing * (float)(++cIdx), bottomY };
            }
        }

        // Place modulators above their first target
        for (int i = 0; i < 6; ++i)
        {
            if (!algorithm.isCarrier(i))
            {
                // Find what this op modulates
                float targetX = b.getCentreX();
                int depth = 0;
                // Walk the chain: find the carrier this leads to
                int current = i;
                int visited = 0;
                while (!algorithm.isCarrier(current) && visited < 6)
                {
                    depth++;
                    // Find what op 'current' modulates
                    bool found = false;
                    for (int t = 0; t < 6; ++t)
                    {
                        if ((topo.modulators[t] & (1 << current)) != 0)
                        {
                            targetX = pos[t].x; // may be 0 if not placed yet
                            current = t;
                            found = true;
                            break;
                        }
                    }
                    if (!found) break;
                    visited++;
                }
                if (algorithm.isCarrier(current))
                    targetX = pos[current].x;

                float yStep = (bottomY - topY) / 4.f;
                pos[i] = { targetX + (float)(depth - 1) * opRadius * 0.4f,
                           bottomY - (float)depth * yStep };
            }
        }

        // Draw connections
        g.setColour(accentColour.withAlpha(0.5f));
        for (int dst = 0; dst < 6; ++dst)
        {
            for (int src = 0; src < 6; ++src)
            {
                if ((topo.modulators[dst] & (1 << src)) != 0)
                {
                    g.drawLine(pos[src].x, pos[src].y + opRadius,
                               pos[dst].x, pos[dst].y - opRadius, 1.5f);
                }
            }
        }

        // Draw feedback arc
        if (topo.feedbackOp >= 0 && topo.feedbackOp < 6)
        {
            int fb = topo.feedbackOp;
            g.setColour(juce::Colours::yellow.withAlpha(0.4f));
            juce::Path arc;
            arc.addArc(pos[fb].x - opRadius * 1.2f, pos[fb].y - opRadius * 1.8f,
                       opRadius * 2.4f, opRadius * 1.6f, -1.f, 1.f);
            g.strokePath(arc, juce::PathStrokeType(1.2f));
        }

        // Draw operator circles
        for (int i = 0; i < 6; ++i)
        {
            bool isCarrier = algorithm.isCarrier(i);
            auto opColour = isCarrier ? accentColour : accentColour.withAlpha(0.6f);
            g.setColour(opColour.withAlpha(0.2f));
            g.fillEllipse(pos[i].x - opRadius, pos[i].y - opRadius, opRadius * 2, opRadius * 2);
            g.setColour(opColour);
            g.drawEllipse(pos[i].x - opRadius, pos[i].y - opRadius, opRadius * 2, opRadius * 2, 1.5f);

            // Op number
            g.setFont(juce::Font(juce::FontOptions(opRadius * 0.9f, juce::Font::bold)));
            g.setColour(isCarrier ? juce::Colours::white : juce::Colour(0xFFcccccc));
            g.drawText(juce::String(i + 1),
                       juce::Rectangle<float>(pos[i].x - opRadius, pos[i].y - opRadius,
                                              opRadius * 2, opRadius * 2),
                       juce::Justification::centred);
        }
    }

private:
    void sliderValueChanged(juce::Slider*) override { updateAlgorithm(); }

    void updateAlgorithm()
    {
        if (algSlider)
        {
            int alg = juce::roundToInt(algSlider->getValue());
            algorithm.setAlgorithm(juce::jlimit(1, 32, alg));
            repaint();
        }
    }

    juce::Colour accentColour;
    juce::Slider* algSlider = nullptr;
    dx7::DX7Algorithm algorithm;
};

} // namespace axelf::ui
