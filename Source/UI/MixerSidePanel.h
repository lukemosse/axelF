#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "AxelFColours.h"
#include "ConsoleBackPanel.h"
#include "../Common/MasterMixer.h"
#include "../Common/ExternalPluginSlot.h"

namespace axelf::ui
{

// ── Vertical mixer panel on the right side of the editor ──────
// 6 channel strips + master strip, each with vertical fader,
// mute/solo, pan knob, send knobs (reverb/delay), and meter.
// Master strip adds EQ/comp/limiter controls.
class MixerSidePanel : public juce::Component, private juce::Timer
{
public:
    MixerSidePanel (MasterMixer& mix,
                    juce::AudioProcessorValueTreeState& mixApvts,
                    juce::AudioProcessorValueTreeState& fxApvts,
                    ExternalPluginSlot* slot0 = nullptr,
                    ExternalPluginSlot* slot1 = nullptr)
        : mixer (mix), mixerAPVTS (mixApvts), effectsAPVTS (fxApvts),
          insertSlot { slot0, slot1 }
    {
        startTimerHz (30);

        static const char* names[] = { "JUP8", "MOOG", "JX3P", "DX7", "PPG", "LINN" };
        static const juce::uint32 accents[] = {
            Colours::jupiter, Colours::moog, Colours::jx3p,
            Colours::dx7, Colours::ppgwave, Colours::linn
        };

        for (int i = 0; i < 6; ++i)
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

            // Send 6 knob (insert 1)
            s.send6Knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.send6Knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.send6Knob.setRange (0.0, 1.0, 0.01);
            s.send6Knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFF44ccaa));
            addAndMakeVisible (s.send6Knob);

            s.send6Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::send6ID (i), s.send6Knob);

            // Send 7 knob (insert 2)
            s.send7Knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.send7Knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.send7Knob.setRange (0.0, 1.0, 0.01);
            s.send7Knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFFcc77aa));
            addAndMakeVisible (s.send7Knob);

            s.send7Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::send7ID (i), s.send7Knob);

            // Reverb depth (predelay) combo — 4 discrete levels
            s.depthBox.addItemList ({ "Close", "Near", "Mid", "Far" }, 1);
            s.depthBox.setJustificationType (juce::Justification::centred);
            s.depthBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF0e1224));
            s.depthBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xFF5b8bcc).withAlpha (0.4f));
            s.depthBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xFF5b8bcc));
            s.depthBox.setTooltip ("Reverb depth: Close / Near / Mid / Far");
            addAndMakeVisible (s.depthBox);

            s.depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                mixApvts, MixerParamIDs::send1PredelayID (i), s.depthBox);

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
                for (int j = 0; j < 6; ++j)
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
                for (int j = 0; j < 6; ++j)
                    mixer.setSolo (j, false);
                updatingExclusive = false;
            };

            // Phase invert button
            s.phaseBtn.setButtonText (juce::CharPointer_UTF8 ("\xc3\x98"));  // Ø
            s.phaseBtn.setClickingTogglesState (true);
            s.phaseBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (Colours::accentOrange));
            s.phaseBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (Colours::bgControl));
            s.phaseBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::textSecondary));
            s.phaseBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (Colours::textPrimary));
            s.phaseBtn.setTooltip ("Phase Invert");
            addAndMakeVisible (s.phaseBtn);

            s.phaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                mixApvts, MixerParamIDs::phaseID (i), s.phaseBtn);

            // HPF combo (Off / 40 / 80 / 120)
            s.hpfBox.addItemList ({ "Off", "40", "80", "120" }, 1);
            s.hpfBox.setJustificationType (juce::Justification::centred);
            s.hpfBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF0e1224));
            s.hpfBox.setColour (juce::ComboBox::outlineColourId, s.accent.withAlpha (0.4f));
            s.hpfBox.setColour (juce::ComboBox::textColourId, juce::Colour (Colours::textPrimary));
            s.hpfBox.setTooltip ("High-pass filter: Off / 40 / 80 / 120 Hz");
            addAndMakeVisible (s.hpfBox);

            s.hpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                mixApvts, MixerParamIDs::hpfID (i), s.hpfBox);

            // Saturation amount knob
            s.satKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.satKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.satKnob.setRange (0.0, 1.0, 0.01);
            s.satKnob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (Colours::accentOrange).withAlpha (0.6f));
            s.satKnob.setTooltip ("Saturation amount");
            addAndMakeVisible (s.satKnob);

            s.satAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                mixApvts, MixerParamIDs::satAmountID (i), s.satKnob);
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

        // ── Master width + parallel crush knobs ──────────────
        initFxKnob (widthKnob,    "fx_master_width",           0.f, 1.5f, 1.f,  Colours::accentBlue);
        initFxKnob (parallelKnob, "fx_master_parallel_blend",  0.f, 1.f,  0.f,  Colours::accentRed);

        widthAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_width",          widthKnob);
        parallelAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_parallel_blend", parallelKnob);

        // ── Console back-panel gear button ────────────────────
        consoleGearBtn.setButtonText ("CONSOLE");
        consoleGearBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF1a2530));
        consoleGearBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::accentGold));
        consoleGearBtn.setTooltip ("Open console back-panel settings");
        consoleGearBtn.onClick = [this]() { backPanel.open(); };
        addAndMakeVisible (consoleGearBtn);

        // Back panel overlay (starts hidden)
        addChildComponent (backPanel);

        // ── Reverb controls (global send 1) ──────────────────
        initFxKnob (reverbSize,    "fx_reverb_size",     0.f, 1.f,   0.5f,  0xFF5b8bcc);
        initFxKnob (reverbDamping, "fx_reverb_damping",  0.f, 1.f,   0.5f,  0xFF5b8bcc);

        reverbSizeAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_reverb_size",    reverbSize);
        reverbDampingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_reverb_damping", reverbDamping);

        // ── Delay controls (global send 2) ───────────────────
        initFxKnob (delayTime,     "fx_delay_time_l",    1.f, 2000.f, 375.f,  0xFFcc8b5b);
        initFxKnob (delayFeedback, "fx_delay_feedback",  0.f, 1.f,   0.5f,  0xFFcc8b5b);
        initFxKnob (delayMix,      "fx_delay_mix",       0.f, 1.f,   0.5f,  0xFFcc8b5b);
        initFxKnob (delayDensity,  "fx_delay_density",   0.f, 1.f,   0.5f,  0xFFcc8b5b);
        initFxKnob (delayWarp,     "fx_delay_warp",      0.f, 1.f,   0.0f,  0xFFcc8b5b);

        delayTimeAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_delay_time_l",   delayTime);
        delayFeedbackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_delay_feedback", delayFeedback);
        delayMixAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_delay_mix",      delayMix);
        delayDensityAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_delay_density",  delayDensity);
        delayWarpAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_delay_warp",     delayWarp);

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

        // ── External insert slot buttons ─────────────────────
        for (int i = 0; i < 2; ++i)
        {
            auto& ins = insUI[i];
            ins.loadBtn.setButtonText ("Load");
            ins.loadBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF1a2530));
            ins.loadBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::textLabel));
            addAndMakeVisible (ins.loadBtn);

            ins.bypassBtn.setButtonText ("BYP");
            ins.bypassBtn.setClickingTogglesState (true);
            ins.bypassBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (Colours::accentOrange));
            ins.bypassBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (Colours::bgControl));
            ins.bypassBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::textSecondary));
            ins.bypassBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (Colours::textPrimary));
            addAndMakeVisible (ins.bypassBtn);

            ins.editBtn.setButtonText ("Edit");
            ins.editBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF1a2530));
            ins.editBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::textLabel));
            addAndMakeVisible (ins.editBtn);

            ins.nameLabel.setJustificationType (juce::Justification::centred);
            ins.nameLabel.setColour (juce::Label::textColourId, juce::Colour (Colours::textPrimary));
            ins.nameLabel.setFont (juce::Font (juce::FontOptions (8.0f)));
            ins.nameLabel.setText ("Empty", juce::dontSendNotification);
            addAndMakeVisible (ins.nameLabel);

            const int slotIdx = i;
            ins.loadBtn.onClick = [this, slotIdx]() { loadPluginForSlot (slotIdx); };
            ins.editBtn.onClick = [this, slotIdx]()
            {
                if (insertSlot[slotIdx] != nullptr)
                    insertSlot[slotIdx]->showEditor();
            };
            ins.bypassBtn.onClick = [this, slotIdx]()
            {
                if (insertSlot[slotIdx] != nullptr)
                    insertSlot[slotIdx]->setBypassed (insUI[slotIdx].bypassBtn.getToggleState());
            };
        }
    }

    // Two-param constructor for backward compatibility — immediately delegates
    MixerSidePanel (MasterMixer& mix, juce::AudioProcessorValueTreeState& mixApvts)
        : MixerSidePanel (mix, mixApvts, mixApvts) {}   // sends attach will fail silently (no fx_ IDs) — safe

    void setActiveModule (int idx) { activeModule = std::clamp (idx, -1, 5); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // Dark background
        g.setColour (juce::Colour (0xFF0d0f1a));
        g.fillRect (b);

        // Left separator line
        g.setColour (juce::Colour (Colours::borderSubtle).withAlpha (0.6f));
        g.fillRect (0.0f, 0.0f, 1.0f, b.getHeight());

        const int totalStrips = 9; // 6 ch + 2 FX + master
        const float stripW = b.getWidth() / static_cast<float> (totalStrips);

        // ── Channel strip labels and meters ──────────────────
        for (int i = 0; i < 6; ++i)
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

            // HPF / SAT labels above their controls
            g.setFont (juce::Font(juce::FontOptions(6.5f)));
            g.setColour (juce::Colour (Colours::textSecondary).withAlpha (0.6f));
            g.drawText ("SAT", static_cast<int>(x), satLabelY_[i],
                        static_cast<int>(stripW), 9, juce::Justification::centred, false);
            g.drawText ("HPF", static_cast<int>(x), hpfLabelY_[i],
                        static_cast<int>(stripW), 9, juce::Justification::centred, false);

            // Send labels (single column, drawn next to each send knob)
            g.setFont (juce::Font(juce::FontOptions(7.0f)));
            static const char* sendNames[] = { "RVB", "DLY", "CHR", "FLG", "DST", "IN1", "IN2" };
            static const juce::uint32 sendCols[] = { 0xFF5b8bcc, 0xFFcc8b5b, 0xFF66bbaa, 0xFFbb66cc, 0xFFcc6655, 0xFF44ccaa, 0xFFcc77aa };
            for (int s = 0; s < 7; ++s)
            {
                int sy = sendKnobYs_[i][s];
                g.setColour (juce::Colour (sendCols[s]).withAlpha (0.7f));
                g.drawText (sendNames[s], static_cast<int>(x + 1), sy,
                            sendLabelW_, sendKnobSize_, juce::Justification::centred, false);
            }
        }

        // ── FX columns (indices 6 and 7) ─────────────────
        for (int col = 0; col < 2; ++col)
        {
            float fxX = (6.0f + col) * stripW;
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
            float fxX = 6.0f * stripW;
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
            lbl1 ("TIME",  fxKnobY_delayTime);
            lbl1 ("FDBK",  fxKnobY_delayFdbk);
            lbl1 ("MIX",   fxKnobY_delayMix);
            lbl1 ("DENS",  fxKnobY_delayDensity);
            lbl1 ("WARP",  fxKnobY_delayWarp);
            lbl1 ("RATE",  fxKnobY_chorusRate);
            lbl1 ("DPTH",  fxKnobY_chorusDepth);
        }

        // FX sub-labels and knob labels — column 2 (flanger, distortion)
        {
            float fxX = 7.0f * stripW;
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

        // ── Insert slot labels (below FX columns) ────────────
        for (int i = 0; i < 2; ++i)
        {
            float fxX = (6.0f + i) * stripW;
            float fxLabelX = fxX + 2.0f;
            float fxLabelW = stripW - 4.0f;
            g.setFont (juce::Font (juce::FontOptions (7.0f, juce::Font::bold)));
            g.setColour (juce::Colour (0xFF44ccaa).withAlpha (0.7f));
            g.drawText (i == 0 ? "INS 1" : "INS 2",
                        static_cast<int>(fxLabelX), insLabelY_[i],
                        static_cast<int>(fxLabelW), 10,
                        juce::Justification::centred, false);
        }

        // ── Master strip (rightmost) ─────────────────────────
        float masterX = 8.0f * stripW;
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

        g.setColour (juce::Colour (Colours::accentBlue).withAlpha (0.7f));
        g.drawText ("BUS", static_cast<int>(mstLabelX), fxWidthLabelY,
                    static_cast<int>(mstLabelW), 10, juce::Justification::centred, false);
        g.setFont (juce::Font(juce::FontOptions(6.5f)));
        g.setColour (juce::Colour (Colours::textSecondary).withAlpha (0.65f));
        mstLbl ("WDTH", fxKnobY_width);
        mstLbl ("CRSH", fxKnobY_parallel);

        // ── Master VU meter ──────────────────────────────────
        {
            float meterW = 6.0f;
            auto meterRect = juce::Rectangle<float> (masterMeterX_, static_cast<float> (masterMeterY_),
                                                     meterW, static_cast<float> (masterMeterH_));
            g.setColour (juce::Colour (0xFF0a0e14));
            g.fillRoundedRectangle (meterRect, 2.0f);

            float level = juce::jlimit (0.0f, 1.0f, masterPeakLevel_);
            float fillH = static_cast<float> (masterMeterH_) * level;
            auto filled = juce::Rectangle<float> (masterMeterX_,
                                                   static_cast<float> (masterMeterY_) + static_cast<float> (masterMeterH_) - fillH,
                                                   meterW, fillH);
            auto colour = level < 0.7f  ? juce::Colour (Colours::accentGreen)
                        : level < 0.9f  ? juce::Colour (0xFFddaa00)
                                        : juce::Colour (Colours::accentRed);
            g.setColour (colour.withAlpha (0.85f));
            g.fillRoundedRectangle (filled, 2.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int totalStrips = 9;
        const int stripW = area.getWidth() / totalStrips;
        // Match channel-strip knobs to FX/Master knob sizing
        const int availH_all = area.getHeight() - 18;
        const int knobSize = std::min (stripW - 4, std::max (20, (availH_all - 3 * 12 - 2 * 2) / 6));

        // ── Send knob sizing ─────────────────────────────────
        sendKnobSize_ = knobSize;   // match pan/tilt knob size
        sendLabelW_ = std::min (18, stripW / 3);
        int sendPitch = sendKnobSize_ + 2;
        int depthBoxH = 16;         // height of the depth combo box
        int sendsH = 7 * sendPitch + depthBoxH + 6;  // 7 sends + depth combo

        sendAreaTop_ = static_cast<float>(area.getHeight() - sendsH);

        // ── 6 channel strips ─────────────────────────────
        for (int i = 0; i < 6; ++i)
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

            // Saturation knob
            satLabelY_[i] = y - 1;
            s.satKnob.setBounds (x + (stripW - knobSize) / 2, y, knobSize, knobSize);
            y += knobSize + 2;

            // HPF combo
            hpfLabelY_[i] = y - 1;
            s.hpfBox.setBounds (x + 2, y, stripW - 4, 18);
            y += 20;

            // Phase / Mute / Solo — full-width stacked rows
            int btnW = stripW - 4;
            int btnH = 22;
            int phaseH = 18;
            s.phaseBtn.setBounds (x + 2, y, btnW, phaseH);
            y += phaseH + 2;
            s.muteBtn.setBounds (x + 2, y, btnW, btnH);
            y += btnH + 2;
            s.soloBtn.setBounds (x + 2, y, btnW, btnH);
            y += btnH + 4;

            // Vertical fader (shorter — ends before send area)
            int faderBottom = area.getHeight() - sendsH - 4;
            int faderW = std::min (stripW - 14, 24);
            s.fader.setBounds (x + (stripW - faderW - 8) / 2, y, faderW, std::max (30, faderBottom - y));

            // Send knobs — single column, stacked vertically
            int sendX = x + sendLabelW_ + 1;
            int sy = area.getHeight() - sendsH + 2;
            juce::Slider* sendKnobs[] = { &s.send1Knob, &s.send2Knob, &s.send3Knob, &s.send4Knob, &s.send5Knob, &s.send6Knob, &s.send7Knob };
            // Send1 (RVB) + depth combo
            sendKnobYs_[i][0] = sy;
            s.send1Knob.setBounds (sendX, sy, sendKnobSize_, sendKnobSize_);
            sy += sendPitch;
            // Depth combo right below RVB knob
            s.depthBox.setBounds (x + 1, sy, stripW - 2, depthBoxH);
            sy += depthBoxH + 2;
            // Sends 2–7
            for (int sk = 1; sk < 7; ++sk)
            {
                sendKnobYs_[i][sk] = sy;
                sendKnobs[sk]->setBounds (sendX, sy, sendKnobSize_, sendKnobSize_);
                sy += sendPitch;
            }
        }

        // ── FX column 1 (reverb, delay, chorus): index 6 ────
        {
            int x = 6 * stripW;
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
            fxKnobY_delayTime = y;
            delayTime.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_delayFdbk = y;
            delayFeedback.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_delayMix = y;
            delayMix.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_delayDensity = y;
            delayDensity.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_delayWarp = y;
            delayWarp.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap + 2;

            fxChorusLabelY = y; y += 12;
            fxKnobY_chorusRate = y;
            chorusRate.setBounds (cx, y, fxKnobSize, fxKnobSize); y += fxKnobSize + labelGap;
            fxKnobY_chorusDepth = y;
            chorusDepth.setBounds (cx, y, fxKnobSize, fxKnobSize);
            y += fxKnobSize + labelGap + 4;

            // Insert 1 section — below chorus in FX-1 column
            insLabelY_[0] = y; y += 14;
            int btnW = stripW - 6;
            int btnH = 18;
            insUI[0].nameLabel.setBounds (x + 2, y, btnW, 12); y += 14;
            insUI[0].loadBtn.setBounds   (x + 3, y, btnW, btnH); y += btnH + 2;
            insUI[0].bypassBtn.setBounds (x + 3, y, btnW, btnH); y += btnH + 2;
            insUI[0].editBtn.setBounds   (x + 3, y, btnW, btnH);
        }

        // ── FX column 2 (flanger, distortion): index 7 ──────
        {
            int x = 7 * stripW;
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
            y += fxKnobSize + labelGap + 4;

            // Insert 2 section — below distortion in FX-2 column
            insLabelY_[1] = y; y += 14;
            int btnW = stripW - 6;
            int btnH = 18;
            insUI[1].nameLabel.setBounds (x + 2, y, btnW, 12); y += 14;
            insUI[1].loadBtn.setBounds   (x + 3, y, btnW, btnH); y += btnH + 2;
            insUI[1].bypassBtn.setBounds (x + 3, y, btnW, btnH); y += btnH + 2;
            insUI[1].editBtn.setBounds   (x + 3, y, btnW, btnH);
        }

        // ── Master strip (rightmost, index 8) ────────────
        {
            int x = 8 * stripW;
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

            // WIDTH / CRSH section
            fxWidthLabelY = y; y += labelH + 2;
            fxKnobY_width = y;
            widthKnob.setBounds (cx, y, mstKnobSize, mstKnobSize); y += mstKnobSize + labelGap;
            fxKnobY_parallel = y;
            parallelKnob.setBounds (cx, y, mstKnobSize, mstKnobSize); y += mstKnobSize + labelGap + 4;

            // Console gear button — below comp, above fader
            int faderBottom = area.getHeight() - 8;
            int faderW = std::min (stripW - 10, 24);
            int faderX = x + (stripW - faderW) / 2;
            int gearBtnW = stripW - 6;
            consoleGearBtn.setBounds (x + 3, y, gearBtnW, 18);
            y += 22;

            // Master fader (fills remaining height)
            masterFader.setBounds (faderX, y, faderW, std::max (30, faderBottom - y));

            // VU meter position — to the right of the fader
            masterMeterX_ = static_cast<float> (faderX + faderW + 3);
            masterMeterY_ = y;
            masterMeterH_ = std::max (30, faderBottom - y);
        }

        // ── Console back panel overlay fills entire mixer ─────
        backPanel.setBounds (getLocalBounds());
    }

private:
    void timerCallback() override
    {
        for (int i = 0; i < 6; ++i)
            strips[i].peakLevel = mixer.getPeakLevel (i);
        masterPeakLevel_ = mixer.getMasterPeakLevel();
        updateInsertNames();
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
        juce::Slider   send6Knob;    // insert 1 send
        juce::Slider   send7Knob;    // insert 2 send
        juce::ComboBox depthBox;     // reverb depth (predelay: Close/Near/Mid/Far)
        juce::TextButton muteBtn;
        juce::TextButton soloBtn;
        juce::TextButton phaseBtn;
        juce::ComboBox hpfBox;
        juce::Slider   satKnob;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tiltAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send1Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send2Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send3Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send4Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send5Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send6Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> send7Attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> depthAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> phaseAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hpfAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> satAttachment;
        float peakLevel = 0.0f;
    };

    StripUI strips[6];

    // ── Master strip ─────────────────────────────────────────
    juce::Slider masterFader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttachment;
    float masterPeakLevel_ = 0.0f;
    int masterMeterY_ = 0, masterMeterH_ = 0;
    float masterMeterX_ = 0.0f;

    // ── Console back-panel ─────────────────────────────────
    juce::TextButton consoleGearBtn;
    ConsoleBackPanel backPanel { mixerAPVTS, effectsAPVTS };

    // ── Master width + parallel crush ─────────────────────
    juce::Slider widthKnob, parallelKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttach, parallelAttach;
    int fxKnobY_width = 0, fxKnobY_parallel = 0;
    int fxWidthLabelY = 0;

    // ── FX controls ──────────────────────────────────────────
    juce::Slider reverbSize, reverbDamping;
    juce::Slider delayTime, delayFeedback, delayMix, delayDensity, delayWarp;
    juce::Slider chorusRate, chorusDepth;
    juce::Slider flangerRate, flangerFeedback;
    juce::Slider distDrive, distTone;
    juce::Slider eqLow, eqMid, eqHigh;
    juce::Slider compThresh, compRatio;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbSizeAttach, reverbDampingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayTimeAttach, delayFeedbackAttach, delayMixAttach, delayDensityAttach, delayWarpAttach;
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
    int fxKnobY_delayTime = 0, fxKnobY_delayFdbk = 0, fxKnobY_delayMix = 0, fxKnobY_delayDensity = 0, fxKnobY_delayWarp = 0;
    int fxKnobY_chorusRate = 0, fxKnobY_chorusDepth = 0;
    int fxKnobY_flangerRate = 0, fxKnobY_flangerFdbk = 0;
    int fxKnobY_distDrive = 0, fxKnobY_distTone = 0;
    int fxKnobY_eqLow = 0, fxKnobY_eqMid = 0, fxKnobY_eqHigh = 0;
    int fxKnobY_compThresh = 0, fxKnobY_compRatio = 0;

    // Send knob positions (per channel)
    float sendAreaTop_ = 400.0f;
    int sendKnobSize_ = 24;
    int sendLabelW_ = 16;
    int sendKnobYs_[6][7] = {};
    int satLabelY_[6] = {};
    int hpfLabelY_[6] = {};

    // ── External insert slot UI ──────────────────────────────
    ExternalPluginSlot* insertSlot[2] = { nullptr, nullptr };
    int insLabelY_[2] = { 0, 0 };

    struct InsertUI
    {
        juce::Label      nameLabel;
        juce::TextButton loadBtn;
        juce::TextButton bypassBtn;
        juce::TextButton editBtn;
    };
    InsertUI insUI[2];

    void loadPluginForSlot (int slotIdx)
    {
        if (insertSlot[slotIdx] == nullptr)
            return;

        auto chooser = std::make_shared<juce::FileChooser> (
            "Select a VST3 plugin",
            juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory),
#if JUCE_WINDOWS
            "*.vst3"
#elif JUCE_MAC
            "*.vst3;*.component"
#else
            "*.vst3;*.so"
#endif
        );

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, slotIdx, chooser] (const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result == juce::File())
                    return;

                if (insertSlot[slotIdx]->loadPlugin (result))
                {
                    insUI[slotIdx].nameLabel.setText (
                        insertSlot[slotIdx]->getPluginName(),
                        juce::dontSendNotification);
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::MessageBoxIconType::WarningIcon,
                        "Plugin Load Failed",
                        "Could not load " + result.getFileName());
                }
            });
    }

    void updateInsertNames()
    {
        for (int i = 0; i < 2; ++i)
        {
            if (insertSlot[i] != nullptr)
            {
                insUI[i].nameLabel.setText (
                    insertSlot[i]->getPluginName(),
                    juce::dontSendNotification);
                insUI[i].bypassBtn.setToggleState (
                    insertSlot[i]->isBypassed(),
                    juce::dontSendNotification);
            }
        }
    }
};

} // namespace axelf::ui
