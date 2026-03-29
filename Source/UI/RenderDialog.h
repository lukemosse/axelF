#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "AxelFColours.h"

// Forward declare — full include only needed in implementation
namespace axelf { class AxelFProcessor; }

namespace axelf::ui
{

// ─────────────────────────────────────────────────────────────
//  OfflineRenderThread — drives processBlock in a tight loop,
//  writes stereo mix and optional per-module tracks to disk.
// ─────────────────────────────────────────────────────────────

class OfflineRenderThread : public juce::Thread
{
public:
    struct Options
    {
        bool         individualTracks = false;
        int          bitDepth         = 24;
        bool         floatFormat      = false;
        bool         useAiff          = false;
        double       sampleRate       = 48000.0;
        int          tailBars         = 2;
        bool         normalize        = false;
        juce::File   outputFile;          // Full Mix mode
        juce::File   outputDir;           // Individual Tracks mode
        juce::String baseName;
    };

    OfflineRenderThread(AxelFProcessor& proc, const Options& opts)
        : juce::Thread("AxelF-Render"), processor(proc), options(opts) {}

    void run() override;

    std::atomic<double> progress { 0.0 };
    std::atomic<bool>   succeeded { false };
    juce::String        errorMessage;

private:
    AxelFProcessor& processor;
    Options options;
};


// ─────────────────────────────────────────────────────────────
//  RenderDialog — overlay component (same pattern as AboutOverlay)
// ─────────────────────────────────────────────────────────────

class RenderDialog : public juce::Component,
                     private juce::Timer
{
public:
    explicit RenderDialog(AxelFProcessor& proc)
        : processor(proc)
    {
        setInterceptsMouseClicks(true, true);

        // ── Title close button ───────────────────────────
        closeButton.setButtonText(juce::String::charToString(0x2715));  // ✕
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000));
        closeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        closeButton.onClick = [this] { dismiss(); };
        addAndMakeVisible(closeButton);

        // ── Output mode toggle ───────────────────────────
        fullMixButton.setButtonText("Full Mix");
        fullMixButton.setRadioGroupId(9001);
        fullMixButton.setClickingTogglesState(true);
        fullMixButton.setToggleState(true, juce::dontSendNotification);
        styleToggle(fullMixButton);
        fullMixButton.onClick = [this] { updateSummary(); };
        addAndMakeVisible(fullMixButton);

        tracksButton.setButtonText("Individual Tracks");
        tracksButton.setRadioGroupId(9001);
        tracksButton.setClickingTogglesState(true);
        styleToggle(tracksButton);
        tracksButton.onClick = [this] { updateSummary(); updatePathLabel(); };
        addAndMakeVisible(tracksButton);

        // ── Format combo ─────────────────────────────────
        formatCombo.addItem("WAV 16-bit", 1);
        formatCombo.addItem("WAV 24-bit", 2);
        formatCombo.addItem("WAV 32-bit float", 3);
        formatCombo.addItem("AIFF 24-bit", 4);
        formatCombo.setSelectedId(2, juce::dontSendNotification);
        styleCombo(formatCombo);
        addAndMakeVisible(formatCombo);

        // ── Sample rate combo ────────────────────────────
        rateCombo.addItem("44100 Hz", 1);
        rateCombo.addItem("48000 Hz", 2);
        rateCombo.addItem("88200 Hz", 3);
        rateCombo.addItem("96000 Hz", 4);
        rateCombo.setSelectedId(2, juce::dontSendNotification);
        styleCombo(rateCombo);
        addAndMakeVisible(rateCombo);

        // ── Tail combo ───────────────────────────────────
        tailCombo.addItem("None (+0)", 1);
        tailCombo.addItem("+1 bar", 2);
        tailCombo.addItem("+2 bars", 3);
        tailCombo.addItem("+4 bars", 4);
        tailCombo.setSelectedId(3, juce::dontSendNotification);
        styleCombo(tailCombo);
        tailCombo.onChange = [this] { updateSummary(); };
        addAndMakeVisible(tailCombo);

        // ── Normalize checkbox ───────────────────────────
        normalizeToggle.setButtonText("Normalize output");
        normalizeToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(Colours::textLabel));
        normalizeToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(Colours::accentBlue));
        addAndMakeVisible(normalizeToggle);

        // ── Summary label ────────────────────────────────
        summaryLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textPrimary));
        summaryLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        summaryLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(summaryLabel);

        // ── Path label + Browse ──────────────────────────
        pathLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textPrimary));
        pathLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        pathLabel.setJustificationType(juce::Justification::centredLeft);
        pathLabel.setMinimumHorizontalScale(0.5f);
        addAndMakeVisible(pathLabel);

        browseButton.setButtonText("Browse...");
        browseButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        browseButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
        browseButton.onClick = [this] { browseForPath(); };
        addAndMakeVisible(browseButton);

        // ── Progress ─────────────────────────────────────
        progressBar = std::make_unique<juce::ProgressBar>(progressValue);
        progressBar->setColour(juce::ProgressBar::foregroundColourId, juce::Colour(Colours::accentGreen));
        progressBar->setColour(juce::ProgressBar::backgroundColourId, juce::Colour(Colours::bgSection));
        progressBar->setVisible(false);
        addAndMakeVisible(*progressBar);

        statusLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textPrimary));
        statusLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setVisible(false);
        addAndMakeVisible(statusLabel);

        // ── Error label ──────────────────────────────────
        errorLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::accentRed));
        errorLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        errorLabel.setJustificationType(juce::Justification::centredLeft);
        errorLabel.setVisible(false);
        addAndMakeVisible(errorLabel);

        // ── Action buttons ───────────────────────────────
        cancelButton.setButtonText("Cancel");
        cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        cancelButton.onClick = [this] { onCancel(); };
        addAndMakeVisible(cancelButton);

        renderButton.setButtonText("Render  \xe2\x96\xb6");
        renderButton.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::accentGreen));
        renderButton.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
        renderButton.onClick = [this] { onRender(); };
        addAndMakeVisible(renderButton);
    }

    ~RenderDialog() override
    {
        stopTimer();
        if (renderThread != nullptr)
        {
            renderThread->signalThreadShouldExit();
            renderThread->waitForThreadToExit(5000);
        }
    }

    // Reset to idle state, called before showing
    void reset();

    void paint(juce::Graphics& g) override
    {
        // Semi-transparent backdrop
        g.fillAll(juce::Colour(0xCC000000));

        auto area = getLocalBounds().toFloat();
        auto box = area.withSizeKeepingCentre(kCardW, kCardH);

        // Card background
        g.setColour(juce::Colour(0xFF1a1a30));
        g.fillRoundedRectangle(box, 8.0f);
        g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.5f));
        g.drawRoundedRectangle(box, 8.0f, 1.5f);

        auto inner = box.reduced(kPad);

        // Title
        g.setColour(juce::Colour(Colours::accentGold));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText("RENDER TO FILE", inner.removeFromTop(22.0f), juce::Justification::centredLeft);

        inner.removeFromTop(4.0f);

        // Separator
        g.setColour(juce::Colour(Colours::borderSubtle));
        g.fillRect(inner.removeFromTop(1.0f));

        inner.removeFromTop(10.0f);

        // Plugin warning banner
    #if !JucePlugin_Build_Standalone
        auto bannerArea = inner.removeFromTop(24.0f);
        g.setColour(juce::Colour(Colours::bgSection));
        g.fillRoundedRectangle(bannerArea, 4.0f);
        g.setColour(juce::Colour(Colours::accentOrange));
        g.fillRect(bannerArea.removeFromLeft(3.0f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(juce::Colour(Colours::textSecondary));
        g.drawText(juce::String::charToString(0x26A0) + "  Stop your DAW before rendering for best results.",
                   bannerArea.reduced(8.0f, 0.0f), juce::Justification::centredLeft);
        inner.removeFromTop(8.0f);
    #endif

        // Section labels
        auto drawLabel = [&](juce::Rectangle<float> r, const juce::String& text)
        {
            g.setColour(juce::Colour(Colours::textSecondary));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText(text, r, juce::Justification::centredLeft);
        };

        // OUTPUT label (above the toggle buttons)
        drawLabel(inner.removeFromTop(16.0f), "OUTPUT");
        inner.removeFromTop(28.0f + 8.0f);  // skip toggle buttons + gap

        // FORMAT and SAMPLE RATE labels
        auto formatRow = inner.removeFromTop(16.0f);
        drawLabel(formatRow.removeFromLeft(formatRow.getWidth() * 0.5f), "FORMAT");
        drawLabel(formatRow, "SAMPLE RATE");
        inner.removeFromTop(28.0f + 8.0f);  // skip combos + gap

        // TAIL and Normalize row label
        auto tailRow = inner.removeFromTop(16.0f);
        drawLabel(tailRow.removeFromLeft(tailRow.getWidth() * 0.5f), "TAIL");
        inner.removeFromTop(28.0f + 8.0f);

        // Summary line — skip (drawn by label component)
        inner.removeFromTop(20.0f + 8.0f);

        // OUTPUT PATH label
        drawLabel(inner.removeFromTop(16.0f), "OUTPUT PATH");
    }

    void resized() override
    {
        auto area = getLocalBounds().toFloat();
        auto box = area.withSizeKeepingCentre(kCardW, kCardH);
        auto inner = box.reduced(kPad).toNearestInt();

        // Title row + close button
        auto titleRow = inner.removeFromTop(22);
        closeButton.setBounds(titleRow.removeFromRight(22));

        inner.removeFromTop(4 + 1 + 10);  // gap + sep + gap

    #if !JucePlugin_Build_Standalone
        inner.removeFromTop(24 + 8);  // banner + gap
    #endif

        // OUTPUT label
        inner.removeFromTop(16);

        // Toggle buttons
        auto toggleRow = inner.removeFromTop(28);
        fullMixButton.setBounds(toggleRow.removeFromLeft(toggleRow.getWidth() / 2 - 4));
        toggleRow.removeFromLeft(8);
        tracksButton.setBounds(toggleRow);
        inner.removeFromTop(8);

        // FORMAT label
        inner.removeFromTop(16);

        // Format + rate combos
        auto comboRow = inner.removeFromTop(28);
        auto halfW = comboRow.getWidth() / 2 - 4;
        formatCombo.setBounds(comboRow.removeFromLeft(halfW));
        comboRow.removeFromLeft(8);
        rateCombo.setBounds(comboRow);
        inner.removeFromTop(8);

        // TAIL label
        inner.removeFromTop(16);

        // Tail + normalize
        auto tailRow = inner.removeFromTop(28);
        tailCombo.setBounds(tailRow.removeFromLeft(halfW));
        tailRow.removeFromLeft(8);
        normalizeToggle.setBounds(tailRow);
        inner.removeFromTop(8);

        // Summary
        summaryLabel.setBounds(inner.removeFromTop(20));
        inner.removeFromTop(8);

        // OUTPUT PATH label
        inner.removeFromTop(16);

        // Path row
        auto pathRow = inner.removeFromTop(24);
        browseButton.setBounds(pathRow.removeFromRight(80));
        pathRow.removeFromRight(6);
        pathLabel.setBounds(pathRow);
        inner.removeFromTop(8);

        // Separator
        inner.removeFromTop(1 + 8);

        // Progress + status (overlap area — positioned but may be invisible)
        progressBar->setBounds(inner.removeFromTop(14));
        inner.removeFromTop(4);

        auto statusRow = inner.removeFromTop(16);
        errorLabel.setBounds(statusRow);
        statusLabel.setBounds(statusRow);
        inner.removeFromTop(8);

        // Buttons
        auto btnRow = inner.removeFromTop(32);
        renderButton.setBounds(btnRow.removeFromRight(120));
        btnRow.removeFromRight(8);
        cancelButton.setBounds(btnRow.removeFromRight(90));
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Click outside the card → dismiss (only if idle or done)
        auto area = getLocalBounds().toFloat();
        auto box = area.withSizeKeepingCentre(kCardW, kCardH);
        if (!box.contains(e.position) && state != State::Rendering)
            dismiss();
    }

private:
    AxelFProcessor& processor;

    static constexpr float kCardW = 520.0f;
    static constexpr float kCardH = 420.0f;
    static constexpr float kPad   = 24.0f;

    enum class State { Idle, Rendering, Done };
    State state = State::Idle;

    // Controls
    juce::TextButton closeButton;
    juce::TextButton fullMixButton, tracksButton;
    juce::ComboBox   formatCombo, rateCombo, tailCombo;
    juce::ToggleButton normalizeToggle;
    juce::Label      summaryLabel, pathLabel;
    juce::TextButton browseButton;
    std::unique_ptr<juce::ProgressBar> progressBar;
    juce::Label      statusLabel, errorLabel;
    juce::TextButton cancelButton, renderButton;

    double progressValue = 0.0;
    juce::File chosenPath;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<OfflineRenderThread> renderThread;

    // ── Helpers ──────────────────────────────────────────

    void styleToggle(juce::TextButton& btn)
    {
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Colours::accentBlue));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        btn.setColour(juce::TextButton::textColourOnId, juce::Colour(Colours::textPrimary));
    }

    void styleCombo(juce::ComboBox& cb)
    {
        cb.setColour(juce::ComboBox::backgroundColourId, juce::Colour(Colours::bgControl));
        cb.setColour(juce::ComboBox::textColourId, juce::Colour(Colours::textPrimary));
        cb.setColour(juce::ComboBox::outlineColourId, juce::Colour(Colours::borderSubtle));
    }

    void dismiss()
    {
        if (state == State::Rendering) return;
        stopTimer();
        setVisible(false);
    }

    // ── Summary / path updates ───────────────────────────

    void updateSummary();
    void updatePathLabel();

    // ── Actions ──────────────────────────────────────────

    void browseForPath();
    void onCancel();
    void onRender();
    void startRender();

    void timerCallback() override
    {
        if (renderThread == nullptr) return;

        progressValue = renderThread->progress.load();
        int pct = static_cast<int>(progressValue * 100.0);
        statusLabel.setText("Rendering... " + juce::String(pct) + "%",
                            juce::dontSendNotification);

        if (!renderThread->isThreadRunning())
        {
            stopTimer();
            onRenderFinished();
        }

        repaint();
    }

    void onRenderFinished()
    {
        state = State::Done;
        progressBar->setVisible(true);

        if (renderThread->succeeded.load())
        {
            progressValue = 1.0;
            auto name = chosenPath.getFileName();
            if (tracksButton.getToggleState())
                name = chosenPath.getFileName() + " (folder)";
            statusLabel.setText("Done!  " + juce::String::charToString(0x2192) + "  " + name,
                                juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::accentGreen));
            statusLabel.setVisible(true);
            errorLabel.setVisible(false);
            renderButton.setButtonText("Render Again");
            renderButton.setEnabled(true);
        }
        else
        {
            statusLabel.setVisible(false);
            errorLabel.setText(renderThread->errorMessage, juce::dontSendNotification);
            errorLabel.setVisible(true);
            renderButton.setButtonText("Render  \xe2\x96\xb6");
            renderButton.setEnabled(true);
        }

        cancelButton.setButtonText("Close");
        renderThread.reset();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RenderDialog)
};

} // namespace axelf::ui
