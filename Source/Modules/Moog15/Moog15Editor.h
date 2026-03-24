#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../../UI/AxelFColours.h"
#include "../../UI/SectionBox.h"
#include "../../UI/PatternStrip.h"
#include "../../UI/ADSRDisplay.h"
#include "../../UI/VoicePresetBar.h"

namespace axelf::moog15
{

class Moog15Editor : public juce::Component
{
public:
    Moog15Editor(juce::AudioProcessorValueTreeState& apvts,
                 axelf::PatternEngine* engine = nullptr, axelf::GlobalTransport* transport = nullptr)
        : vcoSection("VCO", juce::Colour(ui::Colours::moog)),
          mixSection("MIXER", juce::Colour(ui::Colours::moog)),
          vcfSection("VCF - Ladder", juce::Colour(ui::Colours::moog)),
          env1Section("ENV 1 - VCF", juce::Colour(ui::Colours::moog)),
          env2Section("ENV 2 - VCA", juce::Colour(ui::Colours::moog)),
          lfoSection("LFO / GLIDE", juce::Colour(ui::Colours::moog))
    {
        addAndMakeVisible(vcoSection);
        addAndMakeVisible(mixSection);
        addAndMakeVisible(vcfSection);
        addAndMakeVisible(env1Section);
        addAndMakeVisible(env2Section);
        addAndMakeVisible(lfoSection);

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

        // VCO
        makeCombo("moog_vco1_waveform", "VCO-1 Wave");    // combo 0
        makeSlider("moog_vco1_range", "VCO-1 Range");      // slider 0
        makeCombo("moog_vco2_waveform", "VCO-2 Wave");    // combo 1
        makeSlider("moog_vco2_detune", "VCO-2 Detune");    // slider 1
        makeCombo("moog_vco3_waveform", "VCO-3 Wave");    // combo 2
        makeSlider("moog_vco3_detune", "VCO-3 Detune");    // slider 2

        // Mixer
        makeSlider("moog_mix_vco1", "VCO-1 Lvl");          // slider 3
        makeSlider("moog_mix_vco2", "VCO-2 Lvl");          // slider 4
        makeSlider("moog_mix_vco3", "VCO-3 Lvl");          // slider 5

        // VCF
        makeSlider("moog_vcf_cutoff", "Cutoff");            // slider 6
        makeSlider("moog_vcf_resonance", "Resonance");      // slider 7
        makeSlider("moog_vcf_drive", "Drive");               // slider 8
        makeSlider("moog_vcf_env_depth", "Env Depth");      // slider 9

        // Env 1
        makeSlider("moog_env1_attack", "Atk");              // slider 10
        makeSlider("moog_env1_decay", "Dec");                // slider 11
        makeSlider("moog_env1_sustain", "Sus");              // slider 12
        makeSlider("moog_env1_release", "Rel");              // slider 13

        // Env 2
        makeSlider("moog_env2_attack", "Atk");              // slider 14
        makeSlider("moog_env2_decay", "Dec");                // slider 15
        makeSlider("moog_env2_sustain", "Sus");              // slider 16
        makeSlider("moog_env2_release", "Rel");              // slider 17

        // Glide
        makeSlider("moog_glide_time", "Glide");              // slider 18

        // LFO
        makeSlider("moog_lfo_rate", "Rate");                 // slider 19
        makeSlider("moog_lfo_depth", "Depth");               // slider 20
        makeCombo("moog_lfo_waveform", "LFO Wave");          // combo 3

        // Extended VCF controls
        makeCombo("moog_vcf_key_track", "Key Track");        // combo 4
        makeSlider("moog_vcf_lfo_amount", "LFO>VCF");       // slider 21
        makeSlider("moog_noise_level", "Noise");              // slider 22
        makeSlider("moog_lfo_pitch_amount", "LFO>Pitch");   // slider 23

        // PW and extra range
        makeSlider("moog_vco1_pw", "VCO-1 PW");             // slider 24
        makeSlider("moog_vco2_pw", "VCO-2 PW");             // slider 25
        makeSlider("moog_vco3_pw", "VCO-3 PW");             // slider 26
        makeSlider("moog_vco2_range", "VCO-2 Range");       // slider 27
        makeSlider("moog_vco3_range", "VCO-3 Range");       // slider 28

        // Feet notation for range sliders
        auto feetText = [](double val) -> juce::String {
            return juce::String(juce::roundToInt(val)) + "'";
        };
        sliders[0]->textFromValueFunction = feetText;
        sliders[27]->textFromValueFunction = feetText;
        sliders[28]->textFromValueFunction = feetText;

        if (engine != nullptr && transport != nullptr)
        {
            patternStrip = std::make_unique<ui::PatternStrip>(*engine, *transport, kMoog15,
                                                              juce::Colour(ui::Colours::moog));
            addAndMakeVisible(*patternStrip);
        }

        // ADSR displays
        addAndMakeVisible(env1Display);
        addAndMakeVisible(env2Display);
        env1Display.attach(*sliders[10], *sliders[11], *sliders[12], *sliders[13]);
        env2Display.attach(*sliders[14], *sliders[15], *sliders[16], *sliders[17]);

        // Voice preset bar
        voicePresetBar = std::make_unique<ui::VoicePresetBar>(apvts, "Moog15",
                                                              juce::Colour(ui::Colours::moog));
        addAndMakeVisible(*voicePresetBar);

        setSize(1200, 650);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(ui::Colours::bgDark));

        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::moog));
        g.drawText("MOOG MODULAR 15", 16, 4, 400, 24, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
        g.setColour(juce::Colour(ui::Colours::textSecondary));
        g.drawText("Bass - Monophonic / 3 VCO", 16, 26, 400, 16,
                   juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::moog));
        g.drawText("MOOG MODULAR 15", getWidth() - 260, 8, 250, 24, juce::Justification::centredRight);
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

        // Row 1: VCO (50%) + Mixer (18%) + VCF (32%)
        int row1H = std::min(190, (area.getHeight() - 16) * 38 / 100);
        auto row1 = area.removeFromTop(row1H);
        int vcoW = totalW * 50 / 100;
        int mixW = totalW * 18 / 100;
        vcoSection.setBounds(row1.removeFromLeft(vcoW));
        row1.removeFromLeft(gap);
        mixSection.setBounds(row1.removeFromLeft(mixW));
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

        // Row 3: LFO + Glide (fill remaining)
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

        // VCO section
        auto vcoC = vcoSection.getContentArea();
        int vx = vcoSection.getX() + vcoC.getX();
        int vy = vcoSection.getY() + vcoC.getY();
        int vcoColW = vcoC.getWidth() / 3;
        for (int i = 0; i < 3; ++i)
        {
            int ox = vx + i * vcoColW;
            posCombo(i, ox, vy + 4);
            posKnob(i == 0 ? 0 : (i == 1 ? 27 : 28), ox + comboW + 8, vy + 4);
            if (i > 0) posKnob(i == 1 ? 1 : 2, ox + 4, vy + 40);
            posKnob(24 + i, ox + comboW + 8, vy + 40);
        }
        posKnob(0, vx + comboW + 8, vy + 4);  // VCO-1 Range

        // Mixer
        auto mixC = mixSection.getContentArea();
        int mx = mixSection.getX() + mixC.getX();
        int my = mixSection.getY() + mixC.getY();
        for (int i = 0; i < 3; ++i)
            posKnob(3 + i, mx + i * knobPitch, my);
        posKnob(22, mx, my + knobH + 22);

        // VCF
        auto vcfC = vcfSection.getContentArea();
        int fx = vcfSection.getX() + vcfC.getX();
        int fy = vcfSection.getY() + vcfC.getY();
        for (int i = 0; i < 4; ++i)
            posKnob(6 + i, fx + i * knobPitch, fy);
        posCombo(4, fx, fy + knobH + 20);
        posKnob(21, fx + knobPitch * 2, fy + knobH + 22);

        // Env 1
        auto e1C = env1Section.getContentArea();
        int e1x = env1Section.getX() + e1C.getX();
        int e1y = env1Section.getY() + e1C.getY();
        env1Display.setBounds(e1x, e1y, 70, 56);
        for (int i = 0; i < 4; ++i)
            posKnob(10 + i, e1x + 78 + i * knobPitch, e1y);

        // Env 2
        auto e2C = env2Section.getContentArea();
        int e2x = env2Section.getX() + e2C.getX();
        int e2y = env2Section.getY() + e2C.getY();
        env2Display.setBounds(e2x, e2y, 70, 56);
        for (int i = 0; i < 4; ++i)
            posKnob(14 + i, e2x + 78 + i * knobPitch, e2y);

        // LFO + Glide
        auto lfoC = lfoSection.getContentArea();
        int lx = lfoSection.getX() + lfoC.getX();
        int ly = lfoSection.getY() + lfoC.getY();
        posKnob(18, lx, ly);
        posKnob(19, lx + knobPitch, ly);
        posKnob(20, lx + knobPitch * 2, ly);
        posCombo(3, lx + knobPitch * 3, ly + 14);
        posKnob(23, lx + knobPitch * 4 + comboW + 8, ly);
    }

private:
    ui::SectionBox vcoSection, mixSection, vcfSection, env1Section, env2Section, lfoSection;
    ui::ADSRDisplay env1Display { juce::Colour(ui::Colours::moog) };
    ui::ADSRDisplay env2Display { juce::Colour(ui::Colours::moog) };
    std::unique_ptr<ui::VoicePresetBar> voicePresetBar;
    std::unique_ptr<ui::PatternStrip> patternStrip;

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<juce::Label> comboLabels;
};

} // namespace axelf::moog15