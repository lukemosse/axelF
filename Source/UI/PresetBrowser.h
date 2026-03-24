#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "AxelFColours.h"

namespace axelf::ui
{

/**
 * Modal overlay preset browser.
 * Shows Factory / User presets for the active module,
 * with category filtering, search, and save/delete actions.
 */
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::TextEditor::Listener
{
public:
    // Module directory names matching FactoryPresets.h layout
    static constexpr const char* kModuleDirs[] = {
        "Jupiter8", "Moog15", "JX3P", "DX7", "LinnDrum"
    };
    static constexpr const char* kModuleLabels[] = {
        "Jupiter-8", "Moog 15", "JX-3P", "DX7", "LinnDrum"
    };

    // Category tags (derived from preset naming conventions)
    enum Category { All = 0, Bass, Lead, Pad, Keys, Perc, FX, NumCategories };
    static constexpr const char* kCategoryNames[] = {
        "All", "Bass", "Lead", "Pad", "Keys", "Perc", "FX"
    };

    // ── Constructor ──────────────────────────────────────────
    PresetBrowser()
    {
        setOpaque(false);
        setAlwaysOnTop(true);

        // Title
        titleLabel_.setText("PRESET BROWSER", juce::dontSendNotification);
        titleLabel_.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        titleLabel_.setColour(juce::Label::textColourId, juce::Colour(Colours::accentGold));
        titleLabel_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel_);

        // Module label (dynamically updated)
        moduleLabel_.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        moduleLabel_.setColour(juce::Label::textColourId, juce::Colour(Colours::textSecondary));
        moduleLabel_.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(moduleLabel_);

        // Close button
        closeBtn_.setButtonText("X");
        closeBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::accentRed).withAlpha(0.6f));
        closeBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
        closeBtn_.onClick = [this] { setVisible(false); };
        addAndMakeVisible(closeBtn_);

        // Bank buttons
        auto makeToggle = [&](juce::TextButton& btn, const juce::String& text, bool on)
        {
            btn.setButtonText(text);
            btn.setClickingTogglesState(true);
            btn.setToggleState(on, juce::dontSendNotification);
            btn.setRadioGroupId(100);
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
            btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Colours::accentGold).withAlpha(0.5f));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
            btn.setColour(juce::TextButton::textColourOnId, juce::Colour(Colours::textPrimary));
            btn.onClick = [this] { refreshList(); };
            addAndMakeVisible(btn);
        };
        makeToggle(factoryBtn_, "Factory", true);
        makeToggle(userBtn_, "User", false);

        // Category buttons
        for (int i = 0; i < NumCategories; ++i)
        {
            auto* btn = categoryBtns_.add(new juce::TextButton(kCategoryNames[i]));
            btn->setClickingTogglesState(true);
            btn->setToggleState(i == 0, juce::dontSendNotification);
            btn->setRadioGroupId(200);
            btn->setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
            btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(Colours::accentBlue).withAlpha(0.4f));
            btn->setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
            btn->setColour(juce::TextButton::textColourOnId, juce::Colour(Colours::textPrimary));
            btn->onClick = [this] { refreshList(); };
            addAndMakeVisible(btn);
        }

        // Search box
        searchBox_.setTextToShowWhenEmpty("Search...", juce::Colour(Colours::textSecondary));
        searchBox_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(Colours::bgControl));
        searchBox_.setColour(juce::TextEditor::textColourId, juce::Colour(Colours::textPrimary));
        searchBox_.setColour(juce::TextEditor::outlineColourId, juce::Colour(Colours::borderSubtle));
        searchBox_.addListener(this);
        addAndMakeVisible(searchBox_);

        // Preset list
        listBox_.setModel(this);
        listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(Colours::bgDark));
        listBox_.setColour(juce::ListBox::outlineColourId, juce::Colour(Colours::borderSubtle));
        listBox_.setRowHeight(26);
        addAndMakeVisible(listBox_);

        // Action buttons
        auto makeAction = [&](juce::TextButton& btn, const juce::String& text, juce::uint32 col)
        {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, juce::Colour(col).withAlpha(0.5f));
            btn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
            addAndMakeVisible(btn);
        };
        makeAction(saveBtn_, "Save", Colours::accentGreen);
        makeAction(saveAsBtn_, "Save As", Colours::accentBlue);
        makeAction(deleteBtn_, "Delete", Colours::accentRed);

        saveBtn_.onClick   = [this] { savePreset(); };
        saveAsBtn_.onClick = [this] { savePresetAs(); };
        deleteBtn_.onClick = [this] { deletePreset(); };
    }

    // ── Public API ───────────────────────────────────────────
    void open(int moduleIndex, juce::AudioProcessorValueTreeState& apvts)
    {
        moduleIndex_ = juce::jlimit(0, 4, moduleIndex);
        apvts_ = &apvts;
        moduleLabel_.setText(juce::String("Module: ") + kModuleLabels[moduleIndex_],
                             juce::dontSendNotification);

        // Resolve preset directories
        auto root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("AxelF").getChildFile("Voices")
                        .getChildFile(kModuleDirs[moduleIndex_]);
        presetDir_ = root;

        refreshList();
        setVisible(true);
        toFront(true);
    }

    /** Callback fired after a preset is loaded. Set by the editor. */
    std::function<void(const juce::String& presetName)> onPresetLoaded;

    // ── Component overrides ──────────────────────────────────
    void paint(juce::Graphics& g) override
    {
        // Semi-transparent backdrop
        g.fillAll(juce::Colour(0xB0000000));

        // Browser panel
        auto panel = getPanelBounds();
        g.setColour(juce::Colour(Colours::bgPanel));
        g.fillRoundedRectangle(panel.toFloat(), 8.f);
        g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.4f));
        g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 8.f, 1.5f);
    }

    void resized() override
    {
        auto panel = getPanelBounds();
        auto area  = panel.reduced(12);

        // Title row
        auto titleRow = area.removeFromTop(28);
        closeBtn_.setBounds(titleRow.removeFromRight(28));
        titleRow.removeFromRight(4);
        moduleLabel_.setBounds(titleRow.removeFromRight(140));
        titleLabel_.setBounds(titleRow);

        area.removeFromTop(6);

        // Sidebar (left 140px)
        auto sidebar = area.removeFromLeft(140);
        auto sideArea = sidebar;

        // Bank buttons
        factoryBtn_.setBounds(sideArea.removeFromTop(28));
        sideArea.removeFromTop(2);
        userBtn_.setBounds(sideArea.removeFromTop(28));
        sideArea.removeFromTop(10);

        // Category buttons
        for (auto* btn : categoryBtns_)
        {
            btn->setBounds(sideArea.removeFromTop(26));
            sideArea.removeFromTop(2);
        }

        area.removeFromLeft(8);

        // Bottom action buttons
        auto bottomRow = area.removeFromBottom(32);
        int actionW = 80;
        saveBtn_.setBounds(bottomRow.removeFromLeft(actionW));
        bottomRow.removeFromLeft(6);
        saveAsBtn_.setBounds(bottomRow.removeFromLeft(actionW));
        bottomRow.removeFromLeft(6);
        deleteBtn_.setBounds(bottomRow.removeFromLeft(actionW));

        area.removeFromBottom(6);

        // Search box
        searchBox_.setBounds(area.removeFromTop(26));
        area.removeFromTop(4);

        // Preset list
        listBox_.setBounds(area);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Click outside the panel → close
        if (!getPanelBounds().contains(e.getPosition()))
            setVisible(false);
    }

    void visibilityChanged() override
    {
        if (isVisible())
            searchBox_.grabKeyboardFocus();
    }

private:
    // ── ListBoxModel ─────────────────────────────────────────
    int getNumRows() override { return (int)filteredFiles_.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int)filteredFiles_.size()) return;

        if (selected)
        {
            g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.25f));
            g.fillRect(0, 0, w, h);
        }
        g.setColour(selected ? juce::Colour(Colours::textPrimary)
                             : juce::Colour(Colours::textLabel));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(filteredFiles_[row].getFileNameWithoutExtension(),
                   8, 0, w - 16, h, juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        loadPresetAtRow(row);
        setVisible(false);
    }

    void selectedRowsChanged(int row) override
    {
        loadPresetAtRow(row);
    }

    // ── TextEditor::Listener ─────────────────────────────────
    void textEditorTextChanged(juce::TextEditor&) override { refreshList(); }

    // ── Helpers ──────────────────────────────────────────────
    juce::Rectangle<int> getPanelBounds() const
    {
        auto b = getLocalBounds();
        int pw = juce::jmin(560, b.getWidth() - 40);
        int ph = juce::jmin(460, b.getHeight() - 40);
        return { (b.getWidth() - pw) / 2, (b.getHeight() - ph) / 2, pw, ph };
    }

    void refreshList()
    {
        allFiles_.clear();
        filteredFiles_.clear();

        if (!presetDir_.isDirectory())
        {
            listBox_.updateContent();
            return;
        }

        presetDir_.findChildFiles(allFiles_, juce::File::findFiles, false, "*.xml");

        // Sort alphabetically
        std::sort(allFiles_.begin(), allFiles_.end(),
                  [](const juce::File& a, const juce::File& b)
                  { return a.getFileNameWithoutExtension().compareIgnoreCase(
                               b.getFileNameWithoutExtension()) < 0; });

        // Apply bank filter (Factory = came with install, User = anything else)
        // Convention: factory presets are read-only or start with known names.
        // For simplicity, "User" bank shows all, "Factory" shows all too —
        // because all presets live in the same directory.
        // We differentiate by checking if the file is read-only (factory)
        // vs writable (user saved).
        bool showFactory = factoryBtn_.getToggleState();
        // Both banks show the same directory for now; the bank toggle
        // is wired so users see the full list either way. A future
        // improvement could tag factory presets with metadata.

        // Apply search filter
        auto searchText = searchBox_.getText().trim().toLowerCase();

        // Apply category filter
        Category cat = getSelectedCategory();

        for (auto& f : allFiles_)
        {
            auto name = f.getFileNameWithoutExtension().toLowerCase();

            // Search filter
            if (searchText.isNotEmpty() && !name.contains(searchText))
                continue;

            // Category filter (keyword-based heuristic)
            if (cat != All && !matchesCategory(name, cat))
                continue;

            filteredFiles_.add(f);
        }

        listBox_.updateContent();
        listBox_.repaint();
    }

    Category getSelectedCategory() const
    {
        for (int i = 0; i < categoryBtns_.size(); ++i)
            if (categoryBtns_[i]->getToggleState())
                return static_cast<Category>(i);
        return All;
    }

    static bool matchesCategory(const juce::String& name, Category cat)
    {
        switch (cat)
        {
            case Bass: return name.contains("bass") || name.contains("sub");
            case Lead: return name.contains("lead") || name.contains("sync") || name.contains("arp");
            case Pad:  return name.contains("pad") || name.contains("string") || name.contains("ambient") || name.contains("wash");
            case Keys: return name.contains("key") || name.contains("piano") || name.contains("organ")
                            || name.contains("clav") || name.contains("harp") || name.contains("bell")
                            || name.contains("marimba") || name.contains("vibes") || name.contains("kalimba");
            case Perc: return name.contains("drum") || name.contains("perc") || name.contains("kit")
                            || name.contains("conga") || name.contains("hat") || name.contains("snare")
                            || name.contains("kick") || name.contains("tom") || name.contains("clap");
            case FX:   return name.contains("fx") || name.contains("noise") || name.contains("sweep")
                            || name.contains("riser") || name.contains("drop");
            default:   return true;
        }
    }

    void loadPresetAtRow(int row)
    {
        if (apvts_ == nullptr) return;
        if (row < 0 || row >= (int)filteredFiles_.size()) return;

        auto& file = filteredFiles_[row];
        if (auto xml = juce::parseXML(file))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            if (state.isValid())
            {
                apvts_->replaceState(state);
                auto name = file.getFileNameWithoutExtension();
                if (onPresetLoaded)
                    onPresetLoaded(name);
            }
        }
    }

    void savePreset()
    {
        if (apvts_ == nullptr) return;
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Preset", presetDir_, "*.xml");
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File()) return;
                writeStateToFile(file.withFileExtension("xml"));
            });
    }

    void savePresetAs()
    {
        savePreset(); // same behavior — JUCE FileChooser always prompts for name
    }

    void deletePreset()
    {
        int row = listBox_.getSelectedRow();
        if (row < 0 || row >= (int)filteredFiles_.size()) return;

        auto file = filteredFiles_[row];
        auto name = file.getFileNameWithoutExtension();

        // Use async alert for delete confirmation
        auto options = juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Delete Preset")
            .withMessage("Delete \"" + name + "\"?")
            .withButton("Delete")
            .withButton("Cancel");
        juce::AlertWindow::showAsync(options, [this, file](int result)
        {
            if (result == 1)
            {
                file.deleteFile();
                refreshList();
            }
        });
    }

    void writeStateToFile(const juce::File& file)
    {
        if (apvts_ == nullptr) return;
        auto state = apvts_->copyState();
        if (auto xml = state.createXml())
        {
            xml->writeTo(file);
            if (onPresetLoaded)
                onPresetLoaded(file.getFileNameWithoutExtension());
            refreshList();
        }
    }

    // ── Members ──────────────────────────────────────────────
    int moduleIndex_ = 0;
    juce::AudioProcessorValueTreeState* apvts_ = nullptr;
    juce::File presetDir_;

    juce::Label titleLabel_, moduleLabel_;
    juce::TextButton closeBtn_;
    juce::TextButton factoryBtn_, userBtn_;
    juce::OwnedArray<juce::TextButton> categoryBtns_;
    juce::TextEditor searchBox_;
    juce::ListBox listBox_;
    juce::TextButton saveBtn_, saveAsBtn_, deleteBtn_;

    juce::Array<juce::File> allFiles_;
    juce::Array<juce::File> filteredFiles_;
};

} // namespace axelf::ui
