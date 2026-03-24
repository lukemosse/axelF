#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace axelf
{

class PresetManager
{
public:
    struct ModuleAPVTS
    {
        const char* name;
        juce::AudioProcessorValueTreeState* apvts;
    };

    void setModuleTrees(std::initializer_list<ModuleAPVTS> modules);

    void loadPreset(const juce::File& file);
    void savePreset(const juce::File& file);
    void loadFactoryPresets();

    int getCurrentPresetIndex() const { return currentPreset; }
    juce::String getCurrentPresetName() const { return currentPresetName; }

    const juce::StringArray& getPresetNames() const { return presetNames; }

private:
    std::vector<ModuleAPVTS> moduleTrees;
    int currentPreset = 0;
    juce::String currentPresetName { "Init" };
    juce::StringArray presetNames;
    std::vector<juce::File> presetFiles;
};

} // namespace axelf
