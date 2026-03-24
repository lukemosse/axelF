#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../../UI/AxelFColours.h"
#include "../../UI/SectionBox.h"
#include "../../UI/PatternStrip.h"
#include "../../UI/DX7EGDisplay.h"
#include "../../UI/AlgorithmDisplay.h"
#include "../../UI/VoicePresetBar.h"

namespace axelf::dx7
{

class DX7Editor : public juce::Component
{
public:
    DX7Editor(juce::AudioProcessorValueTreeState& apvts,
              axelf::PatternEngine* engine = nullptr, axelf::GlobalTransport* transport = nullptr)
        : globalSection("GLOBAL", juce::Colour(ui::Colours::dx7)),
          lfoSection("LFO", juce::Colour(ui::Colours::dx7))
    {
        addAndMakeVisible(globalSection);
        addAndMakeVisible(lfoSection);

        for (int op = 0; op < 6; ++op)
        {
            opSections[op] = std::make_unique<ui::SectionBox>(
                "OP " + juce::String(op + 1), juce::Colour(ui::Colours::dx7));
            addAndMakeVisible(*opSections[op]);
        }

        auto makeSlider = [&](const juce::String& paramId, const juce::String& label)
        {
            auto* slider = new juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            slider->setName(paramId);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 46, 12);
            slider->setNumDecimalPlacesToDisplay(3);
            slider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(ui::Colours::textSecondary));
            slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
            auto* att = (apvts.getParameter(paramId) != nullptr)
                ? new juce::AudioProcessorValueTreeState::SliderAttachment(apvts, paramId, *slider) : nullptr;
            auto* lbl = new juce::Label({}, label);
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(juce::Font(juce::FontOptions(8.5f)));
            lbl->setColour(juce::Label::textColourId, juce::Colour(ui::Colours::textLabel));
            addAndMakeVisible(slider);
            addAndMakeVisible(lbl);
            sliders.add(slider);
            if (att) sliderAttachments.add(att);
            sliderLabels.add(lbl);
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
            lbl->setFont(juce::Font(juce::FontOptions(9.0f)));
            lbl->setColour(juce::Label::textColourId, juce::Colour(ui::Colours::textLabel));
            addAndMakeVisible(combo);
            addAndMakeVisible(lbl);
            combos.add(combo);
            if (att) comboAttachments.add(att);
            comboLabels.add(lbl);
        };

        // Global: algorithm (slider 0), feedback (slider 1)
        makeSlider("dx7_algorithm", "Algorithm");
        makeSlider("dx7_feedback", "Feedback");

        // Per-operator (6 ops x 11 params = 66 sliders, indices 2-67)
        for (int op = 1; op <= 6; ++op)
        {
            auto prefix = "dx7_op" + juce::String(op) + "_";
            makeSlider(prefix + "level", "Level");
            makeSlider(prefix + "ratio", "Ratio");
            makeSlider(prefix + "detune", "Detune");
            makeSlider(prefix + "eg_r1", "Atk Rate");
            makeSlider(prefix + "eg_r2", "Dcy Rate");
            makeSlider(prefix + "eg_r3", "Sus Rate");
            makeSlider(prefix + "eg_r4", "Rel Rate");
            makeSlider(prefix + "eg_l1", "Atk Lvl");
            makeSlider(prefix + "eg_l2", "Dcy Lvl");
            makeSlider(prefix + "eg_l3", "Sus Lvl");
            makeSlider(prefix + "eg_l4", "Rel Lvl");
        }

        // LFO: sliders 68-71, combo 0
        makeSlider("dx7_lfo_speed", "Speed");
        makeSlider("dx7_lfo_delay", "Delay");
        makeSlider("dx7_lfo_pmd", "PMD");
        makeSlider("dx7_lfo_amd", "AMD");
        makeCombo("dx7_lfo_wave", "LFO Wave");

        // Algorithm display
        addAndMakeVisible(algorithmDisplay);
        algorithmDisplay.attach(*sliders[0]);

        // EG displays per operator
        for (int op = 0; op < 6; ++op)
        {
            addAndMakeVisible(egDisplays[op]);
            int base = 2 + op * 11;
            egDisplays[op].attach(*sliders[base + 3], *sliders[base + 4],
                                  *sliders[base + 5], *sliders[base + 6],
                                  *sliders[base + 7], *sliders[base + 8],
                                  *sliders[base + 9], *sliders[base + 10]);
        }

        if (engine != nullptr && transport != nullptr)
        {
            patternStrip = std::make_unique<ui::PatternStrip>(*engine, *transport, kDX7,
                                                              juce::Colour(ui::Colours::dx7));
            addAndMakeVisible(*patternStrip);
        }

        // Voice preset bar
        voicePresetBar = std::make_unique<ui::VoicePresetBar>(apvts, "DX7",
                                                              juce::Colour(ui::Colours::dx7));
        addAndMakeVisible(*voicePresetBar);

        setSize(1200, 650);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(ui::Colours::bgDark));

        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::dx7));
        g.drawText("YAMAHA DX7", 16, 4, 400, 24, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
        g.setColour(juce::Colour(ui::Colours::textSecondary));
        g.drawText("Marimba / Bell - 6-Op FM, 16-Voice Polyphonic", 16, 26, 400, 16,
                   juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::dx7));
        g.drawText("DX7", getWidth() - 120, 8, 110, 24, juce::Justification::centredRight);

        // Paint group headers inside each operator row
        g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
        auto dx7Col = juce::Colour(ui::Colours::dx7).withAlpha(0.5f);
        g.setColour(dx7Col);
        for (int op = 0; op < 6; ++op)
        {
            if (opSections[op] == nullptr) continue;
            auto oC = opSections[op]->getContentArea();
            int ox = opSections[op]->getX() + oC.getX();
            int oy = opSections[op]->getY() + oC.getY();
            int knobW = std::min(48, getWidth() / 26);
            int knobPitch = knobW + 6;
            int ratesX = ox + 3 * knobPitch + 72;
            int levelsX = ratesX + 4 * knobPitch + 10;
            g.drawText("RATES", ratesX, oy - 10, 4 * knobPitch, 10, juce::Justification::centredLeft);
            g.drawText("LEVELS", levelsX, oy - 10, 4 * knobPitch, 10, juce::Justification::centredLeft);
        }
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
        int knobW = std::min(48, totalW / 26);
        int knobH = knobW;
        int comboW = std::min(88, totalW / 14);
        int comboH = 22;
        int gap = 6;
        int knobPitch = knobW + gap;

        // Top row: Global + Algorithm Display + LFO
        int topH = std::min(90, (area.getHeight() - 12) * 16 / 100);
        auto topRow = area.removeFromTop(topH);
        int globalW = knobPitch * 2 + 12;
        globalSection.setBounds(topRow.removeFromLeft(globalW));
        topRow.removeFromLeft(gap);
        algorithmDisplay.setBounds(topRow.removeFromLeft(170).reduced(4));
        topRow.removeFromLeft(gap);
        lfoSection.setBounds(topRow.removeFromLeft(knobPitch * 4 + comboW + 20));

        area.removeFromTop(gap);

        // Operator rows (proportional)
        int opRowH = (area.getHeight() - 5) / 6;
        for (int op = 0; op < 6; ++op)
        {
            opSections[op]->setBounds(area.removeFromTop(opRowH));
            if (op < 5) area.removeFromTop(1);
        }

        // Position global knobs
        auto posKnob = [&](int idx, int x, int y) {
            sliders[idx]->setBounds(x, y, knobW, knobH);
            sliderLabels[idx]->setBounds(x - 4, y + knobH - 3, knobW + 8, 12);
        };

        auto gC = globalSection.getContentArea();
        int gx = globalSection.getX() + gC.getX();
        int gy = globalSection.getY() + gC.getY();
        posKnob(0, gx, gy);
        posKnob(1, gx + knobPitch, gy);

        // LFO knobs
        auto lC = lfoSection.getContentArea();
        int lx = lfoSection.getX() + lC.getX();
        int ly = lfoSection.getY() + lC.getY();
        for (int i = 0; i < 4; ++i)
            posKnob(68 + i, lx + i * knobPitch, ly);
        combos[0]->setBounds(lx + 4 * knobPitch + 4, ly + 8, comboW, comboH);
        comboLabels[0]->setBounds(lx + 4 * knobPitch + 4, ly - 4, comboW, 12);

        // Operator knobs
        for (int op = 0; op < 6; ++op)
        {
            auto oC = opSections[op]->getContentArea();
            int ox = opSections[op]->getX() + oC.getX();
            int oy = opSections[op]->getY() + oC.getY();
            int base = 2 + op * 11;

            // Level, Ratio, Detune
            for (int p = 0; p < 3; ++p)
                posKnob(base + p, ox + p * knobPitch, oy);

            // Mini EG display
            egDisplays[op].setBounds(ox + 3 * knobPitch, oy, 60, 40);

            // 4 Rate knobs (Attack, Decay, Sustain, Release)
            int ratesX = ox + 3 * knobPitch + 72;
            for (int r = 0; r < 4; ++r)
                posKnob(base + 3 + r, ratesX + r * knobPitch, oy);

            // 4 Level knobs
            int levelsX = ratesX + 4 * knobPitch + 10;
            for (int l = 0; l < 4; ++l)
                posKnob(base + 7 + l, levelsX + l * knobPitch, oy);
        }
    }

private:
    ui::SectionBox globalSection, lfoSection;
    ui::AlgorithmDisplay algorithmDisplay { juce::Colour(ui::Colours::dx7) };
    ui::DX7EGDisplay egDisplays[6] { juce::Colour(ui::Colours::dx7), juce::Colour(ui::Colours::dx7),
                                     juce::Colour(ui::Colours::dx7), juce::Colour(ui::Colours::dx7),
                                     juce::Colour(ui::Colours::dx7), juce::Colour(ui::Colours::dx7) };
    std::unique_ptr<ui::VoicePresetBar> voicePresetBar;
    std::unique_ptr<ui::PatternStrip> patternStrip;
    std::unique_ptr<ui::SectionBox> opSections[6];

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<juce::Label> sliderLabels;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<juce::Label> comboLabels;
};

} // namespace axelf::dx7