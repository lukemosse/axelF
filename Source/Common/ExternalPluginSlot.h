#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace axelf
{

// ── ExternalPluginSlot ───────────────────────────────────────
// Hosts a single external VST3 plugin instance.  Two of these
// sit in the master insert chain (post-aux-return, pre-master-bus).
// Thread-safety: load/unload happen on the message thread;
// process() is called from the audio thread.
class ExternalPluginSlot
{
public:
    ExternalPluginSlot();
    ~ExternalPluginSlot();

    // Call once from prepareToPlay
    void prepare (double sampleRate, int maxBlockSize);

    // Audio-thread: process buffer in-place (stereo)
    void process (juce::AudioBuffer<float>& buffer);

    // Message-thread: load a VST3 from file (returns true on success)
    bool loadPlugin (const juce::File& vst3File);

    // Message-thread: unload current plugin
    void unloadPlugin();

    // True if a plugin is loaded and ready
    bool isLoaded() const;

    // Bypass toggle (audio-thread safe)
    void setBypassed (bool shouldBypass) { bypassed.store (shouldBypass); }
    bool isBypassed() const { return bypassed.load(); }

    // Plugin name for display
    juce::String getPluginName() const;

    // Open/close the plugin's own editor window (message-thread)
    void showEditor();
    void closeEditor();
    bool isEditorOpen() const;

    // Latency reported by this plugin
    int getLatencySamples() const;

    // Serialise: save path + plugin state into XML
    std::unique_ptr<juce::XmlElement> toXml (const juce::String& tag) const;

    // Deserialise: restore from XML
    void fromXml (const juce::XmlElement& xml);

private:
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::File pluginFile;

    std::atomic<bool> bypassed { false };
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    bool prepared = false;

    // Editor window (message-thread only)
    class PluginWindow;
    std::unique_ptr<PluginWindow> editorWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExternalPluginSlot)
};

} // namespace axelf
