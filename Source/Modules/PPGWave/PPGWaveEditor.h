#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../../UI/AxelFColours.h"
#include "../../UI/SectionBox.h"
#include "../../UI/PatternStrip.h"
#include "../../UI/VoicePresetBar.h"

namespace axelf::ppgwave
{

class PPGWaveEditor : public juce::Component
{
public:
    PPGWaveEditor(juce::AudioProcessorValueTreeState& apvts,
                  axelf::PatternEngine* engine = nullptr, axelf::GlobalTransport* transport = nullptr)
        : oscASection("WAVE A", juce::Colour(ui::Colours::ppgwave)),
          oscBSection("WAVE B", juce::Colour(ui::Colours::ppgwave)),
          subSection("SUB / NOISE", juce::Colour(ui::Colours::ppgwave)),
          mixerSection("MIXER", juce::Colour(ui::Colours::ppgwave)),
          filterSection("VCF  SSM 2044", juce::Colour(ui::Colours::ppgwave)),
          envSection("ENVELOPES", juce::Colour(ui::Colours::ppgwave)),
          lfoSection("LFO", juce::Colour(ui::Colours::ppgwave)),
          perfSection("PERFORMANCE", juce::Colour(ui::Colours::ppgwave))
    {
        addAndMakeVisible(oscASection);
        addAndMakeVisible(oscBSection);
        addAndMakeVisible(subSection);
        addAndMakeVisible(mixerSection);
        addAndMakeVisible(filterSection);
        addAndMakeVisible(envSection);
        addAndMakeVisible(lfoSection);
        addAndMakeVisible(perfSection);

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

        // --- Osc A: sliders 0-7, combo 0 ---
        makeCombo("ppg_waveA_table", "Table");        // combo 0
        makeSlider("ppg_waveA_pos", "Position");      // slider 0
        makeSlider("ppg_waveA_env_amt", "Env Amt");   // slider 1
        makeSlider("ppg_waveA_lfo_amt", "LFO Amt");   // slider 2
        makeSlider("ppg_waveA_semi", "Semi");          // slider 3
        makeSlider("ppg_waveA_detune", "Detune");      // slider 4
        makeSlider("ppg_waveA_level", "Level");        // slider 5

        // --- Osc B: sliders 6-11, combo 1 ---
        makeCombo("ppg_waveB_table", "Table");         // combo 1
        makeSlider("ppg_waveB_pos", "Position");       // slider 6
        makeSlider("ppg_waveB_env_amt", "Env Amt");    // slider 7
        makeSlider("ppg_waveB_lfo_amt", "LFO Amt");    // slider 8
        makeSlider("ppg_waveB_semi", "Semi");           // slider 9
        makeSlider("ppg_waveB_detune", "Detune");       // slider 10
        makeSlider("ppg_waveB_level", "Level");         // slider 11

        // --- Sub / Noise: sliders 12-15, combo 2 ---
        makeCombo("ppg_sub_wave", "Sub Wave");          // combo 2
        makeSlider("ppg_sub_level", "Sub Level");       // slider 12
        makeSlider("ppg_noise_level", "Noise Level");   // slider 13
        makeSlider("ppg_noise_color", "Noise Color");   // slider 14

        // --- Mixer: sliders 15-19 ---
        makeSlider("ppg_mix_waveA", "Wave A");         // slider 15
        makeSlider("ppg_mix_waveB", "Wave B");         // slider 16
        makeSlider("ppg_mix_sub", "Sub");              // slider 17
        makeSlider("ppg_mix_noise", "Noise");          // slider 18
        makeSlider("ppg_mix_ringmod", "Ring Mod");     // slider 19

        // --- Filter: sliders 20-24, combo 3 ---
        makeSlider("ppg_vcf_cutoff", "Cutoff");        // slider 20
        makeSlider("ppg_vcf_resonance", "Resonance");  // slider 21
        makeSlider("ppg_vcf_env_amt", "Env Amt");      // slider 22
        makeSlider("ppg_vcf_lfo_amt", "LFO Amt");      // slider 23
        makeSlider("ppg_vcf_keytrack", "Keytrack");     // slider 24
        makeCombo("ppg_vcf_type", "Filter Type");       // combo 3

        // --- Envelopes: sliders 25-36 ---
        // Env 1 (VCF)
        makeSlider("ppg_env1_attack", "Atk");           // slider 25
        makeSlider("ppg_env1_decay", "Dcy");            // slider 26
        makeSlider("ppg_env1_sustain", "Sus");           // slider 27
        makeSlider("ppg_env1_release", "Rel");           // slider 28
        // Env 2 (VCA)
        makeSlider("ppg_env2_attack", "Atk");           // slider 29
        makeSlider("ppg_env2_decay", "Dcy");            // slider 30
        makeSlider("ppg_env2_sustain", "Sus");           // slider 31
        makeSlider("ppg_env2_release", "Rel");           // slider 32
        // Env 3 (Wave)
        makeSlider("ppg_env3_attack", "Atk");           // slider 33
        makeSlider("ppg_env3_decay", "Dcy");            // slider 34
        makeSlider("ppg_env3_sustain", "Sus");           // slider 35
        makeSlider("ppg_env3_release", "Rel");           // slider 36

        // --- LFO: sliders 37-42, combo 4 ---
        makeCombo("ppg_lfo_wave", "LFO Wave");          // combo 4
        makeSlider("ppg_lfo_rate", "Rate");             // slider 37
        makeSlider("ppg_lfo_delay", "Delay");           // slider 38
        makeSlider("ppg_lfo_pitch_amt", "Pitch");       // slider 39
        makeSlider("ppg_lfo_cutoff_amt", "Cutoff");     // slider 40
        makeSlider("ppg_lfo_amp_amt", "Amp");           // slider 41

        // --- VCA + Performance: sliders 42-46 ---
        makeSlider("ppg_vca_level", "VCA Level");       // slider 42
        makeSlider("ppg_vca_vel_sens", "Vel Sens");     // slider 43
        makeSlider("ppg_bend_range", "Bend Range");     // slider 44
        makeSlider("ppg_porta_time", "Porta Time");     // slider 45
        makeCombo("ppg_porta_mode", "Porta Mode");       // combo 5

        if (engine != nullptr && transport != nullptr)
        {
            patternStrip = std::make_unique<ui::PatternStrip>(*engine, *transport, kPPGWave,
                                                              juce::Colour(ui::Colours::ppgwave));
            addAndMakeVisible(*patternStrip);
        }

        voicePresetBar = std::make_unique<ui::VoicePresetBar>(apvts, "PPGWave",
                                                              juce::Colour(ui::Colours::ppgwave));
        addAndMakeVisible(*voicePresetBar);

        setSize(1200, 650);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(ui::Colours::bgDark));

        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::ppgwave));
        g.drawText("PPG WAVE 2.2", 16, 4, 400, 24, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
        g.setColour(juce::Colour(ui::Colours::textSecondary));
        g.drawText("Pad / Crystal - Wavetable, 8-Voice Polyphonic", 16, 26, 400, 16,
                   juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::ppgwave));
        g.drawText("PPG", getWidth() - 120, 8, 110, 24, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(56);

        if (voicePresetBar)
        {
            voicePresetBar->setBounds(area.removeFromTop(36));
            area.removeFromTop(4);
        }

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

        // Row 1: Osc A + Osc B
        int row1H = (area.getHeight() - gap * 5) / 5;
        auto row1 = area.removeFromTop(row1H);
        int halfW = (totalW - gap) / 2;
        oscASection.setBounds(row1.removeFromLeft(halfW));
        row1.removeFromLeft(gap);
        oscBSection.setBounds(row1);
        area.removeFromTop(gap);

        // Row 2: Sub/Noise + Mixer + Filter
        auto row2 = area.removeFromTop(row1H);
        int thirdW = (totalW - gap * 2) / 3;
        subSection.setBounds(row2.removeFromLeft(thirdW));
        row2.removeFromLeft(gap);
        mixerSection.setBounds(row2.removeFromLeft(thirdW));
        row2.removeFromLeft(gap);
        filterSection.setBounds(row2);
        area.removeFromTop(gap);

        // Row 3: Envelopes (3 x 4 knobs)
        auto row3 = area.removeFromTop(row1H);
        envSection.setBounds(row3);
        area.removeFromTop(gap);

        // Row 4: LFO + Performance
        auto row4 = area;
        int lfoW = (totalW * 2) / 3;
        lfoSection.setBounds(row4.removeFromLeft(lfoW));
        row4.removeFromLeft(gap);
        perfSection.setBounds(row4);

        // Position knobs in their sections
        auto posKnob = [&](int idx, int x, int y) {
            sliders[idx]->setBounds(x, y, knobW, knobH);
            sliderLabels[idx]->setBounds(x - 4, y + knobH - 3, knobW + 8, 12);
        };

        auto posCombo = [&](int idx, int x, int y) {
            combos[idx]->setBounds(x, y + 8, comboW, comboH);
            comboLabels[idx]->setBounds(x, y - 4, comboW, 12);
        };

        // Osc A layout
        {
            auto c = oscASection.getContentArea();
            int x = oscASection.getX() + c.getX();
            int y = oscASection.getY() + c.getY();
            posCombo(0, x, y);
            for (int i = 0; i < 6; ++i)
                posKnob(i, x + comboW + gap + i * knobPitch, y);
        }

        // Osc B layout
        {
            auto c = oscBSection.getContentArea();
            int x = oscBSection.getX() + c.getX();
            int y = oscBSection.getY() + c.getY();
            posCombo(1, x, y);
            for (int i = 0; i < 6; ++i)
                posKnob(6 + i, x + comboW + gap + i * knobPitch, y);
        }

        // Sub / Noise
        {
            auto c = subSection.getContentArea();
            int x = subSection.getX() + c.getX();
            int y = subSection.getY() + c.getY();
            posCombo(2, x, y);
            for (int i = 0; i < 3; ++i)
                posKnob(12 + i, x + comboW + gap + i * knobPitch, y);
        }

        // Mixer
        {
            auto c = mixerSection.getContentArea();
            int x = mixerSection.getX() + c.getX();
            int y = mixerSection.getY() + c.getY();
            for (int i = 0; i < 5; ++i)
                posKnob(15 + i, x + i * knobPitch, y);
        }

        // Filter
        {
            auto c = filterSection.getContentArea();
            int x = filterSection.getX() + c.getX();
            int y = filterSection.getY() + c.getY();
            for (int i = 0; i < 5; ++i)
                posKnob(20 + i, x + i * knobPitch, y);
            posCombo(3, x + 5 * knobPitch + gap, y);
        }

        // Envelopes (3 envelopes x 4 ADSR knobs)
        {
            auto c = envSection.getContentArea();
            int x = envSection.getX() + c.getX();
            int y = envSection.getY() + c.getY();
            // Env 1 (VCF)
            for (int i = 0; i < 4; ++i)
                posKnob(25 + i, x + i * knobPitch, y);
            // Env 2 (VCA)
            int env2x = x + 4 * knobPitch + gap * 3;
            for (int i = 0; i < 4; ++i)
                posKnob(29 + i, env2x + i * knobPitch, y);
            // Env 3 (Wave)
            int env3x = env2x + 4 * knobPitch + gap * 3;
            for (int i = 0; i < 4; ++i)
                posKnob(33 + i, env3x + i * knobPitch, y);
        }

        // LFO
        {
            auto c = lfoSection.getContentArea();
            int x = lfoSection.getX() + c.getX();
            int y = lfoSection.getY() + c.getY();
            posCombo(4, x, y);
            for (int i = 0; i < 5; ++i)
                posKnob(37 + i, x + comboW + gap + i * knobPitch, y);
        }

        // Performance (VCA + perf params)
        {
            auto c = perfSection.getContentArea();
            int x = perfSection.getX() + c.getX();
            int y = perfSection.getY() + c.getY();
            for (int i = 0; i < 4; ++i)
                posKnob(42 + i, x + i * knobPitch, y);
            posCombo(5, x + 4 * knobPitch + gap, y);
        }
    }

private:
    ui::SectionBox oscASection, oscBSection, subSection, mixerSection,
                   filterSection, envSection, lfoSection, perfSection;

    std::unique_ptr<ui::PatternStrip> patternStrip;
    std::unique_ptr<ui::VoicePresetBar> voicePresetBar;

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<juce::Label> sliderLabels;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<juce::Label> comboLabels;
};

} // namespace axelf::ppgwave
