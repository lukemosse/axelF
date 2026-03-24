import sys

def patch_file():
    with open('Source/Common/SnapshotManager.cpp', 'r', encoding='utf-8') as f:
        content = f.read()

    s = '} // namespace axelf'
    replacement = '''

bool SnapshotManager::saveSnapshotToFile(int index, const juce::File& file) const
{
    if (index < 0 || index >= getSnapshotCount())
        return false;

    // We can reuse our full XML logic, just extract the single Snapshot node
    auto fullXml = toXml();
    if (!fullXml) return false;

    // The children of SnapshotManager node are the SNAPSHOTs
    auto* snapXml = fullXml->getChildElement(index);
    if (!snapXml) return false;

    return snapXml->writeToFile(file, juce::StringRef());
}

bool SnapshotManager::loadSnapshotFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName("SNAPSHOT"))
        return false;

    Snapshot snap;
    snap.name = xml->getStringAttribute("Name", file.getFileNameWithoutExtension());
    
    for (auto* spXml : xml->getChildWithTagNameIterator("SynthPattern"))
    {
        int m = spXml->getIntAttribute("module", -1);
        if (m < 0 || m >= 4) continue;
        auto& spec = snap.synthPatterns[static_cast<size_t>(m)];
        spec.clear();
        spec.setBarCount(spXml->getIntAttribute("barCount", 4));
        spec.setBeatsPerBar(spXml->getIntAttribute("beatsPerBar", 4));
        for (auto* evtXml : spXml->getChildWithTagNameIterator("Event"))
        {
            MidiEvent evt;
            evt.noteNumber = evtXml->getIntAttribute("note", 60);
            evt.velocity = static_cast<float>(evtXml->getDoubleAttribute("vel", 1.0));
            evt.startBeat = evtXml->getDoubleAttribute("start", 0.0);
            evt.duration = evtXml->getDoubleAttribute("dur", 0.25);
            spec.addEvent(evt);
        }
    }

    if (auto* dpXml = xml->getChildByName("DrumPattern"))
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

    if (auto* mixXml = xml->getChildByName("MixerSnapshot"))
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
    activeSnapshotIndex = static_cast<int>(snapshots.size()) - 1;
    return true;
}

} // namespace axelf
'''
    
    with open('Source/Common/SnapshotManager.cpp', 'w', encoding='utf-8') as f:
        f.write(content.replace(s, replacement))

patch_file()
print("Patched!")
