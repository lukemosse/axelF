#include "SceneManager.h"
#include <algorithm>

namespace axelf
{

SceneManager::SceneManager()
{
    // Name scenes A through P
    for (int i = 0; i < kMaxScenes; ++i)
    {
        scenes[static_cast<size_t>(i)].name = juce::String("Scene ") +
            juce::String::charToString(static_cast<juce::juce_wchar>('A' + i));
    }
}

Scene& SceneManager::getScene(int index)
{
    return scenes[static_cast<size_t>(std::clamp(index, 0, kMaxScenes - 1))];
}

const Scene& SceneManager::getScene(int index) const
{
    return scenes[static_cast<size_t>(std::clamp(index, 0, kMaxScenes - 1))];
}

void SceneManager::saveToScene(int sceneIndex, const PatternEngine& engine,
                                const MixerSnapshot& mixer)
{
    if (sceneIndex < 0 || sceneIndex >= kMaxScenes)
        return;

    auto& scene = scenes[static_cast<size_t>(sceneIndex)];

    for (int i = 0; i < 4; ++i)
        scene.synthPatterns[static_cast<size_t>(i)] = engine.getSynthPattern(i);

    scene.drumPattern = engine.getDrumPattern();
    scene.mixerState = mixer;
}

void SceneManager::loadScene(int sceneIndex, PatternEngine& engine, MixerSnapshot& mixer)
{
    if (sceneIndex < 0 || sceneIndex >= kMaxScenes)
        return;

    const auto& scene = scenes[static_cast<size_t>(sceneIndex)];

    for (int i = 0; i < 4; ++i)
        engine.getSynthPattern(i) = scene.synthPatterns[static_cast<size_t>(i)];

    engine.getDrumPattern() = scene.drumPattern;
    mixer = scene.mixerState;
    activeScene = sceneIndex;
}

void SceneManager::switchScene(int sceneIndex, PatternEngine& engine, MixerSnapshot& mixer)
{
    loadScene(sceneIndex, engine, mixer);
}

void SceneManager::setChain(const std::vector<ChainEntry>& newChain)
{
    chain = newChain;
    chainPosition = 0;
    chainRepeat = 0;
}

bool SceneManager::advanceChain(PatternEngine& engine, MixerSnapshot& mixer)
{
    if (!chainMode || chain.empty())
        return false;

    auto& entry = chain[static_cast<size_t>(chainPosition)];
    chainRepeat++;

    if (chainRepeat >= entry.repeatCount)
    {
        // Move to next entry in chain
        chainRepeat = 0;
        chainPosition = (chainPosition + 1) % static_cast<int>(chain.size());

        auto& nextEntry = chain[static_cast<size_t>(chainPosition)];
        if (nextEntry.sceneIndex != activeScene)
        {
            loadScene(nextEntry.sceneIndex, engine, mixer);
            return true;
        }
    }

    return false;
}

void SceneManager::setSceneName(int index, const juce::String& name)
{
    if (index >= 0 && index < kMaxScenes)
        scenes[static_cast<size_t>(index)].name = name;
}

juce::String SceneManager::getSceneName(int index) const
{
    if (index >= 0 && index < kMaxScenes)
        return scenes[static_cast<size_t>(index)].name;
    return {};
}

void SceneManager::copyScene(int srcIndex, int destIndex)
{
    if (srcIndex >= 0 && srcIndex < kMaxScenes &&
        destIndex >= 0 && destIndex < kMaxScenes && srcIndex != destIndex)
    {
        scenes[static_cast<size_t>(destIndex)] = scenes[static_cast<size_t>(srcIndex)];
    }
}

void SceneManager::setModuleScene(int moduleIndex, int sceneIndex)
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules &&
        sceneIndex >= 0 && sceneIndex < kMaxScenes)
    {
        moduleScenes[static_cast<size_t>(moduleIndex)] = sceneIndex;
    }
}

int SceneManager::getModuleScene(int moduleIndex) const
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        return moduleScenes[static_cast<size_t>(moduleIndex)];
    return 0;
}

void SceneManager::queueModuleScene(int moduleIndex, int sceneIndex)
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules &&
        sceneIndex >= 0 && sceneIndex < kMaxScenes)
    {
        queuedScenes[static_cast<size_t>(moduleIndex)] = sceneIndex;
    }
}

int SceneManager::getQueuedModuleScene(int moduleIndex) const
{
    if (moduleIndex >= 0 && moduleIndex < kNumModules)
        return queuedScenes[static_cast<size_t>(moduleIndex)];
    return -1;
}

bool SceneManager::hasQueuedScenes() const
{
    for (int i = 0; i < kNumModules; ++i)
        if (queuedScenes[static_cast<size_t>(i)] >= 0)
            return true;
    return false;
}

void SceneManager::applyQueuedScenes(PatternEngine& engine)
{
    for (int i = 0; i < kNumModules; ++i)
    {
        auto idx = static_cast<size_t>(i);
        if (queuedScenes[idx] >= 0)
        {
            loadSceneForModule(queuedScenes[idx], i, engine);
            moduleScenes[idx] = queuedScenes[idx];
            queuedScenes[idx] = -1;
        }
    }

    // Sync activeScene if all modules are on the same scene
    int first = moduleScenes[0];
    bool allSame = true;
    for (int i = 1; i < kNumModules; ++i)
    {
        if (moduleScenes[static_cast<size_t>(i)] != first)
        {
            allSame = false;
            break;
        }
    }
    if (allSame)
        activeScene = first;
}

void SceneManager::loadSceneForModule(int sceneIndex, int moduleIndex, PatternEngine& engine)
{
    if (sceneIndex < 0 || sceneIndex >= kMaxScenes) return;
    if (moduleIndex < 0 || moduleIndex >= kNumModules) return;

    const auto& scene = scenes[static_cast<size_t>(sceneIndex)];

    if (moduleIndex == kLinnDrum)
    {
        engine.getDrumPattern() = scene.drumPattern;
    }
    else
    {
        engine.getSynthPattern(moduleIndex) = scene.synthPatterns[static_cast<size_t>(moduleIndex)];
    }
}

void SceneManager::saveModuleToScene(int sceneIndex, int moduleIndex, const PatternEngine& engine)
{
    if (sceneIndex < 0 || sceneIndex >= kMaxScenes) return;
    if (moduleIndex < 0 || moduleIndex >= kNumModules) return;

    auto& scene = scenes[static_cast<size_t>(sceneIndex)];

    if (moduleIndex == kLinnDrum)
        scene.drumPattern = engine.getDrumPattern();
    else
        scene.synthPatterns[static_cast<size_t>(moduleIndex)] = engine.getSynthPattern(moduleIndex);
}

std::unique_ptr<juce::XmlElement> SceneManager::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("SceneManager");
    xml->setAttribute("activeScene", activeScene);

    // Scenes
    for (int i = 0; i < kMaxScenes; ++i)
    {
        const auto& scene = scenes[static_cast<size_t>(i)];
        auto* sceneXml = xml->createNewChildElement("Scene");
        sceneXml->setAttribute("index", i);
        sceneXml->setAttribute("name", scene.name);

        // Synth patterns
        for (int m = 0; m < 4; ++m)
        {
            const auto& sp = scene.synthPatterns[static_cast<size_t>(m)];
            auto* spXml = sceneXml->createNewChildElement("SynthPattern");
            spXml->setAttribute("module", m);
            spXml->setAttribute("barCount", sp.getBarCount());
            spXml->setAttribute("beatsPerBar", sp.getBeatsPerBar());
            spXml->setAttribute("loopBars", sp.getLoopBars());

            for (size_t e = 0; e < sp.getNumEvents(); ++e)
            {
                const auto& evt = sp.getEvent(e);
                auto* evtXml = spXml->createNewChildElement("Event");
                evtXml->setAttribute("note", evt.noteNumber);
                evtXml->setAttribute("vel", static_cast<double>(evt.velocity));
                evtXml->setAttribute("start", evt.startBeat);
                evtXml->setAttribute("dur", evt.duration);
            }
        }

        // Drum pattern
        {
            const auto& dp = scene.drumPattern;
            auto* dpXml = sceneXml->createNewChildElement("DrumPattern");
            dpXml->setAttribute("barCount", dp.getBarCount());
            dpXml->setAttribute("loopBars", dp.getLoopBars());

            const int totalSteps = dp.getTotalSteps();
            for (int t = 0; t < DrumPattern::kMaxTracks; ++t)
            {
                auto* trackXml = dpXml->createNewChildElement("Track");
                trackXml->setAttribute("index", t);

                juce::String stepStr;
                juce::String velStr;
                for (int s = 0; s < totalSteps; ++s)
                {
                    stepStr += dp.getStep(t, s) ? "1" : "0";
                    if (s > 0) velStr += ",";
                    velStr += juce::String(dp.getVelocity(t, s), 3);
                }
                trackXml->setAttribute("steps", stepStr);
                trackXml->setAttribute("velocities", velStr);
            }
        }

        // Mixer snapshot
        {
            auto* mixXml = sceneXml->createNewChildElement("MixerSnapshot");
            for (int m = 0; m < kNumModules; ++m)
            {
                const auto& strip = scene.mixerState.strips[static_cast<size_t>(m)];
                auto* stripXml = mixXml->createNewChildElement("Strip");
                stripXml->setAttribute("index", m);
                stripXml->setAttribute("level", static_cast<double>(strip.level));
                stripXml->setAttribute("pan", static_cast<double>(strip.pan));
                stripXml->setAttribute("mute", strip.mute);
                stripXml->setAttribute("solo", strip.solo);
                stripXml->setAttribute("send1", static_cast<double>(strip.send1));
                stripXml->setAttribute("send2", static_cast<double>(strip.send2));
            }
        }
    }

    // Chain
    if (!chain.empty())
    {
        auto* chainXml = xml->createNewChildElement("Chain");
        for (const auto& entry : chain)
        {
            auto* entryXml = chainXml->createNewChildElement("Entry");
            entryXml->setAttribute("scene", entry.sceneIndex);
            entryXml->setAttribute("repeats", entry.repeatCount);
        }
    }

    // Per-module JAM state
    {
        auto* jamXml = xml->createNewChildElement("JamState");
        juce::String vals;
        for (int i = 0; i < kNumModules; ++i)
        {
            if (i > 0) vals += ",";
            vals += juce::String(moduleScenes[static_cast<size_t>(i)]);
        }
        jamXml->setAttribute("moduleScenes", vals);
    }

    return xml;
}

void SceneManager::fromXml(const juce::XmlElement* xml)
{
    if (xml == nullptr || !xml->hasTagName("SceneManager"))
        return;

    activeScene = xml->getIntAttribute("activeScene", 0);

    // Scenes
    for (auto* sceneXml : xml->getChildWithTagNameIterator("Scene"))
    {
        int i = sceneXml->getIntAttribute("index", -1);
        if (i < 0 || i >= kMaxScenes) continue;

        auto& scene = scenes[static_cast<size_t>(i)];
        scene.name = sceneXml->getStringAttribute("name", "Empty");

        // Synth patterns
        for (auto* spXml : sceneXml->getChildWithTagNameIterator("SynthPattern"))
        {
            int m = spXml->getIntAttribute("module", -1);
            if (m < 0 || m >= 4) continue;

            auto& sp = scene.synthPatterns[static_cast<size_t>(m)];
            sp.clear();
            sp.setBarCount(spXml->getIntAttribute("barCount", 4));
            sp.setBeatsPerBar(spXml->getIntAttribute("beatsPerBar", 4));
            sp.setLoopBars(spXml->getIntAttribute("loopBars", 4));

            for (auto* evtXml : spXml->getChildWithTagNameIterator("Event"))
            {
                MidiEvent evt;
                evt.noteNumber = evtXml->getIntAttribute("note", 60);
                evt.velocity = static_cast<float>(evtXml->getDoubleAttribute("vel", 1.0));
                evt.startBeat = evtXml->getDoubleAttribute("start", 0.0);
                evt.duration = evtXml->getDoubleAttribute("dur", 0.25);
                sp.addEvent(evt);
            }
        }

        // Drum pattern
        if (auto* dpXml = sceneXml->getChildByName("DrumPattern"))
        {
            scene.drumPattern.clear();
            scene.drumPattern.setBarCount(dpXml->getIntAttribute("barCount", 4));
            scene.drumPattern.setLoopBars(dpXml->getIntAttribute("loopBars", 4));

            for (auto* trackXml : dpXml->getChildWithTagNameIterator("Track"))
            {
                int t = trackXml->getIntAttribute("index", -1);
                if (t < 0 || t >= DrumPattern::kMaxTracks) continue;

                auto stepStr = trackXml->getStringAttribute("steps");
                auto velStr = trackXml->getStringAttribute("velocities");
                juce::StringArray velTokens;
                velTokens.addTokens(velStr, ",", "");

                for (int s = 0; s < stepStr.length() && s < scene.drumPattern.getTotalSteps(); ++s)
                {
                    bool active = stepStr[s] == '1';
                    float vel = (s < velTokens.size()) ? velTokens[s].getFloatValue() : 1.0f;
                    scene.drumPattern.setStep(t, s, active, vel);
                }
            }
        }

        // Mixer snapshot
        if (auto* mixXml = sceneXml->getChildByName("MixerSnapshot"))
        {
            for (auto* stripXml : mixXml->getChildWithTagNameIterator("Strip"))
            {
                int m = stripXml->getIntAttribute("index", -1);
                if (m < 0 || m >= kNumModules) continue;

                auto& strip = scene.mixerState.strips[static_cast<size_t>(m)];
                strip.level = static_cast<float>(stripXml->getDoubleAttribute("level", 1.0));
                strip.pan = static_cast<float>(stripXml->getDoubleAttribute("pan", 0.0));
                strip.mute = stripXml->getBoolAttribute("mute", false);
                strip.solo = stripXml->getBoolAttribute("solo", false);
                strip.send1 = static_cast<float>(stripXml->getDoubleAttribute("send1", 0.0));
                strip.send2 = static_cast<float>(stripXml->getDoubleAttribute("send2", 0.0));
            }
        }
    }

    // Chain
    chain.clear();
    if (auto* chainXml = xml->getChildByName("Chain"))
    {
        for (auto* entryXml : chainXml->getChildWithTagNameIterator("Entry"))
        {
            ChainEntry entry;
            entry.sceneIndex = entryXml->getIntAttribute("scene", 0);
            entry.repeatCount = entryXml->getIntAttribute("repeats", 1);
            chain.push_back(entry);
        }
    }

    // Per-module JAM state
    if (auto* jamXml = xml->getChildByName("JamState"))
    {
        auto vals = jamXml->getStringAttribute("moduleScenes");
        juce::StringArray tokens;
        tokens.addTokens(vals, ",", "");
        for (int i = 0; i < std::min(tokens.size(), static_cast<int>(kNumModules)); ++i)
            moduleScenes[static_cast<size_t>(i)] = std::clamp(tokens[i].getIntValue(), 0, kMaxScenes - 1);
    }
}

} // namespace axelf
