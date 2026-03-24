import sys

def patch_file():
    with open('C:/axelF/Source/Common/SnapshotManager.cpp', 'r', encoding='utf-8') as f:
        content = f.read()

    to_xml_idx = content.find('std::unique_ptr<juce::XmlElement> SnapshotManager::toXml() const')
    
    new_methods = """std::unique_ptr<juce::XmlElement> SnapshotManager::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("SNAPSHOTS");
    xml->setAttribute("ActiveSnapshot", activeSnapshotIndex);

    for (const auto& snap : snapshots)
    {
        auto* snapXml = xml->createNewChildElement("SNAPSHOT");
        snapXml->setAttribute("Name", snap.name);

        // Synth patterns
        for (int m = 0; m < 4; ++m)
        {
            const auto& sp = snap.synthPatterns[static_cast<size_t>(m)];
            auto* spXml = snapXml->createNewChildElement("SynthPattern");
            spXml->setAttribute("module", m);
            spXml->setAttribute("barCount", sp.getBarCount());
            spXml->setAttribute("beatsPerBar", sp.getBeatsPerBar());

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
            const auto& dp = snap.drumPattern;
            auto* dpXml = snapXml->createNewChildElement("DrumPattern");
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

        // Mixer
        auto* mixerXml = snapXml->createNewChildElement("MixerSnapshot");
        for (int i = 0; i < kNumModules; ++i)
        {
            auto* stripXml = mixerXml->createNewChildElement("Strip");
            stripXml->setAttribute("index", i);
            stripXml->setAttribute("level", static_cast<double>(snap.mixerState.strips[static_cast<size_t>(i)].level));
            stripXml->setAttribute("pan", static_cast<double>(snap.mixerState.strips[static_cast<size_t>(i)].pan));
            stripXml->setAttribute("mute", snap.mixerState.strips[static_cast<size_t>(i)].mute ? 1 : 0);
            stripXml->setAttribute("solo", snap.mixerState.strips[static_cast<size_t>(i)].solo ? 1 : 0);
            stripXml->setAttribute("send1", static_cast<double>(snap.mixerState.strips[static_cast<size_t>(i)].send1));
            stripXml->setAttribute("send2", static_cast<double>(snap.mixerState.strips[static_cast<size_t>(i)].send2));
        }
    }
    return xml;
}

void SnapshotManager::fromXml(const juce::XmlElement* xml)
{
    if (!xml || !xml->hasTagName("SNAPSHOTS"))
        return;

    activeSnapshotIndex = xml->getIntAttribute("ActiveSnapshot", 0);
    snapshots.clear();

    for (auto* snapXml : xml->getChildWithTagNameIterator("SNAPSHOT"))
    {
        Snapshot snap;
        snap.name = snapXml->getStringAttribute("Name", "Snapshot");

        for (auto* spXml : snapXml->getChildWithTagNameIterator("SynthPattern"))
        {
            int m = spXml->getIntAttribute("module", -1);
            if (m < 0 || m >= 4) continue;

            auto& sp = snap.synthPatterns[static_cast<size_t>(m)];
            sp.clear();
            sp.setBarCount(spXml->getIntAttribute("barCount", 4));
            sp.setBeatsPerBar(spXml->getIntAttribute("beatsPerBar", 4));

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

        if (auto* dpXml = snapXml->getChildByName("DrumPattern"))
        {
            snap.drumPattern.clear();
            snap.drumPattern.setBarCount(dpXml->getIntAttribute("barCount", 4));
            snap.drumPattern.setLoopBars(dpXml->getIntAttribute("loopBars", 4));

            for (auto* trackXml : dpXml->getChildWithTagNameIterator("Track"))
            {
                int t = trackXml->getIntAttribute("index", -1);
                if (t < 0 || t >= DrumPattern::kMaxTracks) continue;

                auto stepStr = trackXml->getStringAttribute("steps");
                auto velStr = trackXml->getStringAttribute("velocities");
                juce::StringArray velTokens;
                velTokens.addTokens(velStr, ",", "");

                for (int s = 0; s < stepStr.length() && s < snap.drumPattern.getTotalSteps(); ++s)
                {
                    bool active = stepStr[s] == '1';
                    float vel = (s < velTokens.size()) ? velTokens[s].getFloatValue() : 1.0f;
                    snap.drumPattern.setStep(t, s, active, vel);
                }
            }
        }

        if (auto* mixXml = snapXml->getChildByName("MixerSnapshot"))
        {
            for (auto* stripXml : mixXml->getChildWithTagNameIterator("Strip"))
            {
                int m = stripXml->getIntAttribute("index", -1);
                if (m < 0 || m >= kNumModules) continue;

                auto& strip = snap.mixerState.strips[static_cast<size_t>(m)];
                strip.level = static_cast<float>(stripXml->getDoubleAttribute("level", 1.0));
                strip.pan = static_cast<float>(stripXml->getDoubleAttribute("pan", 0.0));
                strip.mute = stripXml->getBoolAttribute("mute", false);
                strip.solo = stripXml->getBoolAttribute("solo", false);
                strip.send1 = static_cast<float>(stripXml->getDoubleAttribute("send1", 0.0));
                strip.send2 = static_cast<float>(stripXml->getDoubleAttribute("send2", 0.0));
            }
        }
        snapshots.push_back(snap);
    }
    
    if (snapshots.empty())
        addSnapshot(); // Ensure we always have at least one snapshot
}
} // namespace axelf
"""
    
    with open('C:/axelF/Source/Common/SnapshotManager.cpp', 'w', encoding='utf-8') as f:
        f.write(content[:to_xml_idx] + new_methods)

patch_file()
print("Patched!")
