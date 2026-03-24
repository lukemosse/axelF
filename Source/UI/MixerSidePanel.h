#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "AxelFColours.h"
#include "../Common/MasterMixer.h"

namespace axelf::ui
{

// ── Vertical mixer panel on the right side of the editor ──────
// 5 channel strips + master strip, each with vertical fader,
// mute/solo, pan knob, send knobs (reverb/delay), and meter.
// Master strip adds EQ/comp/limiter controls.
class MixerSidePanel : public juce::Component, private juce::Timer
{
public:
    MixerSidePanel (MasterMixer& mix,
                    juce::AudioProcessorValueTreeState& mixApvts,
                    juce::AudioProcessorValueTreeState& fxApvts)
        : mixer (mix), mixerAPVTS (mixApvts), effectsAPVTS (fxApvts)
    {
        startTimerHz (30);

        static const char* names[] = { "JUP8", "MOOG", "JX3P", "DX7", "LINN" };
        static const juce::uint32 accents[] = {
            Colours::jupiter, Colours::moog, Colours::jx3p,
            Colours::dx7, Colours::linn
        };

        for (int i = 0; i < 5; ++i)
        {
            auto& s = strips[i];
            s.name   = names[i];
            s.accent = juce::Colour (accents[i]);

            // Vertical fader
            s.fader.setSliderStyle (juce::Slider::LinearVertical);
            s.fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.fader.setRange (0.0, 1.5, 0.01);
            s.fader.setColour (juce::Slider::backgroundColourId, juce::Colour (Colours::knobTrack));
            s.fader.setColour (juce::Slider::trackColourId, s.accent.withAlpha (0.5f));
            s.fader.setColour (juce::Slider::thumbColourId, s.accent);
            addAndMakeVisible (s.fader);

            s.levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::levelID (i), s.fader);

            // Pan knob
            s.panKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.panKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.panKnob.setRange (-1.0, 1.0, 0.01);
            s.panKnob.setColour (juce::Slider::rotarySliderFillColourId, s.accent.withAlpha (0.6f));
            addAndMakeVisible (s.panKnob);

            s.panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::panID (i), s.panKnob);

            // Tilt EQ knob
            s.tiltKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.tiltKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.tiltKnob.setRange (-1.0, 1.0, 0.01);
            s.tiltKnob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFaaaa44).withAlpha (0.6f));
            s.tiltKnob.setTooltip ("Tilt EQ (left=warm, right=bright)");
            addAndMakeVisible (s.tiltKnob);

            s.tiltAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::tiltID (i), s.tiltKnob);

            // Send 1 knob (reverb)
            s.send1Knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.send1Knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.send1Knob.setRange (0.0, 1.0, 0.01);
            s.send1Knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFF5b8bcc));
            addAndMakeVisible (s.send1Knob);

            s.send1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::send1ID (i), s.send1Knob);

            // Send 2 knob (delay)
            s.send2Knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.send2Knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.send2Knob.setRange (0.0, 1.0, 0.01);
            s.send2Knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFcc8b5b));
            addAndMakeVisible (s.send2Knob);

            s.send2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::send2ID (i), s.send2Knob);

            // Send 3 knob (chorus)
            s.send3Knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.send3Knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.send3Knob.setRange (0.0, 1.0, 0.01);
            s.send3Knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFF66bbaa));
            addAndMakeVisible (s.send3Knob);

            s.send3Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::send3ID (i), s.send3Knob);

            // Send 4 knob (flanger)
            s.send4Knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.send4Knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.send4Knob.setRange (0.0, 1.0, 0.01);
            s.send4Knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFbb66cc));
            addAndMakeVisible (s.send4Knob);

            s.send4Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::send4ID (i), s.send4Knob);

            // Send 5 knob (distortion)
            s.send5Knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.send5Knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.send5Knob.setRange (0.0, 1.0, 0.01);
            s.send5Knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFcc6655));
            addAndMakeVisible (s.send5Knob);

            s.send5Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::send5ID (i), s.send5Knob);

            // Mute / Solo buttons
            s.muteBtn.setButtonText ("M");
            s.muteBtn.setClickingTogglesState (true);
            s.muteBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (Colours::accentRed));
            s.muteBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (Colours::bgControl));
            s.muteBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::textSecondary));
            s.muteBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (Colours::textPrimary));
            addAndMakeVisible (s.muteBtn);

            s.muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                mixApvts, MixerParamIDs::muteID (i), s.muteBtn);

            // Ctrl+Click mute: clear all mutes
            s.muteBtn.onClick = [this, i]()
            {
                if (updatingExclusive)
                    return;
                if (! juce::ModifierKeys::currentModifiers.isCtrlDown())
                    return;

                updatingExclusive = true;
                for (int j = 0; j < 5; ++j)
                    mixer.setMute (j, false);
                updatingExclusive = false;
            };

            s.soloBtn.setButtonText ("S");
            s.soloBtn.setClickingTogglesState (true);
            s.soloBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (Colours::accentGreen));
            s.soloBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (Colours::bgControl));
            s.soloBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::textSecondary));
            s.soloBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (Colours::textPrimary));
            addAndMakeVisible (s.soloBtn);

            s.soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                mixApvts, MixerParamIDs::soloID (i), s.soloBtn);

            // Ctrl+Click solo: clear all solos
            s.soloBtn.onClick = [this, i]()
            {
                if (updatingExclusive)
                    return;
                if (! juce::ModifierKeys::currentModifiers.isCtrlDown())
                    return;

                updatingExclusive = true;
                for (int j = 0; j < 5; ++j)
                    mixer.setSolo (j, false);
                updatingExclusive = false;
            };
        }

        // ── Master strip ─────────────────────────────────────
        masterFader.setSliderStyle (juce::Slider::LinearVertical);
        masterFader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        masterFader.setRange (0.0, 1.5, 0.01);
        masterFader.setColour (juce::Slider::backgroundColourId, juce::Colour (Colours::knobTrack));
        masterFader.setColour (juce::Slider::trackColourId, juce::Colour (Colours::mixer).withAlpha (0.6f));
        masterFader.setColour (juce::Slider::thumbColourId, juce::Colour (Colours::mixer));
        addAndMakeVisible (masterFader);

        masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            mixApvts, MixerParamIDs::kMasterLevel, masterFader);

        // ── Master EQ knobs ──────────────────────────────────
        auto initFxKnob = [&](juce::Slider& knob, const juce::String& paramID,
                              float lo, float hi, float def, juce::uint32 col)
        {
            knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            knob.setRange (lo, hi, 0.1);
            knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (col).withAlpha (0.6f));
            addAndMakeVisible (knob);
        };

        initFxKnob (eqLow,  "fx_master_eq_low",  -12.f, 12.f, 0.f, Colours::accentOrange);
        initFxKnob (eqMid,  "fx_master_eq_mid",  -12.f, 12.f, 0.f, Colours::accentGold);
        initFxKnob (eqHigh, "fx_master_eq_high", -12.f, 12.f, 0.f, Colours::accentBlue);

        eqLowAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_eq_low",  eqLow);
        eqMidAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_eq_mid",  eqMid);
        eqHighAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_eq_high", eqHigh);

        // ── Master compressor knobs ──────────────────────────
        initFxKnob (compThresh,  "fx_master_comp_threshold", -40.f, 0.f,   0.f,  Colours::accentRed);
        initFxKnob (compRatio,   "fx_master_comp_ratio",      1.f,  20.f,  1.f,  Colours::accentRed);

        compThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_comp_threshold", compThresh);
        compRatioAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_comp_ratio",     compRatio);

        // ── Reverb controls (global send 1) ──────────────────
        initFxKnob (reverbSize,    "fx_reverb_size",     0.f, 1.f,   0.5f,  0xFF5b8bcc);
        initFxKnob (reverbDamping, "fx_reverb_damping",  0.f, 1.f,   0.5f,  0xFF5b8bcc);

        reverbSizeAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_reverb_size",    reverbSize);
        reverbDampingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_reverb_damping", reverbDamping);

        // ── Delay controls (global send 2) ───────────────────
        initFxKnob (delayFeedback, "fx_delay_feedback",  0.f, 0.95f, 0.3f,  0xFFcc8b5b);
        initFxKnob (delayMix,      "fx_delay_mix",       0.f, 1.f,   0.5f,  0xFFcc8b5b);

        delayFeedbackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_delay_feedback", delayFeedback);
        delayMixAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_delay_mix",      delayMix);

        // ── Chorus controls (global send 3) ──────────────────
        initFxKnob (chorusRate,  "fx_chorus_rate",   0.1f, 10.f, 1.5f, 0xFF66bbaa);
        initFxKnob (chorusDepth, "fx_chorus_depth",  0.f,  1.f,  0.5f, 0xFF66bbaa);

        chorusRateAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_chorus_rate",  chorusRate);
        chorusDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_chorus_depth", chorusDepth);

        // ── Flanger controls (global send 4) ─────────────────
        initFxKnob (flangerRate,     "fx_flanger_rate",     0.05f, 5.f,   0.3f, 0xFFbb66cc);
        initFxKnob (flangerFeedback, "fx_flanger_feedback", 0.f,   0.95f, 0.5f, 0xFFbb66cc);

        flangerRateAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_flanger_rate",     flangerRate);
        flangerFeedbackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_flanger_feedback", flangerFeedback);

        // ── Distortion controls (global send 5) ──────────────
        initFxKnob (distDrive, "fx_distortion_drive", 0.f, 1.f, 0.3f, 0xFFcc6655);
        initFxKnob (distTone,  "fx_distortion_tone",  0.f, 1.f, 0.5f, 0xFFcc6655);

        distDriveAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_distortion_drive", distDrive);
        distToneAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_distortion_tone",  distTone);
    }

    // Two-param constructor for backward compatibility — immediately delegates
    MixerSidePanel (MasterMixer& mix, juce::AudioProcessorValueTreeState& mixApvts)
        : MixerSidePanel (mix, mixApvts, mixApvts) {}   // sends attach will fail silently (no fx_ IDs) — safe

    void setActiveModule (int idx) { activeModule = std::clamp (idx, -1, 4); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // Dark background
        g.setColour (juce::Colour (0xFF0d0f1a));
        g.fillRect (b);

        // Left separator line
        g.setColour (juce::Colour (Colours::borderSubtle).withAlpha (0.6f));
        g.fillRect (0.0f, 0.0f, 1.0f, b.getHeight());

        const int totalStrips = 8; // 5 ch + 2 FX + master
        const float stripW = b.getWidth() / static_cast<float> (totalStrips);

        // ── Channel strip labels and meters ──────────────────
        for (int i = 0; i < 5; ++i)
        {
            const float x = static_cast<float> (i) * stripW;

            // Alternating strip background + active highlight
            if (i == activeModule)
            {
                g.setColour (juce::Colour (0xFF1c2040).withAlpha (0.85f));
                g.fillRect (x, 0.0f, stripW, b.getHeight());
            }
            else if (i % 2 == 0)
            {
                g.setColour (juce::Colour (0xFF111328).withAlpha (0.4f));
                g.fillRect (x, 0.0f, stripW, b.getHeight());
            }

            // Separator between strips
            g.setColour (juce::Colour (Colours::borderSubtle).withAlpha (0.3f));
            g.fillRect (x + stripW - 1.0f, 0.0f, 1.0f, b.getHeight());

            // Channel name at top
            g.setFont (juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.setColour (strips[i].accent);
            g.drawText (strips[i].name, static_cast<int>(x), 2,
                        static_cast<int>(stripW), 14,
                        juce::Justification::centred, false);

            // Vertical peak meter (thin bar to right of fader)
            float meterX = x + stripW - 8.0f;
            float meterTop = 56.0f;
            float meterH = std::max (20.0f, sendAreaTop_ - 66.0f);
            auto meterRect = juce::Rectangle<float> (meterX, meterTop, 4.0f, meterH);
            g.setColour (juce::Colour (0xFF0a0e14));
            g.fillRoundedRectangle (meterRect, 1.0f);

            float level = juce::jlimit (0.0f, 1.0f, strips[i].peakLevel);
            float meterFillH = meterH * level;
            auto filled = juce::Rectangle<float> (meterX, meterTop + meterH - meterFillH, 4.0f, meterFillH);
            auto colour = level < 0.7f  ? juce::Colour (Colours::accentGreen)
                        : level < 0.9f  ? juce::Colour (0xFFddaa00)
                                        : juce::Colour (Colours::accentRed);
            g.setColour (colour.withAlpha (0.8f));
            g.fillRoundedRectangle (filled, 1.0f);

            // Send labels (single column, drawn next to each send knob)
            g.setFont (juce::Font(juce::FontOptions(7.0f)));
            static const char* sendNames[] = { "RVB", "DLY", "CHR", "FLG", "DST" };
            static const juce::uint32 sendCols[] = { 0xFF5b8bcc, 0xFFcc8b5b, 0xFF66bbaa, 0xFFbb66cc, 0xFFcc6655 };
            for (int s = 0; s < 5; ++s)
            {
                int sy = sendKnobYs_[i][s];
                g.setColour (juce::Colour (sendCols[s]).withAlpha (0.7f));
                g.drawText (sendNames[s], static_cast<int>(x + 1), sy,
                            sendLabelW_, sendKnobSize_, juce::Justification::centred, false);
            }
        }

        // ── FX columns (indices 5 and 6) ─────────────────────
        for (int col = 0; col < 2; ++col)
        {
            float fxX = (5.0f + col) * stripW;
            g.setColour (juce::Colour (0xFF0a0d18));
            g.fillRect (fxX, 0.0f, stripW, b.getHeight());
            g.setColour (juce::Colour (Colours::borderSubtle).withAlpha (0.3f));
            g.fillRect (fxX - 1.0f, 0.0f, 1.0f, b.getHeight());

            g.setFont (juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            g.setColour (juce::Colour (Colours::textLabel));
            g.drawText (col == 0 ? "FX-1" : "FX-2", static_cast<int>(fxX), 2,
                        static_cast<int>(stripW), 14, juce::Justification::centred, false);
        }

        // FX sub-labels and knob labels — column 1 (reverb, delay, chorus)
        {
            float fxX = 5.0f * stripW;
            float fxLabelX = fxX + 2.0f;
            float fxLabelW = stripW - 4.0f;
            g.setFont (juce::Font(juce::FontOptions(7.0f, juce::Font::bold)));

            g.setColour (juce::Colour (0xFF5b8bcc).withAlpha (0.7f));
            g.drawText ("REVERB", static_cast<int>(fxLabelX), fxReverbLabelY,
                        static_cast<int>(fxLabelW), 10, juce::Justification::centred, false);
            g.setColour (juce::Colour (0xFFcc8b5b).withAlpha (0.7f));
            g.drawText ("DELAY", static_cast<int>(fxLabelX), fxDelayLabelY,
                        static_cast<int>(fxLabelW), 10, juce::Justification::centred, false);
            g.setColour (juce::Colour (0xFF66bbaa).withAlpha (0.7f));
            g.drawText ("CHORUS", static_cast<int>(fxLabelX), fxChorusLabelY,
                        static_cast<int>(fxLabelW), 10, juce::Justification::centred, false);

            g.setFont (juce::Font(juce::FontOptions(6.5f)));
            g.setColour (juce::Colour (Colours::textSecondary).withAlpha (0.65f));
            auto lbl1 = [&](const juce::String& t, int ky) {
                g.drawText (t, static_cast<int>(fxLabelX), ky + fxKnobH - 1, static_cast<int>(fxLabelW), 9, juce::Justification::centred, false);
            };
            lbl1 ("SIZE",  fxKnobY_reverbSize);
            lbl1 ("DAMP",  fxKnobY_reverbDamp);
            lbl1 ("FDBK",  fxKnobY_delayFdbk);
            lbl1 ("MIX",   fxKnobY_delayMix);
            lbl1 ("RATE",  fxKnobY_chorusRate);
            lbl1 ("DPTH",  fxKnobY_chorusDepth);
        }

        // FX sub-labels and knob labels — column 2 (flanger, distortion)
        {
            float fxX = 6.0f * stripW;
            float fxLabelX = fxX + 2.0f;
            float fxLabelW = stripW - 4.0f;
            g.setFont (juce::Font(juce::FontOptions(7.0f, juce::Font::bold)));

            g.setColour (juce::Colour (0xFFbb66cc).withAlpha (0.7f));
            g.drawText ("FLANGER", static_cast<int>(fxLabelX), fxFlangerLabelY,
                        static_cast<int>(fxLabelW), 10, juce::Justification::centred, false);
            g.setColour (juce::Colour (0xFFcc6655).withAlpha (0.7f));
            g.drawText ("DISTORT", static_cast<int>(fxLabelX), fxDistortionLabelY,
                        static_cast<int>(fxLabelW), 10, juce::Justification::centred, false);

            g.setFont (juce::Font(juce::FontOptions(6.5f)));
            g.setColour (juce::Colour (Colours::textSecondary).withAlpha (0.65f));
            auto lbl2 = [&](const juce::String& t, int ky) {
                g.drawText (t, static_cast<int>(fxLabelX), ky + fxKnobH - 1, static_cast<int>(fxLabelW), 9, juce::Justification::centred, false);
            };
            lbl2 ("RATE",  fxKnobY_flangerRate);
            lbl2 ("FDBK",  fxKnobY_flangerFdbk);
            lbl2 ("DRVE",  fxKnobY_distDrive);
            lbl2 ("TONE",  fxKnobY_distTone);
        }

        // ── Master strip (rightmost) ─────────────────────────
        float masterX = 7.0f * stripW;
        g.setColour (juce::Colour (0xFF152540));
        g.fillRect (masterX, 0.0f, stripW, b.getHeight());
        g.setColour (juce::Colour (Colours::borderSubtle).withAlpha (0.3f));
        g.fillRect (masterX - 1.0f, 0.0f, 1.0f, b.getHeight());

        g.setFont (juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour (juce::Colour (Colours::mixer));
        g.drawText ("MST", static_cast<int>(masterX), 2,
                    static_cast<int>(stripW), 14,
                    juce::Justification::centred, false);

        // Master EQ/COMP labels
        float mstLabelX = masterX + 2.0f;
        float mstLabelW = stripW - 4.0f;
        g.setFont (juce::Font(juce::FontOptions(7.0f, juce::Font::bold)));

        g.setColour (juce::Colour (Colours::accentOrange).withAlpha (0.7f));
        g.drawText ("EQ", static_cast<int>(mstLabelX), fxEqLabelY,
                    static_cast<int>(mstLabelW), 10, juce::Justification::centred, false);
        g.setColour (juce::Colour (Colours::accentRed).withAlpha (0.7f));
        g.drawText ("COMP", static_cast<int>(mstLabelX), fxCompLabelY,
                    static_cast<int>(mstLabelW), 10, juce::Justification::centred, false);

        g.setFont (juce::Font(juce::FontOptions(6.5f)));
        g.setColour (juce::Colour (Colours::textSecondary).withAlpha (0.65f));
        auto mstLbl = [&](const juce::String& t, int ky) {
            g.drawText (t, static_cast<int>(mstLabelX), ky + fxKnobH - 1, static_cast<int>(mstLabelW), 9, juce::Justification::centred, false);
        };
        mstLbl ("LOW",  fxKnobY_eqLow);
        mstLbl ("MID",  fxKnobY_eqMid);
        mstLbl ("HIGH", fxKnobY_eqHigh);
        mstLbl ("THR",  fxKnobY_compThresh);
        mstLbl ("RAT",  fxKnobY_compRatio);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int totalStrips = 8;
        const int stripW = area.getWidth() / totalStrips;
        const int knobSize = std::min (stripW - 6, 32);

        // ── Send knob sizing ─────────────────────────────────
        sendKnobSize_ = std::min (stripW - 4, 28);
        sendLabelW_ = std::min (18, stripW / 3);
        int sendPitch = sendKnobSize_ + 2;
        int sendsH = 5 * sendPitch + 4;  // total height for 5 single-column sends

        sendAreaTop_ = static_cast<float>(area.getHeight() - sendsH);

        // ── 5 channel strips ─────────────────────────────────
        for (int i = 0; i < 5; ++i)
        {
            auto& s = strips[i];
            int x = i * stripW;
            int y = 16; // below name label

            // Pan knob
            s.panKnob.setBounds (x + (stripW - knobSize) / 2, y, knobSize, knobSize);
            y += knobSize + 2;

            // Tilt EQ knob
            s.tiltKnob.setBounds (x + (stripW - knobSize) / 2, y, knobSize, knobSize);
            y += knobSize + 2;

            // Mute / Solo side by side
            int btnW = (stripW - 6) / 2;
            int btnH = 16;
            s.muteBtn.setBounds (x + 2, y, btnW, btnH);
            s.soloBtn.setBounds (x + 2 + btnW + 2, y, btnW, btnH);
            y += btnH + 4;

            // Vertical fader (shorter — ends before send area)
            int faderBottom = area.getHeight() - sendsH - 4;
            int faderW = std::min (stripW - 14, 24);
            s.fader.setBounds (x + (stripW - faderW - 8) / 2, y, faderW, std::max (30, faderBottom - y));

            // Send knobs — single column, stacked vertically
            int sendX = x + sendLabelW_ + 1;
            int sy = area.getHeight() - sendsH + 2;
            juce::Slider* sendKnobs[] = { &s.send1Knob, &s.send2Knob, &s.send3Knob, &s.send4Knob, &s.send5Knob };
            for (int sk = 0; sk < 5; ++sk)
            {
                sendKnobYs_[i][sk] = sy;
                sendKnobs[sk]->setBounds (sendX, sy, sendKnobSize_, sendKnobSize_);
                sy += sendPitch;
            }
        }

        // ── FX column 1 (reverb, delay, chorus): index 5 ────
        {
            int x = 5 * stripW;
            int availH = area.getHeight() - 18;
            int fxKnobSize = std::min (stripW - 4, std::max (20, (availH - 3 * 12 - 2 * 2) / 6));
            fxKnobH = fxKnobSize;
            int cx = x + (stripW - fxKnobSize) / 2;
            int y = 18;
            const int labelGap = std::max (2, fxKnobSize / 4);

            fxReverbLabelY = y; y += 12;
            fxKnobY_reverbSize = y;
            reverbSize.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_reverbDamp = y;
            reverbDamping.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap + 2;

            fxDelayLabelY = y; y += 12;
            fxKnobY_delayFdbk = y;
            delayFeedback.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_delayMix = y;
            delayMix.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap + 2;

            fxChorusLabelY = y; y += 12;
            fxKnobY_chorusRate = y;
            chorusRate.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_chorusDepth = y;
            chorusDepth.setBounds (cx, y, fxKnobSize, fxKnobSize);
        }

        // ── FX column 2 (flanger, distortion): index 6 ──────
        {
            int x = 6 * stripW;
            int availH = area.getHeight() - 18;
            int fxKnobSize = std::min (stripW - 4, std::max (20, (availH - 2 * 12 - 1 * 2) / 4));
            fxKnobH = std::min (fxKnobH, fxKnobSize);  // keep consistent
            int cx = x + (stripW - fxKnobSize) / 2;
            int y = 18;
            const int labelGap = std::max (2, fxKnobSize / 4);

            fxFlangerLabelY = y; y += 12;
            fxKnobY_flangerRate = y;
            flangerRate.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_flangerFdbk = y;
            flangerFeedback.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap + 2;

            fxDistortionLabelY = y; y += 12;
            fxKnobY_distDrive = y;
            distDrive.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_distTone = y;
            distTone.setBounds (cx, y, fxKnobSize, fxKnobSize);
        }

        // ── Master strip (rightmost, index 7) ────────────────
        {
            int x = 7 * stripW;
            int y = 16;
            int availH = area.getHeight() - 18;
            // Match FX column knob sizes for consistency
            int mstKnobSize = std::min (stripW - 4, std::max (20, (availH - 2 * 14 - 5 * 2) / 5));
            fxKnobH = std::min (fxKnobH, mstKnobSize);
            int cx = x + (stripW - mstKnobSize) / 2;
            const int labelH = 12;
            const int labelGap = std::max (2, mstKnobSize / 5);

            // EQ section at top of master strip
            fxEqLabelY = y; y += labelH + 2;
            fxKnobY_eqLow = y;
            eqLow.setBounds (cx, y, mstKnobSize, mstKnobSize); y += mstKnobSize + labelGap;
            fxKnobY_eqMid = y;
            eqMid.setBounds (cx, y, mstKnobSize, mstKnobSize); y += mstKnobSize + labelGap;
            fxKnobY_eqHigh = y;
            eqHigh.setBounds (cx, y, mstKnobSize, mstKnobSize); y += mstKnobSize + labelGap + 4;

            // COMP section
            fxCompLabelY = y; y += labelH + 2;
            fxKnobY_compThresh = y;
            compThresh.setBounds (cx, y, mstKnobSize, mstKnobSize); y += mstKnobSize + labelGap;
            fxKnobY_compRatio = y;
            compRatio.setBounds (cx, y, mstKnobSize, mstKnobSize); y += mstKnobSize + labelGap + 4;

            // Master fader (fills remaining height)
            int faderBottom = area.getHeight() - 8;
            int faderW = std::min (stripW - 10, 24);
            masterFader.setBounds (x + (stripW - faderW) / 2, y, faderW, std::max (30, faderBottom - y));
        }
    }

private:
    void timerCallback() override
    {
        for (int i = 0; i < 5; ++i)
            strips[i].peakLevel = mixer.getPeakLevel (i);
        repaint();
    }

    MasterMixer& mixer;
    juce::AudioProcessorValueTreeState& mixerAPVTS;
    juce::AudioProcessorValueTreeState& effectsAPVTS;
    int activeModule = 0;
    bool updatingExclusive = false;

    // ── Per-channel strips ───────────────────────────────────
    struct StripUI
    {
        juce::String   name;
        juce::Colour   accent;
        juce::Slider   fader;
        juce::Slider   panKnob;
        juce::Slider   tiltKnob;
        juce::Slider   send1Knob;    // reverb send
        juce::Slider   send2Knob;    // delay send
        juce::Slider   send3Knob;    // chorus send
        juce::Slider   send4Knob;    // flanger send
        juce::Slider   send5Knob;    // distortion send
        juce::TextButton muteBtn;
        juce::TextButton soloBtn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tiltAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send1Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send2Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send3Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send4Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send5Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
        float peakLevel = 0.0f;
    };

    StripUI strips[5];

    // ── Master strip ─────────────────────────────────────────
    juce::Slider masterFader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;

    // ── FX controls ──────────────────────────────────────────
    juce::Slider reverbSize, reverbDamping;
    juce::Slider delayFeedback, delayMix;
    juce::Slider chorusRate, chorusDepth;
    juce::Slider flangerRate, flangerFeedback;
    juce::Slider distDrive, distTone;
    juce::Slider eqLow, eqMid, eqHigh;
    juce::Slider compThresh, compRatio;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbSizeAttach, reverbDampingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayFeedbackAttach, delayMixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusRateAttach, chorusDepthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flangerRateAttach, flangerFeedbackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distDriveAttach, distToneAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqLowAttach, eqMidAttach, eqHighAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> compThreshAttach, compRatioAttach;

    // Label Y positions computed in resized(), used in paint()
    int fxReverbLabelY = 18, fxDelayLabelY = 80, fxChorusLabelY = 142;
    int fxFlangerLabelY = 200, fxDistortionLabelY = 258;
    int fxEqLabelY = 16, fxCompLabelY = 100;
    int fxKnobH = 28;
    int fxKnobY_reverbSize = 0, fxKnobY_reverbDamp = 0;
    int fxKnobY_delayFdbk = 0, fxKnobY_delayMix = 0;
    int fxKnobY_chorusRate = 0, fxKnobY_chorusDepth = 0;
    int fxKnobY_flangerRate = 0, fxKnobY_flangerFdbk = 0;
    int fxKnobY_distDrive = 0, fxKnobY_distTone = 0;
    int fxKnobY_eqLow = 0, fxKnobY_eqMid = 0, fxKnobY_eqHigh = 0;
    int fxKnobY_compThresh = 0, fxKnobY_compRatio = 0;

    // Send knob positions (per channel)
    float sendAreaTop_ = 400.0f;
    int sendKnobSize_ = 24;
    int sendLabelW_ = 16;
    int sendKnobYs_[5][5] = {};
};

} // namespace axelf::ui
