#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common/SceneManager.h"
#include "../Common/PatternEngine.h"
#include "../Common/GlobalTransport.h"
#include "AxelFColours.h"

namespace axelf::ui
{

// Horizontal timeline showing pattern arrangement across all modules.
// Each module gets a colour-coded lane; chain entries appear as blocks.
// A playhead cursor shows the current transport position.
class ArrangementView : public juce::Component,
                        private juce::Timer
{
public:
    ArrangementView(SceneManager& sm, PatternEngine& pe, GlobalTransport& tr)
        : sceneManager(sm), patternEngine(pe), transport(tr)
    {
        startTimerHz(30);

        // Collapse/expand toggle
        toggleBtn.setButtonText("ARRANGEMENT");
        toggleBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgPanel));
        toggleBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::accentGold));
        toggleBtn.onClick = [this]
        {
            collapsed = !collapsed;
            if (onToggle) onToggle(collapsed);
        };
        addAndMakeVisible(toggleBtn);
    }

    bool isCollapsed() const { return collapsed; }
    void setCollapsed(bool c) { collapsed = c; }

    // Preferred height when expanded
    static constexpr int kExpandedHeight = 140;
    static constexpr int kCollapsedHeight = 22;

    int getDesiredHeight() const { return collapsed ? kCollapsedHeight : kExpandedHeight; }

    /** Called by editor when collapse state changes so it can relayout */
    std::function<void(bool collapsed)> onToggle;

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(Colours::bgDark));
        g.fillRect(bounds);

        // Top border
        g.setColour(juce::Colour(Colours::borderSubtle));
        g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 1.0f);

        if (collapsed) return;

        auto area = getLocalBounds();
        area.removeFromTop(kCollapsedHeight); // below the toggle bar

        if (area.getHeight() < 20) return;

        // ── Draw lanes ───────────────────────────────────────
        const int laneH = area.getHeight() / kNumLanes;
        const int totalBars = transport.getBarCount();
        if (totalBars <= 0) return;

        const float barWidth = static_cast<float>(area.getWidth()) / static_cast<float>(totalBars);
        const float laneY0 = static_cast<float>(area.getY());

        // Module colours
        static constexpr juce::uint32 laneColours[kNumLanes] = {
            Colours::jupiter, Colours::moog, Colours::jx3p, Colours::dx7, Colours::linn
        };
        static const char* laneNames[kNumLanes] = {
            "JUP8", "MOOG", "JX3P", "DX7", "LINN"
        };

        for (int lane = 0; lane < kNumLanes; ++lane)
        {
            float y = laneY0 + static_cast<float>(lane * laneH);

            // Lane background (alternating shade)
            g.setColour(juce::Colour(lane % 2 == 0 ? Colours::bgDark : Colours::bgPanel).withAlpha(0.5f));
            g.fillRect(0.0f, y, bounds.getWidth(), static_cast<float>(laneH));

            // Lane label
            g.setColour(juce::Colour(Colours::textSecondary));
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText(laneNames[lane], 2, static_cast<int>(y) + 1, 32, laneH - 2,
                       juce::Justification::centredLeft, false);

            // Pattern blocks
            auto laneCol = juce::Colour(laneColours[lane]);

            if (lane < 4) // synth modules
            {
                const auto& pat = patternEngine.getSynthPattern(lane);
                int patBars = pat.getBarCount();

                for (int bar = 0; bar < totalBars; ++bar)
                {
                    float bx = static_cast<float>(bar) * barWidth + 34.0f;
                    float bw = barWidth - 2.0f;
                    if (bx + bw > bounds.getWidth()) bw = bounds.getWidth() - bx;

                    bool hasContent = false;
                    if (bar < patBars)
                    {
                        // Check if any events fall in this bar
                        double barStart = static_cast<double>(bar * pat.getBeatsPerBar());
                        double barEnd = barStart + static_cast<double>(pat.getBeatsPerBar());
                        std::vector<MidiEvent> tmp;
                        pat.getEventsInRange(barStart, barEnd, tmp);
                        hasContent = !tmp.empty();
                    }

                    float blockY = y + 2.0f;
                    float blockH = static_cast<float>(laneH) - 4.0f;

                    if (hasContent)
                    {
                        g.setColour(laneCol.withAlpha(0.6f));
                        g.fillRoundedRectangle(bx, blockY, bw, blockH, 3.0f);
                        g.setColour(laneCol.withAlpha(0.9f));
                        g.drawRoundedRectangle(bx, blockY, bw, blockH, 3.0f, 1.0f);
                    }
                    else
                    {
                        g.setColour(laneCol.withAlpha(0.12f));
                        g.fillRoundedRectangle(bx, blockY, bw, blockH, 3.0f);
                    }
                }
            }
            else // drum lane
            {
                const auto& dp = patternEngine.getDrumPattern();
                int patBars = dp.getBarCount();

                for (int bar = 0; bar < totalBars; ++bar)
                {
                    float bx = static_cast<float>(bar) * barWidth + 34.0f;
                    float bw = barWidth - 2.0f;
                    if (bx + bw > bounds.getWidth()) bw = bounds.getWidth() - bx;

                    bool hasContent = false;
                    if (bar < patBars)
                    {
                        int stepStart = bar * DrumPattern::kStepsPerBar;
                        int stepEnd = stepStart + DrumPattern::kStepsPerBar;
                        for (int s = stepStart; s < stepEnd && !hasContent; ++s)
                            for (int t = 0; t < DrumPattern::kMaxTracks && !hasContent; ++t)
                                if (dp.getStep(t, s)) hasContent = true;
                    }

                    float blockY = y + 2.0f;
                    float blockH = static_cast<float>(laneH) - 4.0f;

                    if (hasContent)
                    {
                        g.setColour(laneCol.withAlpha(0.6f));
                        g.fillRoundedRectangle(bx, blockY, bw, blockH, 3.0f);
                        g.setColour(laneCol.withAlpha(0.9f));
                        g.drawRoundedRectangle(bx, blockY, bw, blockH, 3.0f, 1.0f);
                    }
                    else
                    {
                        g.setColour(laneCol.withAlpha(0.12f));
                        g.fillRoundedRectangle(bx, blockY, bw, blockH, 3.0f);
                    }
                }
            }
        }

        // ── Playhead cursor ──────────────────────────────────
        if (transport.isPlaying())
        {
            double posBeats = transport.getPositionInBeats();
            double totalBeats = transport.getPatternLengthInBeats();
            if (totalBeats > 0.0)
            {
                float frac = static_cast<float>(posBeats / totalBeats);
                float usableWidth = bounds.getWidth() - 34.0f;
                float px = 34.0f + frac * usableWidth;

                g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.8f));
                g.fillRect(px - 1.0f, laneY0, 2.0f, static_cast<float>(kNumLanes * laneH));
            }
        }

        // ── Chain indicator (if active) ──────────────────────
        if (sceneManager.isChainMode())
        {
            g.setColour(juce::Colour(Colours::accentOrange));
            g.setFont(juce::Font(9.0f, juce::Font::bold));

            const auto& chain = sceneManager.getChain();
            float cx = 34.0f;
            float chainY = laneY0 + static_cast<float>(kNumLanes * laneH) - 12.0f;
            for (const auto& entry : chain)
            {
                juce::String label = juce::String::charToString('A' + entry.sceneIndex)
                                     + "x" + juce::String(entry.repeatCount);
                g.drawText(label, static_cast<int>(cx), static_cast<int>(chainY), 40, 12,
                           juce::Justification::centredLeft, false);
                cx += 44.0f;
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        toggleBtn.setBounds(area.removeFromTop(kCollapsedHeight));
    }

private:
    static constexpr int kNumLanes = 5;

    SceneManager& sceneManager;
    PatternEngine& patternEngine;
    GlobalTransport& transport;

    juce::TextButton toggleBtn;
    bool collapsed = true;

    void timerCallback() override
    {
        if (!collapsed)
            repaint();
    }
};

} // namespace axelf::ui
