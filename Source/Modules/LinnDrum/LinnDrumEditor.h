#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "LinnDrumParams.h"
#include "../../UI/AxelFColours.h"
#include "../../UI/SectionBox.h"
#include "../../UI/DrumStepGrid.h"

namespace axelf::linndrum
{

class LinnDrumEditor : public juce::Component
{
public:
    LinnDrumEditor(juce::AudioProcessorValueTreeState& apvts,
                   PatternEngine* engine = nullptr, GlobalTransport* transport = nullptr)
        : globalSection("GLOBAL", juce::Colour(ui::Colours::linn)),
          patternEngine(engine)
    {
        addAndMakeVisible(globalSection);

        // Bank selector ComboBox
        bankBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(ui::Colours::bgControl));
        bankBox.setColour(juce::ComboBox::textColourId, juce::Colour(ui::Colours::textPrimary));
        bankBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(ui::Colours::linn).withAlpha(0.4f));
        const auto& bankNames = getBankNames();
        for (int i = 0; i < bankNames.size(); ++i)
            bankBox.addItem(bankNames[i], i + 1);
        addAndMakeVisible(bankBox);
        bankAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, "linn_bank", bankBox);

        bankLabel.setText("BANK", juce::dontSendNotification);
        bankLabel.setJustificationType(juce::Justification::centredRight);
        bankLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        bankLabel.setColour(juce::Label::textColourId, juce::Colour(ui::Colours::linn));
        addAndMakeVisible(bankLabel);

        if (engine != nullptr && transport != nullptr)
        {
            stepGrid = std::make_unique<ui::DrumStepGrid>(*engine, *transport);
            addAndMakeVisible(*stepGrid);

            // Clear pattern button
            clearBtn.setButtonText("CLEAR");
            clearBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(ui::Colours::bgControl));
            clearBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(ui::Colours::textSecondary));
            clearBtn.onClick = [this]()
            {
                patternEngine->getDrumPattern().clear();
                stepGrid->repaint();
            };
            addAndMakeVisible(clearBtn);
        }

        auto makeSlider = [&](const juce::String& paramId, const juce::String& label,
                              juce::Slider::SliderStyle style = juce::Slider::RotaryHorizontalVerticalDrag)
        {
            auto* slider = new juce::Slider(style, juce::Slider::TextBoxBelow);
            slider->setName(paramId);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 46, 12);
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

        // Global controls (sliders 0-1)
        makeSlider("linn_swing", "Swing");
        makeSlider("linn_master_tune", "Master Tune");

        // Per-drum controls: 15 drums x 4 params = 60 sliders (indices 2-61)
        const auto& names = getDrumNames();
        for (int d = 0; d < names.size(); ++d)
        {
            auto prefix = "linn_" + names[d] + "_";
            makeSlider(prefix + "level", "Level");
            makeSlider(prefix + "tune", "Tune");
            makeSlider(prefix + "decay", "Decay");
            makeSlider(prefix + "pan", "Pan");
        }
        setSize(1200, 700);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(ui::Colours::bgDark));

        g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::linn));
        g.drawText("LINNDRUM", getWidth() - 200, 8, 190, 24, juce::Justification::centredRight);

        // Drum pad backgrounds
        const auto& names = getDrumNames();
        int padW = 210, padH = 96;
        int startX = 14, startY = 70;

        for (int d = 0; d < names.size(); ++d)
        {
            int row = d / 5;
            int col = d % 5;
            int x = startX + col * (padW + 6);
            int y = startY + row * (padH + 4);

            g.setColour(juce::Colour(ui::Colours::bgSection));
            g.fillRoundedRectangle((float)x, (float)y, (float)padW, (float)padH, 5.0f);

            g.setColour(juce::Colour(ui::Colours::linn).withAlpha(0.3f));
            g.fillRoundedRectangle((float)x, (float)y, (float)padW, 16.0f, 5.0f);
            g.fillRect((float)x, (float)y + 8.0f, (float)padW, 8.0f);

            g.setColour(juce::Colour(ui::Colours::textPrimary));
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(names[d].toUpperCase(), x + 4, y + 1, padW - 8, 14, juce::Justification::centredLeft);
        }

        // Step grid section label
        int gridTop = startY + 3 * (padH + 4) + 4;
        g.setColour(juce::Colour(ui::Colours::linn).withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("STEP SEQUENCER", 14, gridTop, 200, 18, juce::Justification::centredLeft);
    }

    void resized() override
    {
        int knobW = 44, knobH = 44;

        // Title area
        auto area = getLocalBounds().reduced(10);
        auto topRow = area.removeFromTop(58);

        // Global section (compact)
        globalSection.setBounds(topRow.removeFromLeft(190).withTrimmedTop(2));
        topRow.removeFromLeft(8);

        // Clear button
        clearBtn.setBounds(topRow.removeFromLeft(60).withHeight(26).withY(topRow.getY() + 16));

        // Bank selector — right of clear button
        bankLabel.setBounds(topRow.removeFromLeft(40).withHeight(20).withY(topRow.getY() + 19));
        topRow.removeFromLeft(4);
        bankBox.setBounds(topRow.removeFromLeft(160).withHeight(24).withY(topRow.getY() + 17));

        // Position global knobs
        auto gC = globalSection.getContentArea();
        int gx = globalSection.getX() + gC.getX();
        int gy = globalSection.getY() + gC.getY();
        for (int i = 0; i < 2; ++i)
        {
            sliders[i]->setBounds(gx + i * 84, gy, 60, 28);
            labels[i]->setBounds(gx + i * 84 - 5, gy + 25, 70, 12);
        }

        // Compact drum grid: 5 columns x 3 rows
        int padW = 210, padH = 96;
        int startX = 14, startY = 70;

        for (int d = 0; d < 15; ++d)
        {
            int row = d / 5;
            int col = d % 5;
            int baseX = startX + col * (padW + 6) + 4;
            int baseY = startY + row * (padH + 4) + 18;
            int base = 2 + d * 4;

            for (int p = 0; p < 4; ++p)
            {
                sliders[base + p]->setBounds(baseX + p * 50, baseY, knobW, knobH);
                labels[base + p]->setBounds(baseX + p * 50 - 4, baseY + knobH - 1, 56, 12);
            }
        }

        // Step grid fills the bottom area
        if (stepGrid)
        {
            int gridTop = startY + 3 * (padH + 4) + 22;
            stepGrid->setBounds(14, gridTop, getWidth() - 28, getHeight() - gridTop - 8);
        }
    }

private:
    ui::SectionBox globalSection;
    std::unique_ptr<ui::DrumStepGrid> stepGrid;
    PatternEngine* patternEngine = nullptr;

    juce::TextButton clearBtn;
    juce::ComboBox bankBox;
    juce::Label bankLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bankAttachment;

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    juce::OwnedArray<juce::Label> labels;
};

} // namespace axelf::linndrum
