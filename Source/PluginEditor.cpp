#include "PluginEditor.h"
#include "UI/VoicePresetBar.h"
#include "Modules/Jupiter8/Jupiter8Editor.h"
#include "Modules/Moog15/Moog15Editor.h"
#include "Modules/JX3P/JX3PEditor.h"
#include "Modules/DX7/DX7Editor.h"
#include "Modules/PPGWave/PPGWaveEditor.h"
#include "Modules/LinnDrum/LinnDrumEditor.h"

namespace axelf
{

AxelFEditor::AxelFEditor(AxelFProcessor& processor)
    : AudioProcessorEditor(processor),
      processorRef(processor),
      tabs(processor),
      keyboard(processorRef.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
      renderDialog(processor)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(topBar);

    // ── Wire transport controls ──────────────────────────────
    topBar.getPlayButton().onClick = [this]()
    {
        auto& t = processorRef.getTransport();
        if (t.isPlaying())
            t.stop();
        else
            t.play();
    };

    topBar.getStopButton().onClick = [this]()
    {
        processorRef.getTransport().stop();
    };

    topBar.getRecordButton().onClick = [this]() { toggleRecord(); };

    // ── Wire rewind / fast-forward ───────────────────────
    topBar.getRewindButton().onClick = [this]()
    {
        auto& t = processorRef.getTransport();
        int bpb = t.getTimeSignatureNumerator();
        double pos = t.getPositionInBeats();
        double newPos = std::max(0.0, pos - static_cast<double>(bpb));
        if (t.isPlaying())
            t.setPendingCue(newPos);
        else
            t.seekToPosition(newPos);
    };

    topBar.getFfwdButton().onClick = [this]()
    {
        auto& t = processorRef.getTransport();
        int bpb = t.getTimeSignatureNumerator();
        double pos = t.getPositionInBeats();
        double newPos = pos + static_cast<double>(bpb);
        if (t.isPlaying())
            t.setPendingCue(newPos);
        else
            t.seekToPosition(newPos);
    };

    topBar.getBpmLabel().onTextChange = [this]()
    {
        float bpm = topBar.getBpmLabel().getText().getFloatValue();
        if (bpm >= 20.0f && bpm <= 300.0f)
            processorRef.getTransport().setBpm(bpm);
    };

    topBar.getBarCountBox().onChange = [this]()
    {
        int bars = topBar.getBarCountBox().getSelectedId();
        if (bars > 0)
            processorRef.getTransport().setBarCount(bars);
    };

    topBar.getCountInButton().onClick = [this]()
    {
        bool on = topBar.getCountInButton().getToggleState();
        processorRef.getTransport().setCountInEnabled(on);
    };

    topBar.getClickButton().onClick = [this]()
    {
        bool on = topBar.getClickButton().getToggleState();
        processorRef.getMetronome().setEnabled(on);
    };

    // ── Wire transport display callbacks ─────────────────────
    topBar.onGetCpuUsage = [this]() { return processorRef.getCpuUsage(); };
    topBar.onGetBpm = [this]() { return processorRef.getTransport().getBpm(); };
    topBar.onGetCurrentBar = [this]() { return processorRef.getTransport().getCurrentBar(); };
    topBar.onGetBeatInBar = [this]() { return processorRef.getTransport().getBeatInBar(); };
    topBar.onGetBarCount = [this]() { return processorRef.getTransport().getBarCount(); };
    topBar.onGetIsPlaying = [this]() { return processorRef.getTransport().isPlaying(); };
    topBar.onGetIsRecording = [this]() { return processorRef.getTransport().isRecording(); };
    topBar.onGetIsRecordArmed = [this]() { return processorRef.getPatternEngine().isRecordArmed(processorRef.getActiveModule()); };

    // ── Module tabs ──────────────────────────────────────────
    auto tabBg = juce::Colour(0xFF16162a);
    auto& eng = processorRef.getPatternEngine();
    auto& trp = processorRef.getTransport();

    tabs.addTab("Jupiter-8",  tabBg, new jupiter8::Jupiter8Editor(processorRef.getJupiter8().getAPVTS(), &eng, &trp), true);
    tabs.addTab("Moog 15",    tabBg, new moog15::Moog15Editor(processorRef.getMoog15().getAPVTS(), &eng, &trp), true);
    tabs.addTab("JX-3P",      tabBg, new jx3p::JX3PEditor(processorRef.getJX3P().getAPVTS(), &eng, &trp), true);
    tabs.addTab("DX7",        tabBg, new dx7::DX7Editor(processorRef.getDX7().getAPVTS(), &eng, &trp), true);
    tabs.addTab("PPG Wave",   tabBg, new ppgwave::PPGWaveEditor(processorRef.getPPGWave().getAPVTS(), &eng, &trp), true);
    tabs.addTab("LinnDrum",   tabBg, new linndrum::LinnDrumEditor(processorRef.getLinnDrum().getAPVTS(), &eng, &trp), true);

    tabs.setTabBarDepth(72);
    tabs.onTabChanged = [this](int newIndex)
    {
        if (timelinePanel)
            timelinePanel->setActiveModule(newIndex);
        if (mixerPanel)
            mixerPanel->setActiveModule(newIndex);

        // When recording, move arm to the newly selected module
        auto& pe = processorRef.getPatternEngine();
        if (processorRef.getTransport().isRecording())
        {
            for (int i = 0; i < kNumModules; ++i)
                pe.setRecordArm(i, i == newIndex);
        }
    };
    addAndMakeVisible(tabs);

    // ── Mixer side panel (right side, ~1/4 width) ──────────────
    mixerPanel = std::make_unique<ui::MixerSidePanel>(
        processorRef.getMixer(), processorRef.getMixerAPVTS(), processorRef.getEffectsAPVTS(),
        &processorRef.getInsertSlot(0), &processorRef.getInsertSlot(1));
    addAndMakeVisible(*mixerPanel);

    // ── Timeline panel (replaces PatternBankBar + ArrangementView) ──
    timelinePanel = std::make_unique<ui::TimelinePanel>(
        processorRef.getSceneManager(), processorRef.getPatternEngine(),
        processorRef.getTransport(), processorRef.getArrangement(),
        processorRef.getMixer());
    timelinePanel->onModuleSelected = [this](int moduleIdx)
    {
        tabs.setCurrentTabIndex(moduleIdx);
    };
    addAndMakeVisible(*timelinePanel);

    // ── Virtual keyboard (bottom strip) ──────────────────────
    keyboard.setAvailableRange(36, 96);  // C2–C7
    keyboard.setKeyWidth(18.0f);
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xFFdde0e4));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xFF1a1a2e));
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xFF33334d));
    addAndMakeVisible(keyboard);

    // ── MIDI activity LED callbacks ──────────────────────────
    topBar.onGetMidiActivity = [this](int idx) -> bool
    {
        bool active = processorRef.hasMidiActivity(idx);
        if (active)
            processorRef.clearMidiActivity(idx);
        return active;
    };

    // ── Preset browser overlay (hidden until opened) ─────────
    presetBrowser.setVisible(false);
    addChildComponent(presetBrowser);

    // ── About overlay (hidden until logo click) ──────────────
    aboutOverlay.setVisible(false);
    addChildComponent(aboutOverlay);

    // ── Render dialog (hidden until FILE → Render) ───────────
    renderDialog.setVisible(false);
    addChildComponent(renderDialog);

    topBar.getLogoButton().onClick = [this]()
    {
        aboutOverlay.setBounds(getLocalBounds());
        aboutOverlay.setVisible(true);
        aboutOverlay.toFront(true);
    };

    presetBrowser.onPresetLoaded = [this](const juce::String& name)
    {
        topBar.getPresetLabel().setText(name, juce::dontSendNotification);
        // Also update the active module's VoicePresetBar name
        if (auto* editor = tabs.getCurrentContentComponent())
            if (auto* vpb = dynamic_cast<ui::VoicePresetBar*>(
                    editor->findChildWithID("VoicePresetBar")))
                vpb->setPresetName(name);
    };

    setSize(1500, 950);
    setResizable(true, true);
    setResizeLimits(1200, 700, 2400, 1400);
    setWantsKeyboardFocus(true);

    // ── Session file button ──────────────────────────────────
    topBar.getFileButton().onClick = [this]() { showSessionMenu(); };

    // Auto-load last opened session file on startup
    {
        auto lastFile = AxelFProcessor::getLastSessionFile();
        if (lastFile != juce::File())
        {
            processorRef.loadSessionFromFile(lastFile);
            sessionDirty = false;
        }
    }

    updateSessionLabel();

    // Auto-save every 60 seconds (standalone crash protection)
    startTimer(60000);

    // Add MIDI Learn right-click listener to all child sliders recursively
    std::function<void(juce::Component*)> addListeners = [&](juce::Component* comp)
    {
        if (auto* slider = dynamic_cast<juce::Slider*>(comp))
            slider->addMouseListener(&midiLearnListener, false);
        for (auto* child : comp->getChildren())
            addListeners(child);
    };
    addListeners(&tabs);
    // ── Standalone: listen for audio/MIDI device changes ─────
    // Saves device settings immediately when user changes MIDI inputs etc.
    // JUCE only saves at shutdown, which is lost if the app crashes.
#if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->deviceManager.addChangeListener (this);
#endif
    // ── Wire each module’s VoicePresetBar Load → PresetBrowser ──
    for (int i = 0; i < 6; ++i)
    {
        if (auto* editor = tabs.getTabContentComponent(i))
        {
            if (auto* vpb = dynamic_cast<ui::VoicePresetBar*>(
                    editor->findChildWithID("VoicePresetBar")))
            {
                vpb->onBrowseRequested = [this, i]
                {
                    juce::AudioProcessorValueTreeState* apvts = nullptr;
                    switch (i)
                    {
                        case 0: apvts = &processorRef.getJupiter8().getAPVTS(); break;
                        case 1: apvts = &processorRef.getMoog15().getAPVTS(); break;
                        case 2: apvts = &processorRef.getJX3P().getAPVTS(); break;
                        case 3: apvts = &processorRef.getDX7().getAPVTS(); break;
                        case 4: apvts = &processorRef.getPPGWave().getAPVTS(); break;
                        case 5: apvts = &processorRef.getLinnDrum().getAPVTS(); break;
                        default: return;
                    }
                    presetBrowser.open(i, *apvts);
                };
            }
        }
    }
}

AxelFEditor::~AxelFEditor()
{
#if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->deviceManager.removeChangeListener (this);
#endif
    stopTimer();
    setLookAndFeel(nullptr);
}

void AxelFEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ui::Colours::bgDark));
}

void AxelFEditor::resized()
{
    auto area = getLocalBounds();
    topBar.setBounds(area.removeFromTop(52));

    // Always-visible timeline panel at the top (replaces PatternBankBar + ArrangementView)
    if (timelinePanel != nullptr)
    {
        int tlH = timelinePanel->getDesiredHeight();
        timelinePanel->setBounds(area.removeFromTop(tlH));
    }

    // Virtual keyboard strip
    keyboard.setBounds(area.removeFromBottom(50));

    // Mixer panel on the right side (~1/4 width)
    if (mixerPanel != nullptr)
    {
        int mixW = area.getWidth() / 4;
        mixerPanel->setBounds(area.removeFromRight(mixW));
    }

    tabs.setBounds(area);

    // Preset browser overlays the entire editor
    presetBrowser.setBounds(getLocalBounds());
    aboutOverlay.setBounds(getLocalBounds());
    renderDialog.setBounds(getLocalBounds());
}

void AxelFEditor::toggleRecord()
{
    auto& t = processorRef.getTransport();
    auto& pe = processorRef.getPatternEngine();
    int mod = processorRef.getActiveModule();

    if (t.isRecording())
    {
        for (int i = 0; i < kNumModules; ++i)
            pe.setRecordArm(i, false);
        t.play();  // drop out of record but keep playing
    }
    else
    {
        pe.setRecordArm(mod, true);
        t.startRecording();
    }
}

void AxelFEditor::mouseDown(const juce::MouseEvent&)
{
}

bool AxelFEditor::keyPressed(const juce::KeyPress& key)
{
    const auto ctrl = juce::ModifierKeys::ctrlModifier;

    // Ctrl+Z / Ctrl+Y — undo/redo (arrangement in Song mode, pattern otherwise)
    if (key == juce::KeyPress('z', ctrl, 0))
    {
        if (processorRef.getArrangement().getMode() == axelf::ArrangementMode::Song)
            processorRef.getArrangement().undo();
        else
            processorRef.getPatternEngine().undo(processorRef.getActiveModule());
        repaint();
        return true;
    }

    // Ctrl+R — toggle record
    if (key == juce::KeyPress('r', ctrl, 0))
    {
        toggleRecord();
        return true;
    }

    // Ctrl+S — save session
    if (key == juce::KeyPress('s', ctrl, 0))
    {
        sessionSave();
        return true;
    }

    // Ctrl+Shift+S — save session as
    if (key == juce::KeyPress('s', ctrl | juce::ModifierKeys::shiftModifier, 0))
    {
        sessionSaveAs();
        return true;
    }

    // Ctrl+O — open session
    if (key == juce::KeyPress('o', ctrl, 0))
    {
        sessionOpen();
        return true;
    }

    // Ctrl+N — new session
    if (key == juce::KeyPress('n', ctrl, 0))
    {
        sessionNew();
        return true;
    }

    // Ctrl+E — render to file
    if (key == juce::KeyPress('e', ctrl, 0))
    {
        showRenderDialog();
        return true;
    }

    if (key == juce::KeyPress('y', ctrl, 0))
    {
        if (processorRef.getArrangement().getMode() == axelf::ArrangementMode::Song)
            processorRef.getArrangement().redo();
        else
            processorRef.getPatternEngine().redo(processorRef.getActiveModule());
        repaint();
        return true;
    }

    // Space — play/stop toggle
    if (key == juce::KeyPress(juce::KeyPress::spaceKey))
    {
        auto& t = processorRef.getTransport();
        if (t.isPlaying())
            t.stop();
        else
            t.play();
        return true;
    }

    // 1-6 — switch instrument tabs
    if (key.getKeyCode() >= '1' && key.getKeyCode() <= '6' && !key.getModifiers().isAnyModifierKeyDown())
    {
        int idx = key.getKeyCode() - '1';
        tabs.setCurrentTabIndex(idx);
        return true;
    }

    // Left / Right — navigate voice presets on the active module
    if (key == juce::KeyPress(juce::KeyPress::leftKey) && !key.getModifiers().isAnyModifierKeyDown())
    {
        if (auto* editor = tabs.getCurrentContentComponent())
            if (auto* bar = editor->findChildWithID("VoicePresetBar"))
                if (auto* vpb = dynamic_cast<ui::VoicePresetBar*>(bar))
                    vpb->navigatePreset(-1);
        return true;
    }
    if (key == juce::KeyPress(juce::KeyPress::rightKey) && !key.getModifiers().isAnyModifierKeyDown())
    {
        if (auto* editor = tabs.getCurrentContentComponent())
            if (auto* bar = editor->findChildWithID("VoicePresetBar"))
                if (auto* vpb = dynamic_cast<ui::VoicePresetBar*>(bar))
                    vpb->navigatePreset(1);
        return true;
    }

    // F1–F10: jump to marker / Ctrl+F1–F10: set marker at current position
    {
        int fkey = key.getKeyCode();
        if (fkey >= juce::KeyPress::F1Key && fkey <= juce::KeyPress::F10Key)
        {
            int slot = fkey - juce::KeyPress::F1Key;
            auto& arr = processorRef.getArrangement();
            auto& t = processorRef.getTransport();

            if (key.getModifiers().isCtrlDown())
            {
                // Set marker at current position
                int bpb = t.getTimeSignatureNumerator();
                int curBeat = static_cast<int>(t.getPositionInBeats());
                int barBeat = (curBeat / bpb) * bpb;  // snap to bar
                arr.setMarker(slot, barBeat);
                repaint();
            }
            else
            {
                // Jump to marker
                int beat = arr.getMarkerBeat(slot);
                if (beat >= 0)
                {
                    if (t.isPlaying())
                        t.setPendingCue(static_cast<double>(beat));
                    else
                        t.seekToPosition(static_cast<double>(beat));
                }
            }
            return true;
        }
    }

    return false;
}

void AxelFEditor::MidiLearnMouseListener::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isPopupMenu())
        return;

    // Walk up from the event component to find a Slider
    auto* comp = e.eventComponent;
    juce::Slider* slider = nullptr;
    while (comp != nullptr)
    {
        slider = dynamic_cast<juce::Slider*>(comp);
        if (slider != nullptr)
            break;
        comp = comp->getParentComponent();
    }
    if (slider == nullptr)
        return;

    // The slider name is usually set to the parameter ID by SliderAttachment
    auto paramID = slider->getName();
    if (paramID.isEmpty())
        return;

    owner.showMidiLearnMenu(slider, paramID);
}

void AxelFEditor::showMidiLearnMenu(juce::Component* target, const juce::String& paramID)
{
    auto& ml = processorRef.getMidiLearn();
    int existingCC = ml.getCCForParam(paramID);

    juce::PopupMenu menu;
    menu.addItem(1, "MIDI Learn: " + paramID);

    if (existingCC >= 0)
        menu.addItem(2, "Unmap CC " + juce::String(existingCC));

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
        [this, paramID, &ml](int result)
        {
            if (result == 1)
                ml.startLearning(paramID);
            else if (result == 2)
                ml.unmap(paramID);
        });
}

// ═══════════════════════════════════════════════════════════════
// Render dialog
// ═══════════════════════════════════════════════════════════════

void AxelFEditor::showRenderDialog()
{
    renderDialog.reset();
    renderDialog.setVisible(true);
    renderDialog.toFront(true);
}

// ═══════════════════════════════════════════════════════════════
// Session file management
// ═══════════════════════════════════════════════════════════════

void AxelFEditor::showSessionMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "New Session         Ctrl+N");
    menu.addItem(2, "Open Session...     Ctrl+O");
    menu.addSeparator();
    menu.addItem(3, "Save Session        Ctrl+S");
    menu.addItem(4, "Save Session As...  Ctrl+Shift+S");
    menu.addSeparator();
    menu.addItem(5, "Render to File...   Ctrl+E");

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&topBar.getFileButton()),
        [this](int result)
        {
            switch (result)
            {
                case 1: sessionNew();        break;
                case 2: sessionOpen();       break;
                case 3: sessionSave();       break;
                case 4: sessionSaveAs();     break;
                case 5: showRenderDialog();  break;
                default: break;
            }
        });
}

void AxelFEditor::sessionNew()
{
    auto doNew = [this]()
    {
        processorRef.newSession();
        sessionDirty = false;
        updateSessionLabel();
    };

    if (sessionDirty)
    {
        auto options = juce::MessageBoxOptions()
            .withTitle("New Session")
            .withMessage("Current session has unsaved changes. Discard them?")
            .withButton("Discard")
            .withButton("Cancel")
            .withAssociatedComponent(this);

        juce::AlertWindow::showAsync(options, [doNew](int result)
        {
            if (result == 1)
                doNew();
        });
    }
    else
    {
        doNew();
    }
}

void AxelFEditor::sessionOpen()
{
    auto doOpen = [this]()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Open Session",
            AxelFProcessor::getSessionsDirectory(),
            "*.axelf");

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode
                                     | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File())
                    return;

                if (processorRef.loadSessionFromFile(file))
                {
                    sessionDirty = false;
                    updateSessionLabel();
                }
                else
                {
                    juce::AlertWindow::showAsync(
                        juce::MessageBoxOptions()
                            .withTitle("Open Failed")
                            .withMessage("Could not load session file.")
                            .withButton("OK")
                            .withAssociatedComponent(this),
                        nullptr);
                }
            });
    };

    if (sessionDirty)
    {
        auto options = juce::MessageBoxOptions()
            .withTitle("Open Session")
            .withMessage("Current session has unsaved changes. Discard them?")
            .withButton("Discard")
            .withButton("Cancel")
            .withAssociatedComponent(this);

        juce::AlertWindow::showAsync(options, [doOpen](int result)
        {
            if (result == 1)
                doOpen();
        });
    }
    else
    {
        doOpen();
    }
}

void AxelFEditor::sessionSave()
{
    auto current = processorRef.getCurrentSessionFile();

    if (current != juce::File())
    {
        processorRef.saveSessionToFile(current);
        sessionDirty = false;
        updateSessionLabel();
    }
    else
    {
        sessionSaveAs();
    }
}

void AxelFEditor::sessionSaveAs()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Session As",
        AxelFProcessor::getSessionsDirectory(),
        "*.axelf");

    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                 | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return;

            // Ensure .axelf extension
            if (!file.hasFileExtension("axelf"))
                file = file.withFileExtension("axelf");

            if (processorRef.saveSessionToFile(file))
            {
                sessionDirty = false;
                updateSessionLabel();
            }
        });
}

void AxelFEditor::updateSessionLabel()
{
    auto current = processorRef.getCurrentSessionFile();
    juce::String name = (current != juce::File())
                            ? current.getFileNameWithoutExtension()
                            : "Untitled";

    if (sessionDirty)
        name += " *";

    topBar.getSessionLabel().setText(name, juce::dontSendNotification);
}

void AxelFEditor::autoSaveSession()
{
    auto autoFile = AxelFProcessor::getAutoSaveFile();

    // Write state to auto-save file without changing currentSessionFile
    juce::MemoryBlock data;
    processorRef.getStateInformation(data);
    autoFile.replaceWithData(data.getData(), data.getSize());
}

void AxelFEditor::timerCallback()
{
    autoSaveSession();
}

void AxelFEditor::changeListenerCallback (juce::ChangeBroadcaster* /*source*/)
{
#if JucePlugin_Build_Standalone
    // Persist audio/MIDI device settings immediately on any change
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->saveAudioDeviceState();
#endif
}

} // namespace axelf


