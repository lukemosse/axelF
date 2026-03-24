#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf::ui
{

// Per-module voice preset bar — save / load / prev / next for a single APVTS.
// Stores presets as XML files in a per-module folder under the user data directory.
class VoicePresetBar : public juce::Component,
                       private juce::ValueTree::Listener,
                       private juce::Timer
{
public:
    VoicePresetBar(juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& moduleName,
                   juce::Colour accent)
        : apvts_(apvts), moduleName_(moduleName), accentColour_(accent)
    {
        setComponentID("VoicePresetBar");
        presetDir_ = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("AxelF").getChildFile("Voices").getChildFile(moduleName);
        presetDir_.createDirectory();

        nameLabel_.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        nameLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
        nameLabel_.setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xFF2a2a48));
        nameLabel_.setColour(juce::Label::outlineWhenEditingColourId, accentColour_);
        nameLabel_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        nameLabel_.setJustificationType(juce::Justification::centred);
        nameLabel_.setText("Init", juce::dontSendNotification);
        nameLabel_.setEditable(false, false, false);  // not editable — click opens browser
        nameLabel_.setInterceptsMouseClicks(true, false);
        nameLabel_.addMouseListener(this, false);
        addAndMakeVisible(nameLabel_);

        voiceLabel_.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        voiceLabel_.setColour(juce::Label::textColourId, accent);
        voiceLabel_.setJustificationType(juce::Justification::centred);
        voiceLabel_.setText("VOICE", juce::dontSendNotification);
        addAndMakeVisible(voiceLabel_);

        auto makeBtn = [&](juce::TextButton& btn, const juce::String& text)
        {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, accent.withAlpha(0.4f));
            btn.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(btn);
        };
        makeBtn(prevBtn_, "<");
        makeBtn(nextBtn_, ">");
        makeBtn(saveBtn_, "Save");

        prevBtn_.onClick = [this] { navigate(-1); };
        nextBtn_.onClick = [this] { navigate(1); };
        saveBtn_.onClick = [this] { saveCurrentPreset(); };

        scanPresets();
        loadDefaultPreset();
        snapshotCleanState();

        // Listen for parameter changes to track dirty state
        apvts_.state.addListener(this);
    }

    ~VoicePresetBar() override
    {
        apvts_.state.removeListener(this);
        stopTimer();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(2);
        voiceLabel_.setBounds(area.removeFromLeft(44));
        area.removeFromLeft(2);
        int btnW = 36;
        prevBtn_.setBounds(area.removeFromLeft(btnW));
        area.removeFromLeft(2);
        nextBtn_.setBounds(area.removeFromLeft(btnW));
        area.removeFromLeft(4);
        saveBtn_.setBounds(area.removeFromRight(50));
        area.removeFromRight(4);
        nameLabel_.setBounds(area);
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xFF1a1f2e));
        g.fillRoundedRectangle(b, 4.f);
        g.setColour(accentColour_.withAlpha(0.5f));
        g.drawRoundedRectangle(b.reduced(0.5f), 4.f, 1.5f);
    }

    /** Navigate presets from external keyboard shortcut. delta = -1 (prev) or +1 (next) */
    void navigatePreset(int delta) { navigate(delta); }

    /** Called when name label is clicked — host should open the preset browser. */
    std::function<void()> onBrowseRequested;

    /** Update the displayed preset name (e.g. after loading from PresetBrowser). */
    void setPresetName(const juce::String& name)
    {
        cleanPresetName_ = name;
        nameLabel_.setText(name, juce::dontSendNotification);
        dirty_ = false;
        snapshotCleanState();
    }

    /** Handle click on the name label to open browser */
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.eventComponent == &nameLabel_)
        {
            if (onBrowseRequested)
                onBrowseRequested();
        }
    }

private:
    void scanPresets()
    {
        presetFiles_.clear();
        presetNames_.clear();
        for (auto& f : presetDir_.findChildFiles(juce::File::findFiles, false, "*.xml"))
        {
            presetFiles_.push_back(f);
            presetNames_.add(f.getFileNameWithoutExtension());
        }
    }

    void loadDefaultPreset()
    {
        // Map each module to a cool default preset instead of "Init"
        juce::String target;
        if      (moduleName_ == "Jupiter8")  target = "Lush Strings";
        else if (moduleName_ == "Moog15")    target = "Creamy Lead";
        else if (moduleName_ == "JX3P")      target = "Chorus Pad";
        else if (moduleName_ == "DX7")       target = "E Piano";
        else if (moduleName_ == "LinnDrum")  target = "Standard Kit";
        else return;

        for (size_t i = 0; i < presetFiles_.size(); ++i)
        {
            if (presetFiles_[i].getFileNameWithoutExtension() == target)
            {
                currentIndex_ = (int)i;
                loadFromFile(presetFiles_[i]);
                return;
            }
        }
    }

    void navigate(int delta)
    {
        if (presetFiles_.empty()) return;
        suppressDirtyTracking_ = true;
        currentIndex_ = (currentIndex_ + delta + (int)presetFiles_.size()) % (int)presetFiles_.size();
        loadFromFile(presetFiles_[static_cast<size_t>(currentIndex_)]);
        suppressDirtyTracking_ = false;
        dirty_ = false;
        snapshotCleanState();
    }

    void loadFromFile(const juce::File& file)
    {
        if (auto xml = juce::parseXML(file))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            if (state.isValid())
            {
                suppressDirtyTracking_ = true;
                apvts_.replaceState(state);
                suppressDirtyTracking_ = false;
            }
            cleanPresetName_ = file.getFileNameWithoutExtension();
            nameLabel_.setText(cleanPresetName_, juce::dontSendNotification);
            dirty_ = false;
            snapshotCleanState();
        }
    }

    void saveCurrentPreset()
    {
        auto name = cleanPresetName_.trim();
        if (name.isEmpty() || name == "Init")
        {
            // Auto-generate a unique name
            int n = 1;
            while (presetDir_.getChildFile("Preset " + juce::String(n) + ".xml").existsAsFile())
                ++n;
            name = "Preset " + juce::String(n);
        }

        // If it's a factory preset, save as a new copy
        if (isFactoryPreset(name))
        {
            name = name + " (User)";
            int n = 2;
            while (presetDir_.getChildFile(name + ".xml").existsAsFile())
            {
                name = cleanPresetName_.trim() + " (User " + juce::String(n) + ")";
                ++n;
            }
        }

        auto f = presetDir_.getChildFile(name + ".xml");
        auto state = apvts_.copyState();
        if (auto xml = state.createXml())
            xml->writeTo(f);
        cleanPresetName_ = name;
        nameLabel_.setText(name, juce::dontSendNotification);
        dirty_ = false;
        snapshotCleanState();
        scanPresets();
        for (size_t i = 0; i < presetFiles_.size(); ++i)
            if (presetFiles_[i] == f) { currentIndex_ = (int)i; break; }
    }

    bool isFactoryPreset(const juce::String& name) const
    {
        return presetDir_.getChildFile(name + ".factory").existsAsFile();
    }

    // ValueTree::Listener — tracks dirty state on parameter changes
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override
    {
        if (!suppressDirtyTracking_)
            startTimer(200);  // debounce dirty check
    }

    void timerCallback() override
    {
        stopTimer();
        checkDirty();
    }

    void snapshotCleanState()
    {
        cleanStateXml_.reset();
        cleanStateXml_ = apvts_.copyState().createXml();
    }

    void checkDirty()
    {
        if (cleanStateXml_ == nullptr) { markDirty(true); return; }
        auto currentXml = apvts_.copyState().createXml();
        if (currentXml == nullptr) return;
        bool nowDirty = (currentXml->toString() != cleanStateXml_->toString());
        if (nowDirty != dirty_)
            markDirty(nowDirty);
    }

    void markDirty(bool isDirty)
    {
        dirty_ = isDirty;
        if (dirty_)
            nameLabel_.setText(cleanPresetName_ + " *", juce::dontSendNotification);
        else
            nameLabel_.setText(cleanPresetName_, juce::dontSendNotification);
    }

    juce::AudioProcessorValueTreeState& apvts_;
    juce::String moduleName_;
    juce::Colour accentColour_;
    juce::File presetDir_;

    juce::Label nameLabel_;
    juce::Label voiceLabel_;
    juce::TextButton prevBtn_, nextBtn_, saveBtn_;

    std::vector<juce::File> presetFiles_;
    juce::StringArray presetNames_;
    int currentIndex_ = 0;
    bool suppressDirtyTracking_ = false;
    bool dirty_ = false;
    juce::String cleanPresetName_ { "Init" };
    std::unique_ptr<juce::XmlElement> cleanStateXml_;
};

} // namespace axelf::ui
