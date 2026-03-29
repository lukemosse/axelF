#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "../../UI/AxelFColours.h"
#include "../../UI/SectionBox.h"
#include "../../UI/PatternStrip.h"
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
          env1Section("FILTER CONTOUR", juce::Colour(ui::Colours::moog)),
          env2Section("LOUDNESS CONTOUR", juce::Colour(ui::Colours::moog)),
          lfoSection("MOD / GLIDE", juce::Colour(ui::Colours::moog))
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

        // ── VCO section ────────────────────────────────────────────────────
        makeCombo("moog_vco1_waveform", "VCO-1 Wave");    // combo 0
        makeCombo("moog_vco1_footage", "VCO-1 Range");    // combo 1
        makeSlider("moog_vco1_level", "VCO-1 Lvl");        // slider 0
        makeSlider("moog_vco1_pw", "VCO-1 PW");            // slider 1

        makeCombo("moog_vco2_waveform", "VCO-2 Wave");    // combo 2
        makeCombo("moog_vco2_footage", "VCO-2 Range");    // combo 3
        makeSlider("moog_vco2_detune", "VCO-2 Det");       // slider 2
        makeSlider("moog_vco2_level", "VCO-2 Lvl");        // slider 3
        makeSlider("moog_vco2_pw", "VCO-2 PW");            // slider 4

        makeCombo("moog_vco3_waveform", "VCO-3 Wave");    // combo 4
        makeCombo("moog_vco3_footage", "VCO-3 Range");    // combo 5
        makeSlider("moog_vco3_detune", "VCO-3 Det");       // slider 5
        makeSlider("moog_vco3_level", "VCO-3 Lvl");        // slider 6
        makeSlider("moog_vco3_pw", "VCO-3 PW");            // slider 7
        makeCombo("moog_vco3_ctrl", "Osc3 Ctrl");          // combo 6

        // ── Mixer section (Noise + Feedback) ────────────────────────────────
        makeSlider("moog_noise_level", "Noise");            // slider 8
        makeCombo("moog_noise_color", "Noise Color");      // combo 7
        makeSlider("moog_feedback", "Feedback");            // slider 9

        // ── VCF section ─────────────────────────────────────────────────────
        makeSlider("moog_vcf_cutoff", "Cutoff");            // slider 10
        makeSlider("moog_vcf_resonance", "Resonance");     // slider 11
        makeSlider("moog_vcf_drive", "Drive");              // slider 12
        makeSlider("moog_vcf_env_depth", "Env Depth");     // slider 13
        makeCombo("moog_vcf_key_track", "Key Track");      // combo 8

        // ── Filter Contour (ADS — no release) ──────────────────────────────
        makeSlider("moog_env1_attack", "Atk");              // slider 14
        makeSlider("moog_env1_decay", "Dec/Rel");           // slider 15
        makeSlider("moog_env1_sustain", "Sus");             // slider 16

        // ── Loudness Contour (ADS — no release) ────────────────────────────
        makeSlider("moog_env2_attack", "Atk");              // slider 17
        makeSlider("moog_env2_decay", "Dec/Rel");           // slider 18
        makeSlider("moog_env2_sustain", "Sus");             // slider 19

        // ── Mod / Glide section ─────────────────────────────────────────────
        makeSlider("moog_glide_time", "Glide");             // slider 20
        makeSlider("moog_lfo_rate", "LFO Rate");            // slider 21
        makeCombo("moog_lfo_waveform", "LFO Wave");        // combo 9
        makeSlider("moog_lfo_pitch_amount", "LFO>Pitch");  // slider 22
        makeSlider("moog_vcf_lfo_amount", "LFO>VCF");      // slider 23
        makeSlider("moog_master_vol", "Master");            // slider 24

        if (engine != nullptr && transport != nullptr)
        {
            patternStrip = std::make_unique<ui::PatternStrip>(*engine, *transport, kMoog15,
                                                              juce::Colour(ui::Colours::moog));
            addAndMakeVisible(*patternStrip);
        }

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
        g.drawText("MINIMOOG MODEL D", 16, 4, 400, 24, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
        g.setColour(juce::Colour(ui::Colours::textSecondary));
        g.drawText("Bass - Monophonic / 3 VCO / Click-Free Contours", 16, 26, 500, 16,
                   juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::moog));
        g.drawText("MINIMOOG MODEL D", getWidth() - 280, 8, 270, 24, juce::Justification::centredRight);
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

        // Row 1: VCO (50%) + Mixer (16%) + VCF (34%)
        int row1H = std::min(190, (area.getHeight() - 16) * 38 / 100);
        auto row1 = area.removeFromTop(row1H);
        int vcoW = totalW * 50 / 100;
        int mixW = totalW * 16 / 100;
        vcoSection.setBounds(row1.removeFromLeft(vcoW));
        row1.removeFromLeft(gap);
        mixSection.setBounds(row1.removeFromLeft(mixW));
        row1.removeFromLeft(gap);
        vcfSection.setBounds(row1);

        area.removeFromTop(gap);

        // Row 2: Filter Contour + Loudness Contour (equal)
        int envH = std::min(100, (area.getHeight() - 8) * 32 / 100);
        auto envRow = area.removeFromTop(envH);
        int envW = (totalW - gap) / 2;
        env1Section.setBounds(envRow.removeFromLeft(envW));
        envRow.removeFromLeft(gap);
        env2Section.setBounds(envRow);

        area.removeFromTop(gap);

        // Row 3: Mod/Glide (fill remaining)
        lfoSection.setBounds(area);

        // ── Position controls ──
        auto posKnob = [&](int idx, int x, int y) {
            sliders[idx]->setBounds(x, y, knobW, knobH);
            labels[idx]->setBounds(x - 4, y + knobH + 1, knobW + 8, 14);
        };
        auto posCombo = [&](int idx, int x, int y) {
            combos[idx]->setBounds(x, y, comboW, comboH);
            comboLabels[idx]->setBounds(x, y - 14, comboW, 14);
        };

        // VCO section: 3 columns for VCO 1/2/3
        auto vcoC = vcoSection.getContentArea();
        int vx = vcoSection.getX() + vcoC.getX();
        int vy = vcoSection.getY() + vcoC.getY();
        int vcoColW = vcoC.getWidth() / 3;
        // VCO-1: combo0=wave, combo1=range, slider0=level, slider1=PW
        posCombo(0, vx, vy + 4);
        posCombo(1, vx + comboW + 4, vy + 4);
        posKnob(0, vx, vy + 40);
        posKnob(1, vx + knobPitch, vy + 40);
        // VCO-2: combo2=wave, combo3=range, slider2=detune, slider3=level, slider4=PW
        int v2x = vx + vcoColW;
        posCombo(2, v2x, vy + 4);
        posCombo(3, v2x + comboW + 4, vy + 4);
        posKnob(2, v2x, vy + 40);
        posKnob(3, v2x + knobPitch, vy + 40);
        posKnob(4, v2x + knobPitch * 2, vy + 40);
        // VCO-3: combo4=wave, combo5=range, slider5=detune, slider6=level, slider7=PW, combo6=osc3ctrl
        int v3x = vx + vcoColW * 2;
        posCombo(4, v3x, vy + 4);
        posCombo(5, v3x + comboW + 4, vy + 4);
        posKnob(5, v3x, vy + 40);
        posKnob(6, v3x + knobPitch, vy + 40);
        posKnob(7, v3x + knobPitch * 2, vy + 40);
        posCombo(6, v3x, vy + 86);

        // Mixer section: slider8=noise, combo7=noise color, slider9=feedback
        auto mixC = mixSection.getContentArea();
        int mx = mixSection.getX() + mixC.getX();
        int my = mixSection.getY() + mixC.getY();
        posKnob(8, mx, my);
        posCombo(7, mx, my + knobH + 20);
        posKnob(9, mx + knobPitch, my);

        // VCF section: slider10=cutoff, slider11=reso, slider12=drive, slider13=envdepth, combo8=keytrack
        auto vcfC = vcfSection.getContentArea();
        int fx = vcfSection.getX() + vcfC.getX();
        int fy = vcfSection.getY() + vcfC.getY();
        for (int i = 0; i < 4; ++i)
            posKnob(10 + i, fx + i * knobPitch, fy);
        posCombo(8, fx, fy + knobH + 20);

        // Filter Contour: slider14=atk, slider15=dec, slider16=sus (3 knobs, no release)
        auto e1C = env1Section.getContentArea();
        int e1x = env1Section.getX() + e1C.getX();
        int e1y = env1Section.getY() + e1C.getY();
        for (int i = 0; i < 3; ++i)
            posKnob(14 + i, e1x + i * knobPitch, e1y);

        // Loudness Contour: slider17=atk, slider18=dec, slider19=sus (3 knobs, no release)
        auto e2C = env2Section.getContentArea();
        int e2x = env2Section.getX() + e2C.getX();
        int e2y = env2Section.getY() + e2C.getY();
        for (int i = 0; i < 3; ++i)
            posKnob(17 + i, e2x + i * knobPitch, e2y);

        // Mod/Glide: slider20=glide, slider21=lfo rate, combo9=lfo wave, slider22=lfo>pitch, slider23=lfo>vcf, slider24=master
        auto lfoC = lfoSection.getContentArea();
        int lx = lfoSection.getX() + lfoC.getX();
        int ly = lfoSection.getY() + lfoC.getY();
        posKnob(20, lx, ly);
        posKnob(21, lx + knobPitch, ly);
        posCombo(9, lx + knobPitch * 2, ly + 14);
        posKnob(22, lx + knobPitch * 3 + comboW + 4, ly);
        posKnob(23, lx + knobPitch * 4 + comboW + 4, ly);
        posKnob(24, lx + knobPitch * 5 + comboW + 8, ly);
    }

private:
    ui::SectionBox vcoSection, mixSection, vcfSection, env1Section, env2Section, lfoSection;
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