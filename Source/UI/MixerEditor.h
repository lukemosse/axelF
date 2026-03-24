#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/AxelFColours.h"
#include "../UI/SectionBox.h"
#include "../Common/MasterMixer.h"

namespace axelf
{

class MixerEditor : public juce::Component, private juce::Timer
{
public:
    explicit MixerEditor(MasterMixer& mix) : mixer(mix)
    {
        startTimerHz(30); // refresh meters at 30fps
        static const char* names[] = { "Jupiter-8", "Moog 15", "JX-3P", "DX7", "PPG Wave", "LinnDrum" };
        static const juce::uint32 accents[] = {
            ui::Colours::jupiter, ui::Colours::moog, ui::Colours::jx3p,
            ui::Colours::dx7, ui::Colours::ppgwave, ui::Colours::linn
        };

        for (int i = 0; i < 6; ++i)
        {
            auto* strip = new ChannelStripComp(names[i], juce::Colour(accents[i]), mixer.getStrip(i));
            strips.add(strip);
            addAndMakeVisible(strip);
        }

        masterSection = std::make_unique<ui::SectionBox>("MASTER", juce::Colour(ui::Colours::mixer));
        addAndMakeVisible(*masterSection);

        masterVol.setSliderStyle(juce::Slider::LinearVertical);
        masterVol.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        masterVol.setRange(0.0, 1.5, 0.01);
        masterVol.setValue(1.0);
        masterVol.setColour(juce::Slider::backgroundColourId, juce::Colour(ui::Colours::knobTrack));
        addAndMakeVisible(masterVol);

        masterLabel.setText("Master", juce::dontSendNotification);
        masterLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        masterLabel.setColour(juce::Label::textColourId, juce::Colour(ui::Colours::textLabel));
        masterLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(masterLabel);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(ui::Colours::bgDark));

        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.setColour(juce::Colour(ui::Colours::mixer));
        g.drawText("MASTER MIXER", 16, 4, 400, 24, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
        g.setColour(juce::Colour(ui::Colours::textSecondary));
        g.drawText("6-Channel Mix + Aux Sends + Output Routing", 16, 26, 450, 16,
                   juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        area.removeFromTop(40); // header space

        auto stripWidth = (area.getWidth() - 120) / 6;

        for (int i = 0; i < 6; ++i)
            strips[i]->setBounds(area.removeFromLeft(stripWidth).reduced(4));

        area.removeFromLeft(8);
        masterSection->setBounds(area);

        auto masterArea = masterSection->getContentArea().translated(masterSection->getX(), masterSection->getY());
        masterVol.setBounds(masterArea.getCentreX() - 20, masterArea.getY() + 10,
                            40, masterArea.getHeight() - 40);
        masterLabel.setBounds(masterArea.getCentreX() - 30, masterArea.getBottom() - 24, 60, 16);
    }

private:
    MasterMixer& mixer;

    void timerCallback() override
    {
        for (int i = 0; i < 6; ++i)
            strips[i]->updateMeter(mixer.getStrip(i).peakLevel);
    }

    // Simple vertical level meter
    struct LevelMeter : public juce::Component
    {
        void setLevel(float newLevel) { level = newLevel; repaint(); }

        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced(1.f);
            g.setColour(juce::Colour(0xFF0a0e14));
            g.fillRoundedRectangle(b, 2.f);

            float h = b.getHeight() * juce::jlimit(0.f, 1.f, level);
            auto meterRect = b.withTop(b.getBottom() - h);

            // Green → yellow → red gradient
            auto colour = level < 0.7f ? juce::Colour(ui::Colours::accentGreen)
                        : level < 0.9f ? juce::Colour(0xFFddaa00)
                                       : juce::Colour(ui::Colours::accentRed);
            g.setColour(colour.withAlpha(0.8f));
            g.fillRoundedRectangle(meterRect, 2.f);
        }

    private:
        float level = 0.f;
    };

    struct ChannelStripComp : public juce::Component
    {
        ChannelStripComp(const juce::String& name, juce::Colour accent, MasterMixer::ChannelStrip& stripRef)
            : strip(stripRef), accentColour(accent)
        {
            nameLabel.setText(name, juce::dontSendNotification);
            nameLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
            nameLabel.setColour(juce::Label::textColourId, accent);
            nameLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(nameLabel);

            levelSlider.setSliderStyle(juce::Slider::LinearVertical);
            levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
            levelSlider.setRange(0.0, 1.5, 0.01);
            levelSlider.setValue(strip.level);
            levelSlider.onValueChange = [this] { strip.level = (float)levelSlider.getValue(); };
            addAndMakeVisible(levelSlider);

            panKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            panKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
            panKnob.setRange(-1.0, 1.0, 0.01);
            panKnob.setValue(strip.pan);
            panKnob.onValueChange = [this] { strip.pan = (float)panKnob.getValue(); };
            addAndMakeVisible(panKnob);

            panLabel.setText("Pan", juce::dontSendNotification);
            panLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
            panLabel.setColour(juce::Label::textColourId, juce::Colour(ui::Colours::textLabel));
            panLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(panLabel);

            muteButton.setButtonText("MUTE");
            muteButton.setClickingTogglesState(true);
            muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(ui::Colours::bgControl));
            muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(ui::Colours::accentRed));
            muteButton.setColour(juce::TextButton::textColourOffId, juce::Colour(ui::Colours::textSecondary));
            muteButton.setColour(juce::TextButton::textColourOnId, juce::Colour(ui::Colours::textPrimary));
            muteButton.onClick = [this] { strip.mute = muteButton.getToggleState(); };
            addAndMakeVisible(muteButton);

            soloButton.setButtonText("SOLO");
            soloButton.setClickingTogglesState(true);
            soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour(ui::Colours::bgControl));
            soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(ui::Colours::accentGreen));
            soloButton.setColour(juce::TextButton::textColourOffId, juce::Colour(ui::Colours::textSecondary));
            soloButton.setColour(juce::TextButton::textColourOnId, juce::Colour(ui::Colours::textPrimary));
            soloButton.onClick = [this] { strip.solo = soloButton.getToggleState(); };
            addAndMakeVisible(soloButton);

            addAndMakeVisible(meter);
        }

        void updateMeter(float peak) { meter.setLevel(peak); }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour(juce::Colour(ui::Colours::bgSection));
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(accentColour.withAlpha(0.3f));
            g.fillRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), 24.0f, 6.0f);
            g.fillRect(bounds.getX(), bounds.getY() + 12.0f, bounds.getWidth(), 12.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(6);
            nameLabel.setBounds(area.removeFromTop(22));
            area.removeFromTop(4);

            // Large M/S buttons — full width, stacked
            muteButton.setBounds(area.removeFromBottom(28).reduced(2));
            soloButton.setBounds(area.removeFromBottom(28).reduced(2));
            area.removeFromBottom(2);

            auto panArea = area.removeFromBottom(70);
            panKnob.setBounds(panArea.reduced(10, 0).withHeight(55));
            panLabel.setBounds(panArea.getX(), panArea.getBottom() - 14, panArea.getWidth(), 14);

            levelSlider.setBounds(area.reduced(area.getWidth() / 4, 4));
            meter.setBounds(area.getRight() - 10, area.getY() + 4, 8, area.getHeight() - 8);
        }

        MasterMixer::ChannelStrip& strip;
        juce::Colour accentColour;
        juce::Label nameLabel;
        juce::Slider levelSlider;
        juce::Slider panKnob;
        juce::Label panLabel;
        juce::TextButton muteButton;
        juce::TextButton soloButton;
        LevelMeter meter;
    };

    juce::OwnedArray<ChannelStripComp> strips;
    std::unique_ptr<ui::SectionBox> masterSection;
    juce::Slider masterVol;
    juce::Label masterLabel;
};

} // namespace axelf
