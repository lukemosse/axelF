#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UI/AxelFLookAndFeel.h"
#include "UI/TopBar.h"
#include "UI/TimelinePanel.h"
#include "UI/MixerSidePanel.h"
#include "UI/PresetBrowser.h"
#include "UI/AboutOverlay.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace axelf
{

class AxelFEditor : public juce::AudioProcessorEditor,
                    private juce::Timer,
                    private juce::ChangeListener
{
public:
    explicit AxelFEditor(AxelFProcessor& processor);
    ~AxelFEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void showMidiLearnMenu(juce::Component* target, const juce::String& paramID);

    /** Toggle record: arm active module + start/stop transport recording. */
    void toggleRecord();

    // Session file management
    void showSessionMenu();
    void sessionNew();
    void sessionOpen();
    void sessionSave();
    void sessionSaveAs();
    void updateSessionLabel();
    void autoSaveSession();

private:
    AxelFProcessor& processorRef;
    ui::AxelFLookAndFeel lookAndFeel;
    ui::TopBar topBar;

    struct ModuleTabs : juce::TabbedComponent
    {
        ModuleTabs(AxelFProcessor& p)
            : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop), proc(p) {}

        void currentTabChanged(int newIndex, const juce::String&) override
        {
            // Send all-notes-off to the previously active module
            int prev = proc.getActiveModule();
            if (prev != newIndex && prev >= 0 && prev < kNumModules)
                proc.queueAllNotesOff(prev);

            if (newIndex >= 0 && newIndex < 5)
                proc.setActiveModule(newIndex);

            if (onTabChanged)
                onTabChanged(newIndex);
        }

        std::function<void(int)> onTabChanged;
        AxelFProcessor& proc;
    };

    ModuleTabs tabs;
    std::unique_ptr<ui::MixerSidePanel> mixerPanel;
    ui::PresetBrowser presetBrowser;
    std::unique_ptr<ui::TimelinePanel> timelinePanel;

    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltipWindow { this, 400 };
    ui::AboutOverlay aboutOverlay;

    // MIDI Learn right-click listener for all child sliders
    struct MidiLearnMouseListener : public juce::MouseListener
    {
        AxelFEditor& owner;
        explicit MidiLearnMouseListener(AxelFEditor& o) : owner(o) {}
        void mouseDown(const juce::MouseEvent& e) override;
    };
    MidiLearnMouseListener midiLearnListener { *this };

    // Session state
    bool sessionDirty = false;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AxelFEditor)
};

} // namespace axelf
