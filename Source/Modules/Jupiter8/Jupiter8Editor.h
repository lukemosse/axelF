#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../../UI/AxelFColours.h"
#include "../../UI/SectionBox.h"
#include "../../UI/PatternStrip.h"
#include "../../UI/ADSRDisplay.h"
#include "../../UI/VoicePresetBar.h"

namespace axelf::jupiter8
{

class Jupiter8Editor : public juce::Component
{
public:
    Jupiter8Editor(juce::AudioProcessorValueTreeState& apvts,
                   axelf::PatternEngine* engine = nullptr, axelf::GlobalTransport* transport = nullptr)
        : dcoSection("DCO", juce::Colour(ui::Colours::jupiter)),
          vcfSection("VCF", juce::Colour(ui::Colours::jupiter)),
          env1Section("ENV 1 - Filter", juce::Colour(ui::Colours::jupiter)),
          env2Section("ENV 2 - Amp", juce::Colour(ui::Colours::jupiter)),
          lfoSection("LFO / VOICE / ARP", juce::Colour(ui::Colours::jupiter))
    {
        addAndMakeVisible(dcoSection);
        addAndMakeVisible(vcfSection);
        addAndMakeVisible(env1Section);
        addAndMakeVisible(env2Section);
        addAndMakeVisible(lfoSection);

        if (engine != nullptr && transport != nullptr)
        {
            patternStrip = std::make_unique<ui::PatternStrip>(*engine, *transport, kJupiter8,
                                                              juce::Colour(ui::Colours::jupiter));
            addAndMakeVisible(*patternStrip);
        }

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
        makeCombo("jup8_dco1_waveform", "DCO-1 Wave");       // combo 0
        makeSlider("jup8_dco1_range", "DCO-1 Range");         // slider 0
        makeCombo("jup8_dco2_waveform", "DCO-2 Wave");       // combo 1
        makeSlider("jup8_dco2_detune", "DCO-2 Detune");       // slider 1
        makeSlider("jup8_dco2_range", "DCO-2 Range");         // slider 2
        makeSlider("jup8_mix_dco1", "DCO-1 Lvl");             // slider 3
        makeSlider("jup8_mix_dco2", "DCO-2 Lvl");             // slider 4

        // VCF
        makeSlider("jup8_vcf_cutoff", "Cutoff");              // slider 5
        makeSlider("jup8_vcf_resonance", "Resonance");        // slider 6
        makeSlider("jup8_vcf_env_depth", "Env Depth");        // slider 7
        makeCombo("jup8_vcf_hpf", "HPF Mode");               // combo 2

        // Env 1
        makeSlider("jup8_env1_attack", "Atk");                // slider 8
        makeSlider("jup8_env1_decay", "Dec");                  // slider 9
        makeSlider("jup8_env1_sustain", "Sus");                // slider 10
        makeSlider("jup8_env1_release", "Rel");                // slider 11

        // Env 2
        makeSlider("jup8_env2_attack", "Atk");                // slider 12
        makeSlider("jup8_env2_decay", "Dec");                  // slider 13
        makeSlider("jup8_env2_sustain", "Sus");                // slider 14
        makeSlider("jup8_env2_release", "Rel");                // slider 15

        // LFO
        makeSlider("jup8_lfo_rate", "Rate");                   // slider 16
        makeSlider("jup8_lfo_depth", "Depth");                 // slider 17
        makeCombo("jup8_lfo_waveform", "LFO Wave");           // combo 3
        makeCombo("jup8_voice_mode", "Voice Mode");           // combo 4

        // Extended DCO controls
        makeSlider("jup8_pulse_width", "Pulse Width");         // slider 18
        makeSlider("jup8_sub_level", "Sub Level");             // slider 19
        makeSlider("jup8_noise_level", "Noise Level");         // slider 20
        makeCombo("jup8_vcf_key_track", "Key Track");         // combo 5
        makeSlider("jup8_vcf_lfo_amount", "LFO>VCF");         // slider 21
        makeSlider("jup8_lfo_delay", "LFO Delay");            // slider 22
        makeSlider("jup8_portamento", "Portamento");           // slider 23

        // Cross-Mod & Sync
        makeSlider("jup8_dco_cross_mod", "X-Mod");            // slider 24
        makeCombo("jup8_dco_sync", "Sync");                   // combo 6

        // Arpeggiator
        makeCombo("jup8_arp_on", "Arp");                       // combo 7
        makeCombo("jup8_arp_mode", "Arp Mode");               // combo 8
        makeCombo("jup8_arp_range", "Arp Range");             // combo 9

        // Feet notation for range sliders
        auto feetText = [](double val) -> juce::String {
            return juce::String(juce::roundToInt(val)) + "'";
        };
        sliders[0]->textFromValueFunction = feetText;
        sliders[2]->textFromValueFunction = feetText;

        // ADSR displays
        addAndMakeVisible(env1Display);
        addAndMakeVisible(env2Display);
        env1Display.attach(*sliders[8], *sliders[9], *sliders[10], *sliders[11]);
        env2Display.attach(*sliders[12], *sliders[13], *sliders[14], *sliders[15]);

        // Voice preset bar
        voicePresetBar = std::make_unique<ui::VoicePresetBar>(apvts, "Jupiter8",
                                                              juce::Colour(ui::Colours::jupiter));
        addAndMakeVisible(*voicePresetBar);

        setSize(1200, 650);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(ui::Colours::bgDark));

        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::jupiter));
        g.drawText("ROLAND JUPITER-8", 16, 4, 400, 24, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
        g.setColour(juce::Colour(ui::Colours::textSecondary));
        g.drawText("Lead Synth - 8-Voice Polyphonic / Unison", 16, 26, 400, 16,
                   juce::Justification::centredLeft);

        // Module badge
        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::jupiter));
        g.drawText("JUPITER-8", getWidth() - 200, 8, 190, 24, juce::Justification::centredRight);
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

        // Pattern strip at bottom
        if (patternStrip)
        {
            patternStrip->setBounds(area.removeFromBottom(44));
            area.removeFromBottom(4);
        }

        // Scale knob size to available space
        int totalW = area.getWidth();
        int knobW = std::min(56, totalW / 20);
        int knobH = knobW;
        int comboW = std::min(100, totalW / 12);
        int comboH = 24;
        int gap = 6;
        int knobPitch = knobW + gap;   // center-to-center

        // Row 1: DCO + VCF  (65% / 35% split)
        int row1H = std::min(190, (area.getHeight() - 16) * 38 / 100);
        auto row1 = area.removeFromTop(row1H);
        int dcoW = totalW * 60 / 100;
        dcoSection.setBounds(row1.removeFromLeft(dcoW));
        row1.removeFromLeft(gap);
        vcfSection.setBounds(row1);

        area.removeFromTop(gap);

        // Row 2: ENV 1 + ENV 2  (prominent — 30% of space, equal split)
        int envH = std::min(110, (area.getHeight() - 8) * 35 / 100);
        auto envRow = area.removeFromTop(envH);
        int envW = (totalW - gap) / 2;
        env1Section.setBounds(envRow.removeFromLeft(envW));
        envRow.removeFromLeft(gap);
        env2Section.setBounds(envRow);

        area.removeFromTop(gap);

        // Row 3: LFO / Voice / Arp
        lfoSection.setBounds(area);

        // --- Position controls ---
        auto posKnob = [&](int idx, int x, int y) {
            sliders[idx]->setBounds(x, y, knobW, knobH);
            labels[idx]->setBounds(x - 4, y + knobH + 1, knobW + 8, 14);
        };
        auto posCombo = [&](int idx, int x, int y) {
            combos[idx]->setBounds(x, y, comboW, comboH);
            comboLabels[idx]->setBounds(x, y - 14, comboW, 14);
        };

        // DCO section
        auto dcoC = dcoSection.getContentArea();
        int dx = dcoSection.getX() + dcoC.getX();
        int dy = dcoSection.getY() + dcoC.getY();
        int dcoCol = (dcoC.getWidth() - 8) / 6;
        posCombo(0, dx, dy + 4);                                    // DCO-1 Wave
        posKnob(0, dx + comboW + 8, dy);                           // DCO-1 Range
        posKnob(20, dx + comboW + 8 + knobPitch, dy);              // Noise Level
        posKnob(3, dx + comboW + 8 + knobPitch * 2, dy);           // DCO-1 Lvl
        posKnob(24, dx + comboW + 8 + knobPitch * 3, dy);          // X-Mod
        posCombo(6, dx + comboW + 8 + knobPitch * 4, dy + 12);     // Sync

        int row2y = dy + knobH + 22;
        posCombo(1, dx, row2y + 4);                                 // DCO-2 Wave
        posKnob(1, dx + comboW + 8, row2y);                        // DCO-2 Detune
        posKnob(2, dx + comboW + 8 + knobPitch, row2y);            // DCO-2 Range
        posKnob(4, dx + comboW + 8 + knobPitch * 2, row2y);        // DCO-2 Lvl
        posKnob(18, dx + comboW + 8 + knobPitch * 3, row2y);       // Pulse Width
        posKnob(19, dx + comboW + 8 + knobPitch * 4, row2y);       // Sub Level

        // VCF section
        auto vcfC = vcfSection.getContentArea();
        int vx = vcfSection.getX() + vcfC.getX();
        int vy = vcfSection.getY() + vcfC.getY();
        posKnob(5, vx, vy);                                        // Cutoff
        posKnob(6, vx + knobPitch, vy);                            // Resonance
        posKnob(7, vx + knobPitch * 2, vy);                        // Env Depth

        int vcfRow2 = vy + knobH + 22;
        posCombo(2, vx, vcfRow2 + 4);                              // HPF Mode
        posCombo(5, vx + comboW + 8, vcfRow2 + 4);                 // Key Track
        posKnob(21, vx + knobPitch * 2, vcfRow2);                  // LFO>VCF

        // Env 1 section
        auto e1C = env1Section.getContentArea();
        int e1x = env1Section.getX() + e1C.getX();
        int e1y = env1Section.getY() + e1C.getY();
        env1Display.setBounds(e1x, e1y, 70, 56);
        for (int i = 0; i < 4; ++i)
            posKnob(8 + i, e1x + 78 + i * knobPitch, e1y);

        // Env 2 section
        auto e2C = env2Section.getContentArea();
        int e2x = env2Section.getX() + e2C.getX();
        int e2y = env2Section.getY() + e2C.getY();
        env2Display.setBounds(e2x, e2y, 70, 56);
        for (int i = 0; i < 4; ++i)
            posKnob(12 + i, e2x + 78 + i * knobPitch, e2y);

        // LFO section
        auto lfoC = lfoSection.getContentArea();
        int lx = lfoSection.getX() + lfoC.getX();
        int ly = lfoSection.getY() + lfoC.getY();
        posKnob(16, lx, ly);                                       // Rate
        posKnob(17, lx + knobPitch, ly);                           // Depth
        posKnob(22, lx + knobPitch * 2, ly);                       // LFO Delay
        posCombo(3, lx + knobPitch * 3, ly + 14);                  // LFO Wave
        posCombo(4, lx + knobPitch * 3 + comboW + 12, ly + 14);    // Voice Mode
        posKnob(23, lx + knobPitch * 3 + comboW + 12, ly + comboH + 20); // Portamento

        // Arp controls
        int arpX = lx + knobPitch * 3 + (comboW + 12) * 2;
        posCombo(7, arpX, ly + 4);                                  // Arp On/Off
        posCombo(8, arpX, ly + comboH + 10);                       // Arp Mode
        posCombo(9, arpX, ly + (comboH + 10) * 2);                 // Arp Range
    }

private:
    ui::SectionBox dcoSection, vcfSection, env1Section, env2Section, lfoSection;
    ui::ADSRDisplay env1Display { juce::Colour(ui::Colours::jupiter) };
    ui::ADSRDisplay env2Display { juce::Colour(ui::Colours::jupiter) };
    std::unique_ptr<ui::VoicePresetBar> voicePresetBar;
    std::unique_ptr<ui::PatternStrip> patternStrip;

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<juce::Label> comboLabels;
};

} // namespace axelf::jupiter8