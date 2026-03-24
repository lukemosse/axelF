#include "PresetManager.h"

namespace axelf
{

void PresetManager::setModuleTrees(std::initializer_list<ModuleAPVTS> modules)
{
    moduleTrees.assign(modules.begin(), modules.end());
}

void PresetManager::loadPreset(const juce::File& file)
{
    auto text = file.loadFileAsString();
    if (text.isEmpty())
        return;

    auto root = juce::parseXML(text);
    if (root == nullptr || !root->hasTagName("AxelFPreset"))
        return;

    for (auto& mod : moduleTrees)
    {
        if (auto* wrapper = root->getChildByName(mod.name))
        {
            if (auto* child = wrapper->getFirstChildElement())
                mod.apvts->replaceState(juce::ValueTree::fromXml(*child));
        }
    }

    currentPresetName = file.getFileNameWithoutExtension();
}

void PresetManager::savePreset(const juce::File& file)
{
    auto root = std::make_unique<juce::XmlElement>("AxelFPreset");
    root->setAttribute("name", file.getFileNameWithoutExtension());

    for (auto& mod : moduleTrees)
    {
        if (auto xml = mod.apvts->copyState().createXml())
        {
            auto* wrapper = root->createNewChildElement(mod.name);
            wrapper->addChildElement(xml.release());
        }
    }

    file.replaceWithText(root->toString());
    currentPresetName = file.getFileNameWithoutExtension();
}

void PresetManager::loadFactoryPresets()
{
    presetNames.clear();
    presetFiles.clear();

    // Look for factory presets alongside the executable
    auto presetsDir = juce::File::getSpecialLocation(
                          juce::File::currentApplicationFile)
                          .getParentDirectory()
                          .getChildFile("Presets");

    if (!presetsDir.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator(presetsDir, false, "*.axpreset"))
    {
        presetFiles.push_back(entry.getFile());
        presetNames.add(entry.getFile().getFileNameWithoutExtension());
    }
}

} // namespace axelf
