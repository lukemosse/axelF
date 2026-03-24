#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common/Pattern.h"
#include "../Common/GlobalTransport.h"
#include "../Common/PatternEngine.h"
#include "../Modules/LinnDrum/LinnDrumParams.h"
#include "AxelFColours.h"

namespace axelf::ui
{

// Multi-bar step sequencer grid for the LinnDrum module.
// Displays 15 drum tracks × (barCount × 16) steps with:
//   - click-to-toggle steps
//   - velocity editing via vertical drag within active cells
//   - velocity shown as filled bar height + brightness
//   - playback cursor
//   - bar divider lines
class DrumStepGrid : public juce::Component,
                     private juce::Timer
{
public:
    DrumStepGrid(PatternEngine& engine, GlobalTransport& transport)
        : patternEngine(engine), globalTransport(transport)
    {
        // Loop length selector
        loopLenBox.addItem("1 Bar", 1);
        loopLenBox.addItem("2 Bars", 2);
        loopLenBox.addItem("4 Bars", 4);
        loopLenBox.setSelectedId(4, juce::dontSendNotification);
        loopLenBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(Colours::bgControl));
        loopLenBox.setColour(juce::ComboBox::textColourId, juce::Colour(Colours::textPrimary));
        loopLenBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(Colours::borderSubtle));
        loopLenBox.onChange = [this]()
        {
            int bars = loopLenBox.getSelectedId();
            if (bars > 0)
                patternEngine.getDrumPattern().setLoopBars(bars);
            repaint();
        };
        addAndMakeVisible(loopLenBox);

        loopLenLabel.setText("Loop:", juce::dontSendNotification);
        loopLenLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        loopLenLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textLabel));
        loopLenLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(loopLenLabel);

        startTimerHz(30);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto headerArea = area.removeFromTop(24);
        headerArea.removeFromLeft(70); // align with grid label width
        loopLenLabel.setBounds(headerArea.removeFromLeft(40));
        headerArea.removeFromLeft(4);
        loopLenBox.setBounds(headerArea.removeFromLeft(70));
    }

    void paint(juce::Graphics& g) override
    {
        auto& pattern = patternEngine.getDrumPattern();
        const int loopSteps = pattern.getLoopSteps();
        const int numTracks = DrumPattern::kMaxTracks;
        const auto& drumNames = linndrum::getDrumNames();

        const float headerH = 24.0f;
        const float labelW = 70.0f;
        const float gridX = labelW;
        const float gridW = static_cast<float>(getWidth()) - labelW;
        const float gridH = static_cast<float>(getHeight()) - headerH;
        const float cellW = loopSteps > 0 ? gridW / static_cast<float>(loopSteps) : 16.0f;
        const float cellH = numTracks > 0 ? gridH / static_cast<float>(numTracks) : 16.0f;

        g.fillAll(juce::Colour(Colours::bgDark));

        // Draw track labels
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        for (int t = 0; t < numTracks; ++t)
        {
            float y = headerH + static_cast<float>(t) * cellH;
            g.setColour(juce::Colour(Colours::textLabel));
            g.drawText(drumNames[t].toUpperCase(), 4, (int)y, (int)labelW - 8, (int)cellH,
                       juce::Justification::centredRight, false);
        }

        // Draw grid cells
        for (int t = 0; t < numTracks; ++t)
        {
            float y = headerH + static_cast<float>(t) * cellH;
            for (int s = 0; s < loopSteps; ++s)
            {
                float x = gridX + static_cast<float>(s) * cellW;
                auto cellRect = juce::Rectangle<float>(x + 0.5f, y + 0.5f, cellW - 1.0f, cellH - 1.0f);

                bool active = pattern.getStep(t, s);
                if (active)
                {
                    float vel = pattern.getVelocity(t, s);

                    // Background fill (dimmer)
                    g.setColour(juce::Colour(Colours::linn).withAlpha(0.2f));
                    g.fillRoundedRectangle(cellRect, 2.0f);

                    // Velocity bar: fills from bottom up proportional to velocity
                    float barH = std::max(2.0f, (cellRect.getHeight() - 2.0f) * vel);
                    auto velRect = juce::Rectangle<float>(
                        cellRect.getX() + 1.0f,
                        cellRect.getBottom() - barH - 1.0f,
                        cellRect.getWidth() - 2.0f,
                        barH);
                    auto barCol = juce::Colour(Colours::linn).withAlpha(0.4f + 0.6f * vel);
                    g.setColour(barCol);
                    g.fillRoundedRectangle(velRect, 1.5f);

                    // Border for the entire cell
                    g.setColour(juce::Colour(Colours::linn).withAlpha(0.5f));
                    g.drawRoundedRectangle(cellRect, 2.0f, 0.5f);
                }
                else
                {
                    // Alternate background for beat groups
                    int bar = s / DrumPattern::kStepsPerBar;
                    int beatInBar = (s % DrumPattern::kStepsPerBar) / 4;
                    bool darkBeat = (bar % 2 == 0) ? (beatInBar % 2 == 0) : (beatInBar % 2 == 1);
                    auto bgCol = darkBeat ? juce::Colour(Colours::bgSection) : juce::Colour(Colours::bgPanel);
                    g.setColour(bgCol);
                    g.fillRoundedRectangle(cellRect, 2.0f);
                }
            }
        }

        // Draw bar divider lines
        g.setColour(juce::Colour(Colours::borderSubtle).withAlpha(0.8f));
        int loopBars = pattern.getLoopBars();
        for (int bar = 1; bar < loopBars; ++bar)
        {
            float x = gridX + static_cast<float>(bar * DrumPattern::kStepsPerBar) * cellW;
            g.drawLine(x, headerH, x, headerH + gridH, 1.5f);
        }

        // Draw beat divider lines (lighter)
        g.setColour(juce::Colour(Colours::borderSubtle).withAlpha(0.3f));
        for (int s = 4; s < loopSteps; s += 4)
        {
            if (s % DrumPattern::kStepsPerBar == 0) continue; // skip bar lines
            float x = gridX + static_cast<float>(s) * cellW;
            g.drawLine(x, headerH, x, headerH + gridH, 0.5f);
        }

        // Track divider lines
        g.setColour(juce::Colour(Colours::borderSubtle).withAlpha(0.2f));
        for (int t = 1; t < numTracks; ++t)
        {
            float y = headerH + static_cast<float>(t) * cellH;
            g.drawLine(gridX, y, gridX + gridW, y, 0.5f);
        }

        // Playback cursor
        if (globalTransport.isPlaying())
        {
            int currentStep = globalTransport.getCurrentStep() % loopSteps;
            if (currentStep >= 0 && currentStep < loopSteps)
            {
                float x = gridX + static_cast<float>(currentStep) * cellW;
                g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.8f));
                g.fillRect(x, headerH, cellW, gridH);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto [track, step] = getCellAt(e.getPosition());
        if (track < 0 || step < 0) return;

        auto& pattern = patternEngine.getDrumPattern();

        if (e.mods.isShiftDown() && pattern.getStep(track, step))
        {
            // Shift+click on active step: enter velocity edit mode
            velocityEditMode = true;
            velEditTrack = track;
            velEditStep = step;
            setVelocityFromY(e.getPosition(), track, step);
            return;
        }

        // Normal click: toggle step
        velocityEditMode = false;
        toggleStepAt(e.getPosition());
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (velocityEditMode)
        {
            // Velocity editing: drag vertically to adjust velocity
            auto [track, step] = getCellAt(e.getPosition());

            // If dragged to a different cell, adjust that cell's velocity instead
            // (only if it's active)
            if (track >= 0 && step >= 0)
            {
                auto& pattern = patternEngine.getDrumPattern();
                if (pattern.getStep(track, step))
                {
                    velEditTrack = track;
                    velEditStep = step;
                }
            }

            setVelocityFromY(e.getPosition(), velEditTrack, velEditStep);
            return;
        }

        // Paint mode: toggle cells
        auto [track, step] = getCellAt(e.getPosition());
        if (track < 0 || step < 0) return;

        // Only toggle if we moved to a new cell
        if (track != lastDragTrack || step != lastDragStep)
        {
            auto& pattern = patternEngine.getDrumPattern();
            pattern.setStep(track, step, dragSetActive, dragSetActive ? 0.8f : 0.0f);
            lastDragTrack = track;
            lastDragStep = step;
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        lastDragTrack = -1;
        lastDragStep = -1;
        velocityEditMode = false;
        velEditTrack = -1;
        velEditStep = -1;
    }

private:
    PatternEngine& patternEngine;
    GlobalTransport& globalTransport;

    juce::ComboBox loopLenBox;
    juce::Label loopLenLabel;

    bool dragSetActive = true;
    int lastDragTrack = -1;
    int lastDragStep = -1;

    // Velocity editing state
    bool velocityEditMode = false;
    int velEditTrack = -1;
    int velEditStep = -1;

    void timerCallback() override
    {
        if (globalTransport.isPlaying())
            repaint();
    }

    // Compute velocity from vertical mouse position within a cell
    // Top of cell = max velocity, bottom = min velocity
    void setVelocityFromY(juce::Point<int> pos, int track, int step)
    {
        if (track < 0 || step < 0) return;

        auto& pattern = patternEngine.getDrumPattern();
        if (!pattern.getStep(track, step)) return;

        const float headerH = 24.0f;
        const float gridH = static_cast<float>(getHeight()) - headerH;
        const float cellH = gridH / static_cast<float>(DrumPattern::kMaxTracks);

        float cellTopY = headerH + static_cast<float>(track) * cellH;
        float relY = static_cast<float>(pos.getY()) - cellTopY;
        float normalizedY = relY / cellH;

        // Invert: top = high velocity (1.0), bottom = low (0.1)
        float vel = 1.0f - std::clamp(normalizedY, 0.0f, 1.0f);
        vel = std::max(0.1f, vel); // minimum 0.1 so step stays audible

        pattern.setStep(track, step, true, vel);
        repaint();
    }

    std::pair<int, int> getCellAt(juce::Point<int> pos) const
    {
        auto& pattern = patternEngine.getDrumPattern();
        const int loopSteps = pattern.getLoopSteps();
        const int numTracks = DrumPattern::kMaxTracks;

        const float headerH = 24.0f;
        const float labelW = 70.0f;
        const float gridW = static_cast<float>(getWidth()) - labelW;
        const float gridH = static_cast<float>(getHeight()) - headerH;

        float relX = static_cast<float>(pos.getX()) - labelW;
        float relY = static_cast<float>(pos.getY()) - headerH;

        if (relX < 0 || relX >= gridW || relY < 0 || relY >= gridH)
            return {-1, -1};

        int track = static_cast<int>(relY / (gridH / static_cast<float>(numTracks)));
        int step = static_cast<int>(relX / (gridW / static_cast<float>(loopSteps)));

        track = std::clamp(track, 0, numTracks - 1);
        step = std::clamp(step, 0, loopSteps - 1);
        return {track, step};
    }

    void toggleStepAt(juce::Point<int> pos)
    {
        auto [track, step] = getCellAt(pos);
        if (track < 0 || step < 0) return;

        auto& pattern = patternEngine.getDrumPattern();
        bool wasActive = pattern.getStep(track, step);
        dragSetActive = !wasActive;
        pattern.setStep(track, step, dragSetActive, dragSetActive ? 0.8f : 0.0f);
        lastDragTrack = track;
        lastDragStep = step;
        repaint();
    }
};

} // namespace axelf::ui
