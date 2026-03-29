#include "ExternalPluginSlot.h"
#include <juce_audio_processors/format/juce_AudioPluginFormatManagerHelpers.h>

namespace axelf
{

// ── Floating editor window ───────────────────────────────────
class ExternalPluginSlot::PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (juce::AudioProcessorEditor* ed, const juce::String& name)
        : juce::DocumentWindow (name, juce::Colours::black, juce::DocumentWindow::closeButton)
    {
        setContentOwned (ed, true);
        setResizable (true, false);
        centreWithSize (ed->getWidth(), ed->getHeight());
        setVisible (true);
        setAlwaysOnTop (true);
    }

    void closeButtonPressed() override { setVisible (false); }
};

// ── Construction / destruction ───────────────────────────────

ExternalPluginSlot::ExternalPluginSlot()
{
    juce::addDefaultFormatsToManager (formatManager);   // registers VST3 (+ AU on macOS)
}

ExternalPluginSlot::~ExternalPluginSlot()
{
    closeEditor();
    unloadPlugin();
}

// ── Prepare ──────────────────────────────────────────────────

void ExternalPluginSlot::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = maxBlockSize;
    prepared = true;

    if (instance != nullptr)
    {
        instance->setPlayConfigDetails (2, 2, sampleRate, maxBlockSize);
        instance->prepareToPlay (sampleRate, maxBlockSize);
    }
}

// ── Audio-thread process ─────────────────────────────────────

void ExternalPluginSlot::process (juce::AudioBuffer<float>& buffer)
{
    if (instance == nullptr || bypassed.load())
        return;

    juce::MidiBuffer emptyMidi;
    instance->processBlock (buffer, emptyMidi);
}

// ── Load / unload (message thread) ──────────────────────────

bool ExternalPluginSlot::loadPlugin (const juce::File& vst3File)
{
    closeEditor();
    unloadPlugin();

    juce::OwnedArray<juce::PluginDescription> descriptions;
    juce::KnownPluginList scanner;

    for (auto* format : formatManager.getFormats())
    {
        scanner.scanAndAddFile (vst3File.getFullPathName(), true, descriptions, *format);
    }

    if (descriptions.isEmpty())
        return false;

    juce::String errorMsg;
    instance = formatManager.createPluginInstance (*descriptions[0],
                                                   currentSampleRate,
                                                   currentBlockSize,
                                                   errorMsg);

    if (instance == nullptr)
    {
        DBG ("ExternalPluginSlot: failed to load " + vst3File.getFileName() + " — " + errorMsg);
        return false;
    }

    pluginFile = vst3File;

    instance->setPlayConfigDetails (2, 2, currentSampleRate, currentBlockSize);

    if (prepared)
        instance->prepareToPlay (currentSampleRate, currentBlockSize);

    DBG ("ExternalPluginSlot: loaded " + instance->getName());
    return true;
}

void ExternalPluginSlot::unloadPlugin()
{
    closeEditor();

    if (instance != nullptr)
    {
        instance->releaseResources();
        instance.reset();
    }

    pluginFile = {};
}

// ── Queries ──────────────────────────────────────────────────

bool ExternalPluginSlot::isLoaded() const
{
    return instance != nullptr;
}

juce::String ExternalPluginSlot::getPluginName() const
{
    if (instance != nullptr)
        return instance->getName();
    return "Empty";
}

int ExternalPluginSlot::getLatencySamples() const
{
    if (instance != nullptr)
        return instance->getLatencySamples();
    return 0;
}

// ── Editor window ────────────────────────────────────────────

void ExternalPluginSlot::showEditor()
{
    if (instance == nullptr)
        return;

    if (editorWindow != nullptr && editorWindow->isVisible())
    {
        editorWindow->toFront (true);
        return;
    }

    if (auto* editor = instance->createEditor())
    {
        editorWindow = std::make_unique<PluginWindow> (editor, instance->getName());
    }
}

void ExternalPluginSlot::closeEditor()
{
    editorWindow.reset();
}

bool ExternalPluginSlot::isEditorOpen() const
{
    return editorWindow != nullptr && editorWindow->isVisible();
}

// ── Serialisation ────────────────────────────────────────────

std::unique_ptr<juce::XmlElement> ExternalPluginSlot::toXml (const juce::String& tag) const
{
    auto xml = std::make_unique<juce::XmlElement> (tag);
    xml->setAttribute ("bypassed", bypassed.load() ? 1 : 0);

    if (instance != nullptr && pluginFile.existsAsFile())
    {
        xml->setAttribute ("path", pluginFile.getFullPathName());
        xml->setAttribute ("pluginName", instance->getName());

        juce::MemoryBlock stateData;
        instance->getStateInformation (stateData);
        xml->setAttribute ("state", stateData.toBase64Encoding());
    }

    return xml;
}

void ExternalPluginSlot::fromXml (const juce::XmlElement& xml)
{
    bypassed.store (xml.getIntAttribute ("bypassed", 0) != 0);

    auto path = xml.getStringAttribute ("path");
    if (path.isNotEmpty())
    {
        juce::File file (path);
        if (file.existsAsFile() && loadPlugin (file))
        {
            auto stateStr = xml.getStringAttribute ("state");
            if (stateStr.isNotEmpty())
            {
                juce::MemoryBlock stateData;
                stateData.fromBase64Encoding (stateStr);
                instance->setStateInformation (stateData.getData(),
                                               static_cast<int> (stateData.getSize()));
            }
        }
    }
}

} // namespace axelf
