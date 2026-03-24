#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common/SceneManager.h"
#include "../Common/PatternEngine.h"
#include "../Common/MasterMixer.h"
#include "AxelFColours.h"

namespace axelf::ui
{

// Compact bar showing scenes A–P (paged 8 at a time) with Save + pattern tools.
class PatternBankBar : public juce::Component
{
public:
    PatternBankBar(SceneManager& sm, PatternEngine& pe, MasterMixer& mx)
        : sceneManager(sm), patternEngine(pe), mixer(mx)
    {
        // ── Scene buttons (A–P, 16 total) ────────────────────
        for (int i = 0; i < kTotalScenes; ++i)
        {
            auto* btn = sceneButtons.add(new juce::TextButton(juce::String::charToString('A' + i)));
            btn->setRadioGroupId(7777);
            btn->setClickingTogglesState(true);
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
            btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(Colours::accentGold).withAlpha(0.5f));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
            btn->setColour(juce::TextButton::textColourOnId, juce::Colour(Colours::textPrimary));
            btn->onClick = [this, i]() { loadBank(i); };
            addAndMakeVisible(btn);
        }
        sceneButtons[0]->setToggleState(true, juce::dontSendNotification);

        // ── Page arrows (show 8 scenes at a time) ────────────
        auto makeNav = [&](juce::TextButton& btn, const juce::String& text)
        {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
            addAndMakeVisible(btn);
        };
        makeNav(prevPageBtn, "<");
        makeNav(nextPageBtn, ">");
        prevPageBtn.onClick = [this] { setPage(page - 1); };
        nextPageBtn.onClick = [this] { setPage(page + 1); };

        // ── Pattern tools ────────────────────────────────────
        auto makeTool = [&](juce::TextButton& btn, const juce::String& text, juce::uint32 col)
        {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(col).withAlpha(0.5f));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
            addAndMakeVisible(btn);
        };
        makeTool(copyBtn,  "CPY", Colours::accentBlue);
        makeTool(pasteBtn, "PST", Colours::accentBlue);
        makeTool(clearBtn, "CLR", Colours::accentRed);

        copyBtn.onClick  = [this] { patternEngine.copyPattern(activeModuleIndex); };
        pasteBtn.onClick = [this] { patternEngine.pastePattern(activeModuleIndex); };
        clearBtn.onClick = [this] { patternEngine.clearPattern(activeModuleIndex); };

        // ── Save button ──────────────────────────────────────
        saveBtn.setButtonText("SAVE");
        saveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1B5E20));
        saveBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF66BB6A));
        saveBtn.onClick = [this]() { saveCurrentBank(); };
        addAndMakeVisible(saveBtn);

        // ── Chain toggle ─────────────────────────────────────
        chainBtn.setButtonText("CHAIN");
        chainBtn.setClickingTogglesState(true);
        chainBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        chainBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Colours::accentOrange).withAlpha(0.5f));
        chainBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        chainBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Colours::textPrimary));
        chainBtn.onClick = [this] { sceneManager.setChainMode(chainBtn.getToggleState()); };
        addAndMakeVisible(chainBtn);

        bankLabel.setText("SCENES", juce::dontSendNotification);
        bankLabel.setFont(juce::Font(9.0f, juce::Font::bold));
        bankLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textLabel));
        bankLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(bankLabel);

        updatePageVisibility();
    }

    /** Set the active module index for copy/paste/clear operations */
    void setActiveModule(int idx) { activeModuleIndex = std::clamp(idx, 0, kNumModules - 1); }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xFF0f0f24));
        g.fillRect(getLocalBounds().toFloat());
        g.setColour(juce::Colour(Colours::borderSubtle));
        g.fillRect(0.0f, static_cast<float>(getHeight()) - 1.0f, static_cast<float>(getWidth()), 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8, 2);
        bankLabel.setBounds(area.removeFromLeft(52));
        area.removeFromLeft(4);

        // Page arrows
        prevPageBtn.setBounds(area.removeFromLeft(20));
        area.removeFromLeft(2);

        // Scene buttons — show kScenesPerPage at a time
        for (int i = 0; i < kTotalScenes; ++i)
        {
            if (sceneButtons[i]->isVisible())
            {
                sceneButtons[i]->setBounds(area.removeFromLeft(28));
                area.removeFromLeft(2);
            }
        }

        nextPageBtn.setBounds(area.removeFromLeft(20));
        area.removeFromLeft(8);

        // Right side: chain, save, pattern tools
        chainBtn.setBounds(area.removeFromRight(48));
        area.removeFromRight(4);
        saveBtn.setBounds(area.removeFromRight(44));
        area.removeFromRight(8);
        clearBtn.setBounds(area.removeFromRight(34));
        area.removeFromRight(2);
        pasteBtn.setBounds(area.removeFromRight(34));
        area.removeFromRight(2);
        copyBtn.setBounds(area.removeFromRight(34));
    }

private:
    static constexpr int kTotalScenes = 16;
    static constexpr int kScenesPerPage = 8;

    SceneManager& sceneManager;
    PatternEngine& patternEngine;
    MasterMixer& mixer;

    juce::OwnedArray<juce::TextButton> sceneButtons;
    juce::TextButton prevPageBtn, nextPageBtn;
    juce::TextButton copyBtn, pasteBtn, clearBtn;
    juce::TextButton saveBtn;
    juce::TextButton chainBtn;
    juce::Label bankLabel;

    int activeBank = 0;
    int activeModuleIndex = 0;
    int page = 0;  // 0 = A–H, 1 = I–P

    void setPage(int newPage)
    {
        page = std::clamp(newPage, 0, (kTotalScenes - 1) / kScenesPerPage);
        updatePageVisibility();
        resized();
    }

    void updatePageVisibility()
    {
        int start = page * kScenesPerPage;
        for (int i = 0; i < kTotalScenes; ++i)
            sceneButtons[i]->setVisible(i >= start && i < start + kScenesPerPage);
        prevPageBtn.setEnabled(page > 0);
        nextPageBtn.setEnabled(page < (kTotalScenes - 1) / kScenesPerPage);
    }

    MixerSnapshot getMixerSnapshot()
    {
        MixerSnapshot snap;
        for (int i = 0; i < kNumModules; ++i)
        {
            snap.strips[static_cast<size_t>(i)].level = mixer.getLevel(i);
            snap.strips[static_cast<size_t>(i)].pan   = mixer.getPan(i);
            snap.strips[static_cast<size_t>(i)].mute  = mixer.getMute(i);
            snap.strips[static_cast<size_t>(i)].solo  = mixer.getSolo(i);
        }
        return snap;
    }

    void applyMixerSnapshot(const MixerSnapshot& snap)
    {
        for (int i = 0; i < kNumModules; ++i)
        {
            mixer.setLevel(i, snap.strips[static_cast<size_t>(i)].level);
            mixer.setPan(i, snap.strips[static_cast<size_t>(i)].pan);
            mixer.setMute(i, snap.strips[static_cast<size_t>(i)].mute);
            mixer.setSolo(i, snap.strips[static_cast<size_t>(i)].solo);
        }
    }

    void saveCurrentBank()
    {
        auto snap = getMixerSnapshot();
        sceneManager.saveToScene(activeBank, patternEngine, snap);
        // Sync per-module scenes to this bank slot
        for (int m = 0; m < kNumModules; ++m)
            sceneManager.setModuleScene(m, activeBank);
    }

    void loadBank(int bankIndex)
    {
        activeBank = bankIndex;
        auto snap = getMixerSnapshot();
        sceneManager.loadScene(bankIndex, patternEngine, snap);
        applyMixerSnapshot(snap);
    }
};

} // namespace axelf::ui
