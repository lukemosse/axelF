#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../../UI/AxelFColours.h"
#include "../../UI/SectionBox.h"
#include "../../UI/PatternStrip.h"
#include "../../UI/ADSRDisplay.h"
#include "../../UI/VoicePresetBar.h"

namespace axelf::jx3p
{

class JX3PEditor : public juce::Component
{
public:
    JX3PEditor(juce::AudioProcessorValueTreeState& apvts,
               axelf::PatternEngine* engine = nullptr, axelf::GlobalTransport* transport = nullptr)
        : dcoSection("DCO", juce::Colour(ui::Colours::jx3p)),
          vcfSection("VCF - IR3109", juce::Colour(ui::Colours::jx3p)),
          env1Section("ENV 1 - VCF", juce::Colour(ui::Colours::jx3p)),
          env2Section("ENV 2 - VCA", juce::Colour(ui::Colours::jx3p)),
          lfoSection("LFO", juce::Colour(ui::Colours::jx3p)),
          chorusSection("CHORUS", juce::Colour(ui::Colours::jx3p))
    {
        addAndMakeVisible(dcoSection);
        addAndMakeVisible(vcfSection);
        addAndMakeVisible(env1Section);
        addAndMakeVisible(env2Section);
        addAndMakeVisible(lfoSection);
        addAndMakeVisible(chorusSection);

        auto makeSlider = [&](const juce::String& paramId, const juce::String& label,
                              juce::Slider::SliderStyle style = juce::Slider::RotaryHorizontalVerticalDrag)
        {
            auto* slider = new juce::Slider(style, juce::Slider::TextBoxBelow);
            slider->setName(paramId);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
            slider->setNumDecimalPlacesToDisplay(3);
            slider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(ui::Colours::textSecondary));
            slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
            auto* att = (apvts.getParameter(paramId) != nullptr)
                ? new juce::AudioProcessorValueTreeState::SliderAttachment(apvts, paramId, *slider) : nullptr;
            auto* lbl = new juce::Label({}, label);
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(juce::Font(juce::FontOptions(10.0f)));
            lbl->setColour(juce::Label::textColourId, juce::Colour(ui::Colours::textLabel));
            addAndMakeVisible(slider);
            addAndMakeVisible(lbl);
            sliders.add(slider);
            if (att) attachments.add(att);
            labels.add(lbl);
        };

        auto makeCombo = [&](const juce::String& paramId, const juce::String& label)
        {
            auto* combo = new juce::ComboBox();
            if (auto* cp = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
                combo->addItemList(cp->choices, 1);
            auto* att = (apvts.getParameter(paramId) != nullptr)
                ? new juce::AudioProcessorValueTreeState::ComboBoxAttachment(apvts, paramId, *combo) : nullptr;
            auto* lbl = new juce::Label({}, label);
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(juce::Font(juce::FontOptions(10.0f)));
            lbl->setColour(juce::Label::textColourId, juce::Colour(ui::Colours::textLabel));
            addAndMakeVisible(combo);
            addAndMakeVisible(lbl);
            combos.add(combo);
            if (att) comboAttachments.add(att);
            comboLabels.add(lbl);
        };

        // DCO
        makeCombo("jx3p_dco1_waveform", "DCO-1 Wave");    // combo 0
        makeSlider("jx3p_dco1_range", "DCO-1 Range");      // slider 0
        makeCombo("jx3p_dco2_waveform", "DCO-2 Wave");    // combo 1
        makeSlider("jx3p_dco2_detune", "DCO-2 Detune");    // slider 1
        makeSlider("jx3p_dco2_range", "DCO-2 Range");      // slider 2
        makeSlider("jx3p_mix_dco1", "DCO-1 Lvl");          // slider 3
        makeSlider("jx3p_mix_dco2", "DCO-2 Lvl");          // slider 4

        // VCF
        makeSlider("jx3p_vcf_cutoff", "Cutoff");            // slider 5
        makeSlider("jx3p_vcf_resonance", "Resonance");      // slider 6
        makeSlider("jx3p_vcf_env_depth", "Env Depth");      // slider 7

        // Env 1
        makeSlider("jx3p_env_attack", "Atk");               // slider 8
        makeSlider("jx3p_env_decay", "Dec");                 // slider 9
        makeSlider("jx3p_env_sustain", "Sus");               // slider 10
        makeSlider("jx3p_env_release", "Rel");               // slider 11

        // Env 2
        makeSlider("jx3p_env2_attack", "Atk");              // slider 12
        makeSlider("jx3p_env2_decay", "Dec");                // slider 13
        makeSlider("jx3p_env2_sustain", "Sus");              // slider 14
        makeSlider("jx3p_env2_release", "Rel");              // slider 15

        // LFO
        makeSlider("jx3p_lfo_rate", "Rate");                 // slider 16
        makeSlider("jx3p_lfo_depth", "Depth");               // slider 17
        makeCombo("jx3p_lfo_waveform", "LFO Wave");         // combo 2

        // Chorus
        makeCombo("jx3p_chorus_mode", "Mode");               // combo 3
        makeSlider("jx3p_chorus_rate", "Rate");              // slider 18
        makeSlider("jx3p_chorus_depth", "Depth");            // slider 19

        // Extended controls
        makeCombo("jx3p_vcf_key_track", "Key Track");       // combo 4
        makeSlider("jx3p_vcf_lfo_amount", "LFO>VCF");       // slider 20
        makeSlider("jx3p_lfo_delay", "LFO Delay");          // slider 21
        makeSlider("jx3p_dco_cross_mod", "X-Mod");          // slider 22
        makeCombo("jx3p_dco_sync", "Sync");                  // combo 5
        makeSlider("jx3p_chorus_spread", "Spread");          // slider 23

        // Feet notation for range sliders
        auto feetText = [](double val) -> juce::String {
            return juce::String(juce::roundToInt(val)) + "'";
        };
        sliders[0]->textFromValueFunction = feetText;
        sliders[2]->textFromValueFunction = feetText;

        if (engine != nullptr && transport != nullptr)
        {
            patternStrip = std::make_unique<ui::PatternStrip>(*engine, *transport, kJX3P,
                                                              juce::Colour(ui::Colours::jx3p));
            addAndMakeVisible(*patternStrip);
        }

        // ADSR displays
        addAndMakeVisible(env1Display);
        addAndMakeVisible(env2Display);
        env1Display.attach(*sliders[8], *sliders[9], *sliders[10], *sliders[11]);
        env2Display.attach(*sliders[12], *sliders[13], *sliders[14], *sliders[15]);

        // Voice preset bar
        voicePresetBar = std::make_unique<ui::VoicePresetBar>(apvts, "JX3P",
                                                              juce::Colour(ui::Colours::jx3p));
        addAndMakeVisible(*voicePresetBar);

        setSize(1200, 650);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(ui::Colours::bgDark));

        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::jx3p));
        g.drawText("ROLAND JX-3P", 16, 4, 400, 24, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
        g.setColour(juce::Colour(ui::Colours::textSecondary));
        g.drawText("Chord Stabs - 6-Voice Polyphonic + Chorus", 16, 26, 400, 16,
                   juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::jx3p));
        g.drawText("JX-3P", getWidth() - 160, 8, 150, 24, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(56);  // panel header

        // Voice preset bar
        if (voicePresetBar)
        {
            voicePresetBar->setBounds(area.removeFromTop(36));
            area.removeFromTop(4);
        }

        // Pattern strip at the bottom
        if (patternStrip)
        {
            patternStrip->setBounds(area.removeFromBottom(44));
            area.removeFromBottom(4);
        }

        int totalW = area.getWidth();
        int knobW = std::min(56, totalW / 20);
        int knobH = knobW;
        int comboW = std::min(100, totalW / 12);
        int comboH = 24;
        int gap = 6;
        int knobPitch = knobW + gap;

        // Row 1: DCO + VCF  (55% / 45%)
        int row1H = std::min(190, (area.getHeight() - 16) * 38 / 100);
        auto row1 = area.removeFromTop(row1H);
        int dcoW = totalW * 55 / 100;
        dcoSection.setBounds(row1.removeFromLeft(dcoW));
        row1.removeFromLeft(gap);
        vcfSection.setBounds(row1);

        area.removeFromTop(gap);

        // Row 2: ENV 1 + ENV 2 (equal, prominent)
        int envH = std::min(110, (area.getHeight() - 8) * 35 / 100);
        auto envRow = area.removeFromTop(envH);
        int envW = (totalW - gap) / 2;
        env1Section.setBounds(envRow.removeFromLeft(envW));
        envRow.removeFromLeft(gap);
        env2Section.setBounds(envRow);

        area.removeFromTop(gap);

        // Row 3: LFO + Chorus
        auto row3 = area;
        int lfoW = (totalW - gap) / 2;
        lfoSection.setBounds(row3.removeFromLeft(lfoW));
        row3.removeFromLeft(gap);
        chorusSection.setBounds(row3);

        // Position controls
        auto posKnob = [&](int idx, int x, int y) {
            sliders[idx]->setBounds(x, y, knobW, knobH);
            labels[idx]->setBounds(x - 4, y + knobH + 1, knobW + 8, 14);
        };
        auto posCombo = [&](int idx, int x, int y) {
            combos[idx]->setBounds(x, y, comboW, comboH);
            comboLabels[idx]->setBounds(x, y - 14, comboW, 14);
        };

        // DCO
        auto dcoC = dcoSection.getContentArea();
        int dx = dcoSection.getX() + dcoC.getX();
        int dy = dcoSection.getY() + dcoC.getY();
        posCombo(0, dx, dy + 4);
        posKnob(0, dx + comboW + 8, dy);
        posKnob(3, dx + comboW + 8 + knobPitch, dy);
        posKnob(22, dx + comboW + 8 + knobPitch * 2, dy);
        posCombo(5, dx + comboW + 8 + knobPitch * 3, dy + 12);

        int dcoRow2 = dy + knobH + 22;
        posCombo(1, dx, dcoRow2 + 4);
        posKnob(1, dx + comboW + 8, dcoRow2);
        posKnob(2, dx + comboW + 8 + knobPitch, dcoRow2);
        posKnob(4, dx + comboW + 8 + knobPitch * 2, dcoRow2);

        // VCF
        auto vcfC = vcfSection.getContentArea();
        int vx = vcfSection.getX() + vcfC.getX();
        int vy = vcfSection.getY() + vcfC.getY();
        posKnob(5, vx, vy);
        posKnob(6, vx + knobPitch, vy);
        posKnob(7, vx + knobPitch * 2, vy);

        int vcfRow2 = vy + knobH + 22;
        posCombo(4, vx, vcfRow2 + 4);
        posKnob(20, vx + comboW + 8, vcfRow2);

        // Env 1
        auto e1C = env1Section.getContentArea();
        int e1x = env1Section.getX() + e1C.getX();
        int e1y = env1Section.getY() + e1C.getY();
        env1Display.setBounds(e1x, e1y, 70, 56);
        for (int i = 0; i < 4; ++i)
            posKnob(8 + i, e1x + 78 + i * knobPitch, e1y);

        // Env 2
        auto e2C = env2Section.getContentArea();
        int e2x = env2Section.getX() + e2C.getX();
        int e2y = env2Section.getY() + e2C.getY();
        env2Display.setBounds(e2x, e2y, 70, 56);
        for (int i = 0; i < 4; ++i)
            posKnob(12 + i, e2x + 78 + i * knobPitch, e2y);

        // LFO
        auto lfoC = lfoSection.getContentArea();
        int lx = lfoSection.getX() + lfoC.getX();
        int ly = lfoSection.getY() + lfoC.getY();
        posKnob(16, lx, ly);
        posKnob(17, lx + knobPitch, ly);
        posKnob(21, lx + knobPitch * 2, ly);
        posCombo(2, lx + knobPitch * 3, ly + 14);

        // Chorus
        auto chC = chorusSection.getContentArea();
        int cx = chorusSection.getX() + chC.getX();
        int cy = chorusSection.getY() + chC.getY();
        posCombo(3, cx, cy + 4);
        posKnob(18, cx + comboW + 8, cy);
        posKnob(19, cx + comboW + 8 + knobPitch, cy);
        posKnob(23, cx + comboW + 8 + knobPitch * 2, cy);
    }

private:
    ui::SectionBox dcoSection, vcfSection, env1Section, env2Section, lfoSection, chorusSection;
    ui::ADSRDisplay env1Display { juce::Colour(ui::Colours::jx3p) };
    ui::ADSRDisplay env2Display { juce::Colour(ui::Colours::jx3p) };
    std::unique_ptr<ui::VoicePresetBar> voicePresetBar;
    std::unique_ptr<ui::PatternStrip> patternStrip;

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<juce::Label> comboLabels;
};

} // namespace axelf::jx3p