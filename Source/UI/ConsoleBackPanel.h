#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "AxelFColours.h"
#include "../Common/MasterMixer.h"

namespace axelf::ui
{

// ── Modal popup for "set-and-forget" analog console settings ──
// Per-channel: saturation color
// Global: crosstalk, noise floor, summing color, drift
// Includes save / load / delete of console presets (~/.AxelF/Console/)
class ConsoleBackPanel : public juce::Component
{
public:
    ConsoleBackPanel (juce::AudioProcessorValueTreeState& mixApvts,
                      juce::AudioProcessorValueTreeState& fxApvts)
        : mixApvts_ (mixApvts), fxApvts_ (fxApvts)
    {
        // ── Preset directory ─────────────────────────────────
        presetDir_ = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("AxelF").getChildFile ("Console");
        presetDir_.createDirectory();

        // ── Per-channel saturation color combos ──────────────
        static const char* names[] = { "JUP8", "MOOG", "JX3P", "DX7", "PPG", "LINN" };
        static const juce::uint32 accents[] = {
            Colours::jupiter, Colours::moog, Colours::jx3p,
            Colours::dx7, Colours::ppgwave, Colours::linn
        };

        for (int i = 0; i < 6; ++i)
        {
            auto& ch = channelUI[i];
            ch.name = names[i];
            ch.accent = juce::Colour (accents[i]);

            ch.satColorBox.addItemList ({ "Clean", "Transformer", "Console", "Tape" }, 1);
            ch.satColorBox.setJustificationType (juce::Justification::centred);
            ch.satColorBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF0e1224));
            ch.satColorBox.setColour (juce::ComboBox::outlineColourId, ch.accent.withAlpha (0.4f));
            ch.satColorBox.setColour (juce::ComboBox::textColourId, juce::Colour (Colours::textPrimary));
            addAndMakeVisible (ch.satColorBox);

            ch.satColorAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                mixApvts, MixerParamIDs::satColorID (i), ch.satColorBox);
        }

        // ── Global console knobs ─────────────────────────────
        auto initKnob = [&](juce::Slider& knob, float lo, float hi, juce::uint32 col)
        {
            knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 14);
            knob.setRange (lo, hi, 0.01);
            knob.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (col).withAlpha (0.6f));
            knob.setColour (juce::Slider::textBoxTextColourId, juce::Colour (Colours::textSecondary));
            knob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
            addAndMakeVisible (knob);
        };

        initKnob (crosstalkKnob,  0.0f, 1.0f, Colours::accentOrange);
        initKnob (noiseFloorKnob, 0.0f, 1.0f, Colours::accentOrange);
        initKnob (summingKnob,    0.0f, 1.0f, Colours::accentGold);
        initKnob (driftKnob,      0.0f, 1.0f, Colours::accentGold);

        crosstalkAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (mixApvts, MixerParamIDs::kCrosstalk, crosstalkKnob);
        noiseFloorAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (mixApvts, MixerParamIDs::kNoiseFloor, noiseFloorKnob);
        summingAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_summing", summingKnob);
        driftAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (fxApvts, "fx_master_drift", driftKnob);

        // Summing color combo
        summingColorBox.addItemList ({ "Clean", "Neve", "SSL", "API" }, 1);
        summingColorBox.setJustificationType (juce::Justification::centred);
        summingColorBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF0e1224));
        summingColorBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (Colours::accentGold).withAlpha (0.4f));
        summingColorBox.setColour (juce::ComboBox::textColourId, juce::Colour (Colours::textPrimary));
        addAndMakeVisible (summingColorBox);

        summingColorAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            fxApvts, "fx_master_summing_color", summingColorBox);

        // Close button
        closeBtn.setButtonText ("X");
        closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2a2a44));
        closeBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::textSecondary));
        closeBtn.onClick = [this]() { setVisible (false); };
        addAndMakeVisible (closeBtn);

        // ── Preset controls ──────────────────────────────────
        presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xFF0e1224));
        presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (Colours::accentGold).withAlpha (0.4f));
        presetBox.setColour (juce::ComboBox::textColourId, juce::Colour (Colours::textPrimary));
        presetBox.setTextWhenNothingSelected ("(no preset)");
        presetBox.onChange = [this]()
        {
            int idx = presetBox.getSelectedItemIndex();
            if (idx >= 0 && idx < presetFiles_.size())
                loadSetup (presetFiles_[idx]);
        };
        addAndMakeVisible (presetBox);

        auto initBtn = [&](juce::TextButton& btn, const juce::String& text)
        {
            btn.setButtonText (text);
            btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF1e1e38));
            btn.setColour (juce::TextButton::textColourOffId, juce::Colour (Colours::accentGold));
            addAndMakeVisible (btn);
        };
        initBtn (saveBtn, "SAVE");
        initBtn (deleteBtn, "DEL");

        saveBtn.onClick   = [this]() { saveSetup(); };
        deleteBtn.onClick = [this]() { deleteSetup(); };

        refreshPresetList();
    }

    void open()
    {
        refreshPresetList();
        setVisible (true);
        toFront (true);
    }

    // ── Preset persistence ───────────────────────────────────

    void refreshPresetList()
    {
        presetFiles_ = presetDir_.findChildFiles (juce::File::findFiles, false, "*.xml");
        std::sort (presetFiles_.begin(), presetFiles_.end());

        presetBox.clear (juce::dontSendNotification);
        for (int i = 0; i < presetFiles_.size(); ++i)
            presetBox.addItem (presetFiles_[i].getFileNameWithoutExtension(), i + 1);
    }

    void saveSetup()
    {
        auto aw = std::make_shared<juce::AlertWindow> (
            "Save Console Setup", "Enter a name:", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", "", "Name:");
        aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, aw] (int result)
            {
                if (result != 1) return;
                auto name = aw->getTextEditorContents ("name").trim();
                if (name.isEmpty()) return;

                auto file = presetDir_.getChildFile (
                    juce::File::createLegalFileName (name) + ".xml");
                writeSetupToFile (file);
                refreshPresetList();

                for (int i = 0; i < presetFiles_.size(); ++i)
                    if (presetFiles_[i] == file)
                    { presetBox.setSelectedItemIndex (i, juce::dontSendNotification); break; }
            }));
    }

    void deleteSetup()
    {
        int idx = presetBox.getSelectedItemIndex();
        if (idx < 0 || idx >= presetFiles_.size()) return;

        presetFiles_[idx].deleteFile();
        refreshPresetList();
    }

    // ── Paint ────────────────────────────────────────────────

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // Semi-transparent backdrop
        g.setColour (juce::Colour (0xB0000000));
        g.fillRect (b);

        // Panel area (centered card)
        auto card = panelBounds_.toFloat();
        g.setColour (juce::Colour (0xFF151828));
        g.fillRoundedRectangle (card, 8.0f);
        g.setColour (juce::Colour (Colours::borderSubtle));
        g.drawRoundedRectangle (card.reduced (0.5f), 8.0f, 1.0f);

        // Title
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        g.setColour (juce::Colour (Colours::accentGold));
        g.drawText ("CONSOLE BACK PANEL", card.getX(), card.getY() + 10,
                     card.getWidth(), 24, juce::Justification::centred, false);

        // Preset label
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.setColour (juce::Colour (Colours::textLabel));
        g.drawText ("PRESET", panelBounds_.getX() + 16, presetRowY_,
                     50, 22, juce::Justification::centredLeft, false);

        // ── Section: Per-Channel Saturation Color ────────────
        int sectionY = presetRowY_ + 30;
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.setColour (juce::Colour (Colours::textLabel));
        g.drawText ("CHANNEL SATURATION COLOR", panelBounds_.getX() + 16, sectionY,
                     panelBounds_.getWidth() - 32, 16, juce::Justification::centredLeft, false);

        // Channel labels above combos
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        int comboY = sectionY + 20;
        int comboW = (panelBounds_.getWidth() - 32 - 5 * 8) / 6;
        for (int i = 0; i < 6; ++i)
        {
            int cx = panelBounds_.getX() + 16 + i * (comboW + 8);
            g.setColour (channelUI[i].accent);
            g.drawText (channelUI[i].name, cx, comboY, comboW, 14,
                        juce::Justification::centred, false);
        }

        // ── Section: Global Console ──────────────────────────
        int globalY = comboY + 50;
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        g.setColour (juce::Colour (Colours::textLabel));
        g.drawText ("CONSOLE CHARACTER", panelBounds_.getX() + 16, globalY,
                     panelBounds_.getWidth() - 32, 16, juce::Justification::centredLeft, false);

        // Knob labels
        g.setFont (juce::Font (juce::FontOptions (8.0f)));
        g.setColour (juce::Colour (Colours::textSecondary));
        int knobRow = globalY + 20;
        int knobSize = 52;
        int spacing = (panelBounds_.getWidth() - 32 - 5 * knobSize) / 4;
        const char* labels[] = { "CROSSTALK", "NOISE", "SUMMING", "SUM COLOR", "DRIFT" };
        for (int i = 0; i < 5; ++i)
        {
            int kx = panelBounds_.getX() + 16 + i * (knobSize + spacing);
            int labelY = knobRow + knobSize + 16 + 4;
            g.drawText (labels[i], kx, labelY, knobSize, 12,
                        juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();

        // Centered card panel (taller to fit preset row)
        int panelW = std::min (520, b.getWidth() - 40);
        int panelH = 254;
        panelBounds_ = juce::Rectangle<int> ((b.getWidth() - panelW) / 2,
                                              (b.getHeight() - panelH) / 2,
                                              panelW, panelH);

        // Close button
        closeBtn.setBounds (panelBounds_.getRight() - 28, panelBounds_.getY() + 6, 22, 22);

        // ── Preset row ───────────────────────────────────────
        presetRowY_ = panelBounds_.getY() + 38;
        int px = panelBounds_.getX() + 66;             // after "PRESET" label
        int pw = panelBounds_.getWidth() - 66 - 16;    // right margin
        presetBox.setBounds  (px, presetRowY_, pw - 108, 22);
        saveBtn.setBounds    (px + pw - 102, presetRowY_, 48, 22);
        deleteBtn.setBounds  (px + pw - 48,  presetRowY_, 42, 22);

        // Per-channel sat color combos
        int comboY = presetRowY_ + 30 + 20 + 14;
        int comboW = (panelBounds_.getWidth() - 32 - 5 * 8) / 6;
        for (int i = 0; i < 6; ++i)
        {
            int cx = panelBounds_.getX() + 16 + i * (comboW + 8);
            channelUI[i].satColorBox.setBounds (cx, comboY, comboW, 22);
        }

        // Global knobs row
        int globalY = comboY + 36;
        int knobSize = 52;
        int spacing = (panelBounds_.getWidth() - 32 - 5 * knobSize) / 4;

        int kx0 = panelBounds_.getX() + 16;
        crosstalkKnob.setBounds  (kx0,                              globalY, knobSize, knobSize + 16);
        noiseFloorKnob.setBounds (kx0 + 1 * (knobSize + spacing),  globalY, knobSize, knobSize + 16);
        summingKnob.setBounds    (kx0 + 2 * (knobSize + spacing),  globalY, knobSize, knobSize + 16);
        summingColorBox.setBounds(kx0 + 3 * (knobSize + spacing),  globalY + 14, knobSize, 22);
        driftKnob.setBounds      (kx0 + 4 * (knobSize + spacing),  globalY, knobSize, knobSize + 16);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Click outside the card closes the panel
        if (! panelBounds_.contains (e.getPosition()))
            setVisible (false);
    }

private:
    // ── Preset serialization ─────────────────────────────────

    void writeSetupToFile (const juce::File& file)
    {
        juce::XmlElement xml ("ConsoleSetup");

        auto writeParam = [&] (juce::AudioProcessorValueTreeState& tree,
                               const juce::String& id)
        {
            if (auto* p = tree.getParameter (id))
            {
                auto* e = xml.createNewChildElement ("Param");
                e->setAttribute ("id", id);
                e->setAttribute ("value", (double) p->getValue());
            }
        };

        for (int i = 0; i < 6; ++i)
        {
            writeParam (mixApvts_, MixerParamIDs::phaseID (i));
            writeParam (mixApvts_, MixerParamIDs::hpfID (i));
            writeParam (mixApvts_, MixerParamIDs::satAmountID (i));
            writeParam (mixApvts_, MixerParamIDs::satColorID (i));
        }

        writeParam (mixApvts_, MixerParamIDs::kCrosstalk);
        writeParam (mixApvts_, MixerParamIDs::kNoiseFloor);

        for (auto& id : { juce::String ("fx_master_summing"),
                          juce::String ("fx_master_summing_color"),
                          juce::String ("fx_master_parallel_blend"),
                          juce::String ("fx_master_width"),
                          juce::String ("fx_master_drift") })
            writeParam (fxApvts_, id);

        xml.writeTo (file);
    }

    void loadSetup (const juce::File& file)
    {
        auto xml = juce::parseXML (file);
        if (! xml || xml->getTagName() != "ConsoleSetup") return;

        for (auto* elem : xml->getChildIterator())
        {
            if (elem->getTagName() != "Param") continue;
            auto id  = elem->getStringAttribute ("id");
            auto val = (float) elem->getDoubleAttribute ("value");

            if (auto* p = mixApvts_.getParameter (id))
                p->setValueNotifyingHost (val);
            else if (auto* p2 = fxApvts_.getParameter (id))
                p2->setValueNotifyingHost (val);
        }
    }

    // ── Members ──────────────────────────────────────────────

    juce::AudioProcessorValueTreeState& mixApvts_;
    juce::AudioProcessorValueTreeState& fxApvts_;

    juce::File              presetDir_;
    juce::Array<juce::File> presetFiles_;
    juce::ComboBox          presetBox;
    juce::TextButton        saveBtn, deleteBtn;
    int                     presetRowY_ = 0;

    juce::Rectangle<int> panelBounds_;
    juce::TextButton closeBtn;

    struct ChannelUI
    {
        juce::String name;
        juce::Colour accent;
        juce::ComboBox satColorBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satColorAttach;
    };
    ChannelUI channelUI[6];

    juce::Slider crosstalkKnob, noiseFloorKnob, summingKnob, driftKnob;
    juce::ComboBox summingColorBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> crosstalkAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseFloorAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> summingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driftAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> summingColorAttach;
};

} // namespace axelf::ui
