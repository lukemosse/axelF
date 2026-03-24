#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common/Pattern.h"
#include "../Common/GlobalTransport.h"
#include "../Common/PatternEngine.h"
#include "AxelFColours.h"

namespace axelf::ui
{

// Compact pattern strip for synth module editors.
// Shows: Record Arm | mini piano-roll | Clear | Quantize selector
// Designed to sit at the bottom of each synth editor.
class PatternStrip : public juce::Component,
                     private juce::Timer
{
public:
    PatternStrip(PatternEngine& engine, GlobalTransport& transport, int moduleIdx,
                 juce::Colour accentColour)
        : patternEngine(engine), globalTransport(transport),
          moduleIndex(moduleIdx), accent(accentColour)
    {
        // Clear button
        clearBtn.setButtonText("CLR");
        clearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        clearBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        clearBtn.onClick = [this]()
        {
            patternEngine.getSynthPattern(moduleIndex).clear();
            repaint();
        };
        addAndMakeVisible(clearBtn);

        // Quantize selector
        quantizeBox.addItem("Off", 1);
        quantizeBox.addItem("1/4", 2);
        quantizeBox.addItem("1/8", 3);
        quantizeBox.addItem("1/16", 4);
        quantizeBox.addItem("1/32", 5);
        quantizeBox.setSelectedId(4); // default 1/16
        quantizeBox.onChange = [this]()
        {
            int sel = quantizeBox.getSelectedId();
            QuantizeGrid grid = QuantizeGrid::Off;
            switch (sel)
            {
                case 2: grid = QuantizeGrid::Quarter; break;
                case 3: grid = QuantizeGrid::Eighth; break;
                case 4: grid = QuantizeGrid::Sixteenth; break;
                case 5: grid = QuantizeGrid::ThirtySecond; break;
                default: break;
            }
            patternEngine.setQuantizeGrid(moduleIndex, grid);
        };
        addAndMakeVisible(quantizeBox);

        // Quantize label
        quantizeLbl.setText("Quantize", juce::dontSendNotification);
        quantizeLbl.setFont(juce::Font(juce::FontOptions(9.0f)));
        quantizeLbl.setColour(juce::Label::textColourId, juce::Colour(Colours::textLabel));
        quantizeLbl.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(quantizeLbl);

        startTimerHz(20);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(Colours::bgSection));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Accent stripe at top
        g.setColour(accent.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), 3.0f, 4.0f);

        // Label
        g.setColour(juce::Colour(Colours::textLabel));
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText("PATTERN", 6, 5, 56, 14, juce::Justification::centredLeft);

        // Piano roll area
        drawPianoRoll(g);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(4, 4);

        // Left side: label
        area.removeFromLeft(62);

        // Right side: clear + quantize
        auto right = area.removeFromRight(180);
        right.removeFromTop(2);
        quantizeLbl.setBounds(right.removeFromLeft(48).withHeight(20));
        right.removeFromLeft(2);
        quantizeBox.setBounds(right.removeFromLeft(60).withHeight(22));
        right.removeFromLeft(8);
        clearBtn.setBounds(right.removeFromLeft(44).withHeight(22));

        // Piano roll area = everything in between (stored for use in paint)
        pianoRollArea = area.reduced(4, 2);
    }

private:
    PatternEngine& patternEngine;
    GlobalTransport& globalTransport;
    int moduleIndex;
    juce::Colour accent;

    juce::TextButton clearBtn;
    juce::ComboBox quantizeBox;
    juce::Label quantizeLbl;
    juce::Rectangle<int> pianoRollArea;

    void timerCallback() override
    {
        if (globalTransport.isPlaying())
            repaint();
    }

    void drawPianoRoll(juce::Graphics& g) const
    {
        if (pianoRollArea.isEmpty()) return;

        auto& pattern = patternEngine.getSynthPattern(moduleIndex);
        const auto& events = pattern.getEvents();
        double patternLen = pattern.getLengthInBeats();
        if (patternLen <= 0.0) return;

        float x0 = static_cast<float>(pianoRollArea.getX());
        float y0 = static_cast<float>(pianoRollArea.getY());
        float w = static_cast<float>(pianoRollArea.getWidth());
        float h = static_cast<float>(pianoRollArea.getHeight());

        // Background
        g.setColour(juce::Colour(Colours::bgDark).withAlpha(0.6f));
        g.fillRoundedRectangle(x0, y0, w, h, 3.0f);

        // Beat grid lines
        int totalBeats = static_cast<int>(patternLen);
        g.setColour(juce::Colour(Colours::borderSubtle).withAlpha(0.3f));
        for (int b = 1; b < totalBeats; ++b)
        {
            float bx = x0 + static_cast<float>(b) / static_cast<float>(patternLen) * w;
            g.drawLine(bx, y0, bx, y0 + h, 0.5f);
        }

        // Bar lines (heavier)
        int beatsPerBar = pattern.getBeatsPerBar();
        g.setColour(juce::Colour(Colours::borderSubtle).withAlpha(0.6f));
        for (int b = beatsPerBar; b < totalBeats; b += beatsPerBar)
        {
            float bx = x0 + static_cast<float>(b) / static_cast<float>(patternLen) * w;
            g.drawLine(bx, y0, bx, y0 + h, 1.0f);
        }

        // Note range for scaling
        if (events.empty()) return;
        int minNote = 127, maxNote = 0;
        for (const auto& e : events)
        {
            minNote = std::min(minNote, e.noteNumber);
            maxNote = std::max(maxNote, e.noteNumber);
        }
        int noteRange = std::max(maxNote - minNote + 1, 12); // at least one octave
        int midNote = (minNote + maxNote) / 2;
        int noteBottom = midNote - noteRange / 2;

        // Draw events as small rectangles
        for (const auto& e : events)
        {
            float ex = x0 + static_cast<float>(e.startBeat / patternLen) * w;
            float ew = static_cast<float>(e.duration / patternLen) * w;
            ew = std::max(ew, 2.0f);
            float ey = y0 + h - ((static_cast<float>(e.noteNumber - noteBottom) + 0.5f) / static_cast<float>(noteRange)) * h;
            float eh = std::max(h / static_cast<float>(noteRange), 2.0f);

            g.setColour(accent.withAlpha(0.4f + 0.6f * e.velocity));
            g.fillRoundedRectangle(ex, ey - eh * 0.5f, ew, eh, 1.0f);
        }

        // Playback cursor
        if (globalTransport.isPlaying())
        {
            double pos = globalTransport.getPositionInBeats();
            double wrapped = std::fmod(pos, patternLen);
            float cx = x0 + static_cast<float>(wrapped / patternLen) * w;
            g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.8f));
            g.drawLine(cx, y0, cx, y0 + h, 1.5f);
        }
    }
};

} // namespace axelf::ui
