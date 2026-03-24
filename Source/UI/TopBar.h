#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AxelFColours.h"

namespace axelf::ui
{

class TopBar : public juce::Component,
               private juce::Timer
{
public:
    TopBar()
    {
        // ── Preset label ─────────────────────────────
        presetLabel.setText("", juce::dontSendNotification);
        presetLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        presetLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::accentGold));
        presetLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(presetLabel);

        // ── Transport buttons ────────────────────────
        playButton.setButtonText(juce::String::charToString(0x25B6));  // play triangle
        playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1B5E20));
        playButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF66BB6A));
        addAndMakeVisible(playButton);

        stopButton.setButtonText(juce::String::charToString(0x25A0));  // stop square
        stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF37474F));
        stopButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
        addAndMakeVisible(stopButton);

        recordButton.setButtonText(juce::String::charToString(0x25CF));  // record circle
        recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF5D0000));
        recordButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFEF5350));
        addAndMakeVisible(recordButton);

        // ── Count-in toggle ──────────────────────────
        countInButton.setButtonText("COUNT IN");
        countInButton.setClickingTogglesState(true);
        countInButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        countInButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF1B5E20));
        countInButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        countInButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF66BB6A));
        addAndMakeVisible(countInButton);

        // ── BPM editor ───────────────────────────────
        bpmLabel.setText("120", juce::dontSendNotification);
        bpmLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
        bpmLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textPrimary));
        bpmLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xFF1a1a3e));
        bpmLabel.setColour(juce::Label::outlineColourId, juce::Colour(Colours::borderSubtle));
        bpmLabel.setJustificationType(juce::Justification::centred);
        bpmLabel.setEditable(true);
        addAndMakeVisible(bpmLabel);

        bpmSuffix.setText("BPM", juce::dontSendNotification);
        bpmSuffix.setFont(juce::Font(juce::FontOptions(9.0f)));
        bpmSuffix.setColour(juce::Label::textColourId, juce::Colour(Colours::textSecondary));
        bpmSuffix.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(bpmSuffix);

        // ── Position display ─────────────────────────
        positionLabel.setText("1.1 / 4", juce::dontSendNotification);
        positionLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        positionLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::accentGold));
        positionLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(positionLabel);

        // ── Bar count selector ───────────────────────
        barCountBox.addItem("1 Bar", 1);
        barCountBox.addItem("2 Bars", 2);
        barCountBox.addItem("4 Bars", 4);
        barCountBox.addItem("8 Bars", 8);
        barCountBox.addItem("16 Bars", 16);
        barCountBox.setSelectedId(4, juce::dontSendNotification);
        barCountBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF1a1a3e));
        barCountBox.setColour(juce::ComboBox::textColourId, juce::Colour(Colours::textPrimary));
        barCountBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(Colours::borderSubtle));
        addAndMakeVisible(barCountBox);

        // ── Click / metronome toggle ──────────────────
        clickButton.setButtonText("CLICK");
        clickButton.setClickingTogglesState(true);
        clickButton.setToggleState(true, juce::dontSendNotification);
        clickButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        clickButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF1B5E20));
        clickButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        clickButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF66BB6A));
        addAndMakeVisible(clickButton);

        // ── Panic button ─────────────────────────────
        panicButton.setButtonText("PANIC");
        panicButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::accentRed).withAlpha(0.7f));
        panicButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
        addAndMakeVisible(panicButton);

        // ── File / session button ───────────────────────
        fileButton.setButtonText("FILE");
        fileButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        fileButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        fileButton.setTooltip("Session: New, Open, Save (Ctrl+S)");
        addAndMakeVisible(fileButton);

        // ── Session name label ───────────────────────
        sessionLabel.setText("Untitled", juce::dontSendNotification);
        sessionLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
        sessionLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textSecondary));
        sessionLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(sessionLabel);

        // ── CPU meter label ──────────────────────────
        cpuLabel.setText("CPU 0%", juce::dontSendNotification);
        cpuLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
        cpuLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textSecondary));
        cpuLabel.setJustificationType(juce::Justification::centred);
        cpuLabel.setTooltip("Audio processing CPU usage");
        addAndMakeVisible(cpuLabel);

        // ── Invisible logo click area ─────────────────
        logoButton.setTooltip("About Axel F");
        logoButton.setAlpha(0.0f);
        addAndMakeVisible(logoButton);

        // Tooltips
        playButton.setTooltip("Play / Pause (Space)");
        stopButton.setTooltip("Stop");
        recordButton.setTooltip("Record");
        countInButton.setTooltip("Enable 1-bar count-in before play/record");
        clickButton.setTooltip("Toggle metronome click on/off");
        panicButton.setTooltip("All Notes Off — silence all modules");
        barCountBox.setTooltip("Pattern length in bars");

        startTimerHz(30);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Gradient background matching HTML mockup
        juce::ColourGradient bg(juce::Colour(0xFF2a2a48), bounds.getX(), bounds.getY(),
                                juce::Colour(0xFF1a1a30), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(bg);
        g.fillRect(bounds);

        // Gold bottom border (3px)
        g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.6f));
        g.fillRect(bounds.getX(), bounds.getBottom() - 3.0f, bounds.getWidth(), 3.0f);

        // Logo with glow effect
        g.setColour(juce::Colour(0x30d4a843));  // faint gold glow
        g.setFont(juce::Font(juce::FontOptions(34.0f, juce::Font::bold)));
        g.drawText("AXEL F", 13, 1, 150, getHeight() - 10, juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(Colours::accentGold));
        g.setFont(juce::Font(juce::FontOptions(34.0f, juce::Font::bold)));
        g.drawText("AXEL F", 12, 0, 150, getHeight() - 10, juce::Justification::centredLeft, false);

        // Subtitle
        g.setColour(juce::Colour(Colours::textSecondary));
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.drawText("SYNTH WORKSTATION", 16, getHeight() / 2 + 6, 140, getHeight() / 2 - 6,
                   juce::Justification::topLeft, false);

        // MIDI activity LEDs (5 dots between logo and transport area)
        {
            static const juce::Colour ledColours[5] = {
                juce::Colour(0xFFFF6B6B), // Jupiter-8 red
                juce::Colour(0xFF4ECDC4), // Moog teal
                juce::Colour(0xFFFFD93D), // JX-3P yellow
                juce::Colour(0xFF6C5CE7), // DX7 purple
                juce::Colour(0xFFFF9F43), // LinnDrum orange
            };
            const float ledY = static_cast<float>(getHeight() - 10);
            for (int i = 0; i < 5; ++i)
            {
                float ledX = 14.0f + static_cast<float>(i) * 22.0f;
                g.setColour(midiLedState[static_cast<size_t>(i)]
                    ? ledColours[i] : juce::Colour(0xFF2a2a48));
                g.fillEllipse(ledX, ledY, 6.0f, 6.0f);
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8, 4);

        // Right side: panic
        panicButton.setBounds(area.removeFromRight(60));
        area.removeFromRight(4);

        // Right side: CPU meter
        cpuLabel.setBounds(area.removeFromRight(52));
        area.removeFromRight(4);

        // Right side: click toggle
        clickButton.setBounds(area.removeFromRight(52));
        area.removeFromRight(8);

        // Right side: bar count
        barCountBox.setBounds(area.removeFromRight(80));
        area.removeFromRight(8);

        // Right side: position
        positionLabel.setBounds(area.removeFromRight(70));
        area.removeFromRight(4);

        // Right side: BPM
        bpmSuffix.setBounds(area.removeFromRight(26));
        bpmLabel.setBounds(area.removeFromRight(44));
        area.removeFromRight(12);

        // Left of preset: transport buttons
        area.removeFromLeft(160); // logo space
        logoButton.setBounds(0, 0, 160, getHeight());

        // FILE button + session name
        fileButton.setBounds(area.removeFromLeft(52));
        area.removeFromLeft(4);
        sessionLabel.setBounds(area.removeFromLeft(100));
        area.removeFromLeft(4);

        auto transportArea = area.removeFromLeft(110);
        int btnW = 34;
        playButton.setBounds(transportArea.removeFromLeft(btnW));
        transportArea.removeFromLeft(2);
        stopButton.setBounds(transportArea.removeFromLeft(btnW));
        transportArea.removeFromLeft(2);
        recordButton.setBounds(transportArea.removeFromLeft(btnW));

        area.removeFromLeft(4);
        countInButton.setBounds(area.removeFromLeft(64));

        area.removeFromLeft(8);
        presetLabel.setBounds(area);
    }

    // ── Public accessors ─────────────────────────────────
    juce::Label& getPresetLabel() { return presetLabel; }
    juce::Label& getBpmLabel() { return bpmLabel; }
    juce::Label& getPositionLabel() { return positionLabel; }
    juce::TextButton& getPlayButton() { return playButton; }
    juce::TextButton& getStopButton() { return stopButton; }
    juce::TextButton& getRecordButton() { return recordButton; }
    juce::TextButton& getPanicButton() { return panicButton; }
    juce::TextButton& getCountInButton() { return countInButton; }
    juce::TextButton& getClickButton() { return clickButton; }
    juce::TextButton& getLogoButton() { return logoButton; }
    juce::ComboBox& getBarCountBox() { return barCountBox; }
    juce::TextButton& getFileButton() { return fileButton; }
    juce::Label& getSessionLabel() { return sessionLabel; }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        {
            if (auto* peer = window->getPeer())
            {
                bool isMax = peer->isFullScreen();
                peer->setFullScreen(!isMax);
            }
        }
    }

    // Set by PluginEditor so TopBar can poll transport state
    std::function<float()> onGetCpuUsage;
    std::function<float()> onGetBpm;
    std::function<int()> onGetCurrentBar;
    std::function<double()> onGetBeatInBar;
    std::function<int()> onGetBarCount;
    std::function<bool()> onGetIsPlaying;
    std::function<bool()> onGetIsRecording;
    std::function<bool()> onGetIsRecordArmed;

    // MIDI activity callback: returns true if module had activity since last poll
    std::function<bool(int)> onGetMidiActivity;

private:
    juce::Label presetLabel;
    juce::Label bpmLabel;
    juce::Label bpmSuffix;
    juce::Label positionLabel;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::TextButton recordButton;
    juce::TextButton countInButton;
    juce::TextButton clickButton;
    juce::TextButton panicButton;
    juce::Label cpuLabel;
    juce::TextButton logoButton;
    juce::ComboBox barCountBox;
    juce::TextButton fileButton;
    juce::Label sessionLabel;
    std::array<bool, 5> midiLedState { false, false, false, false, false };

    void timerCallback() override
    {
        // Update position display
        if (onGetCurrentBar && onGetBeatInBar && onGetBarCount)
        {
            int bar = onGetCurrentBar() + 1;  // 1-based for display
            double beat = onGetBeatInBar() + 1.0;  // 1-based
            int bars = onGetBarCount();
            positionLabel.setText(juce::String(bar) + "." + juce::String(static_cast<int>(beat))
                                 + " / " + juce::String(bars),
                                 juce::dontSendNotification);
        }

        // Update BPM display (only if not being edited)
        if (onGetBpm && !bpmLabel.isBeingEdited())
        {
            int bpm = static_cast<int>(onGetBpm());
            bpmLabel.setText(juce::String(bpm), juce::dontSendNotification);
        }

        // Update transport button colours based on state
        if (onGetIsPlaying)
        {
            bool playing = onGetIsPlaying();
            playButton.setColour(juce::TextButton::buttonColourId,
                                 playing ? juce::Colour(0xFF2E7D32) : juce::Colour(0xFF1B5E20));
        }
        if (onGetIsRecording)
        {
            bool recording = onGetIsRecording();
            bool armed = onGetIsRecordArmed ? onGetIsRecordArmed() : false;
            if (recording)
                recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFB71C1C));
            else if (armed)
                recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF8B0000));
            else
                recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF5D0000));
        }

        // CPU usage
        if (onGetCpuUsage)
        {
            float pct = onGetCpuUsage() * 100.0f;
            cpuLabel.setText("CPU " + juce::String(static_cast<int>(pct)) + "%",
                             juce::dontSendNotification);
            cpuLabel.setColour(juce::Label::textColourId,
                               pct > 80.0f ? juce::Colour(Colours::accentRed)
                                           : juce::Colour(Colours::textSecondary));
        }

        // MIDI activity LEDs: poll and decay
        if (onGetMidiActivity)
        {
            for (int i = 0; i < 5; ++i)
                midiLedState[static_cast<size_t>(i)] = onGetMidiActivity(i);
            repaint(0, getHeight() - 14, 130, 14);
        }
    }
};

} // namespace axelf::ui
