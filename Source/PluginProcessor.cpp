#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Common/FactoryPresets.h"
#include "Common/EffectsParams.h"
#include <algorithm>
#include <chrono>

namespace axelf
{

AxelFProcessor::AxelFProcessor()
    : AudioProcessor(BusesProperties()
          .withOutput("Main", juce::AudioChannelSet::stereo(), true)),
      mixerAPVTS(*this, nullptr, "MixerParams", createMixerParameterLayout()),
      effectsAPVTS(*this, nullptr, "EffectsParams", createEffectsParameterLayout())
{
    mixer.bindToAPVTS(mixerAPVTS);

    FactoryPresets::installIfNeeded();

    presetManager.setModuleTrees({
        { "Jupiter8", &jupiter8.getAPVTS() },
        { "Moog15",   &moog15.getAPVTS()   },
        { "JX3P",     &jx3p.getAPVTS()     },
        { "DX7",      &dx7.getAPVTS()      },
        { "LinnDrum", &linnDrum.getAPVTS()  }
    });
    presetManager.loadFactoryPresets();

    modMatrix.addParameterTree(&jupiter8.getAPVTS());
    modMatrix.addParameterTree(&moog15.getAPVTS());
    modMatrix.addParameterTree(&jx3p.getAPVTS());
    modMatrix.addParameterTree(&dx7.getAPVTS());
    modMatrix.addParameterTree(&linnDrum.getAPVTS());
}

void AxelFProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    jupiter8.prepareToPlay(sampleRate, samplesPerBlock);
    moog15.prepareToPlay(sampleRate, samplesPerBlock);
    jx3p.prepareToPlay(sampleRate, samplesPerBlock);
    dx7.prepareToPlay(sampleRate, samplesPerBlock);
    linnDrum.prepareToPlay(sampleRate, samplesPerBlock);

    // Allocate per-module stereo buffers
    jup8Buffer.setSize(2, samplesPerBlock);
    moogBuffer.setSize(2, samplesPerBlock);
    jx3pBuffer.setSize(2, samplesPerBlock);
    dx7Buffer.setSize(2, samplesPerBlock);
    linnBuffer.setSize(2, samplesPerBlock);

    // Allocate aux send buffers
    aux1Buffer.setSize(2, samplesPerBlock);
    aux2Buffer.setSize(2, samplesPerBlock);

    // Prepare global effects
    plateReverb.prepare(sampleRate, samplesPerBlock);
    stereoDelay.prepare(sampleRate, samplesPerBlock);
    stereoChorus.prepare(sampleRate, samplesPerBlock);
    stereoFlanger.prepare(sampleRate, samplesPerBlock);
    distortion.prepare(sampleRate, samplesPerBlock);
    masterBus.prepare(sampleRate, samplesPerBlock);

    metronome.prepare(sampleRate);

    // Prepare mixer smoothing
    mixer.prepare(sampleRate);

    // Report total latency — max across all oversampled modules
    int maxLatency = std::max({ jupiter8.getLatencyInSamples(),
                                moog15.getLatencyInSamples(),
                                jx3p.getLatencyInSamples(),
                                dx7.getLatencyInSamples() });
    setLatencySamples(maxLatency);

    // Wire metronome click to transport beat callback
    transport.onMetronomeTick = [this](int /*beatInBar*/, bool isDownbeat)
    {
        metronome.trigger(isDownbeat);
    };

    prepared = true;
}

void AxelFProcessor::releaseResources()
{
    prepared = false;
    jupiter8.releaseResources();
    moog15.releaseResources();
    jx3p.releaseResources();
    dx7.releaseResources();
    linnDrum.releaseResources();
}

void AxelFProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (!prepared || buffer.getNumChannels() < 2)
        return;

    const auto blockStart = std::chrono::steady_clock::now();

    const int numSamples = buffer.getNumSamples();

    // ── 1. Advance transport ──────────────────────────────────
    // Check for host playhead sync
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue() && pos->getIsPlaying())
            {
                double hostBpm = *pos->getBpm();
                double hostPpq = pos->getPpqPosition().orFallback(0.0);
                transport.syncToHost(hostBpm, hostPpq, pos->getIsPlaying());
            }
        }
    }

    // Set pattern length override for Song mode (total arrangement bars)
    if (arrangement.getMode() == ArrangementMode::Song)
    {
        int totalBars = arrangement.getTotalBars(transport.getTimeSignatureNumerator());
        if (totalBars > 0)
        {
            // Valid arrangement: override transport length to span all blocks
            transport.setPatternLengthOverride(
                static_cast<double>(totalBars) * static_cast<double>(transport.getTimeSignatureNumerator()));
        }
        else
        {
            // Empty arrangement: keep normal pattern-length looping so the
            // user's patterns still play correctly while they build the timeline
            transport.setPatternLengthOverride(0.0);
        }
    }
    else
    {
        transport.setPatternLengthOverride(0.0);
    }

    transport.advance(numSamples, getSampleRate());

    // ── 1b. Merge virtual keyboard MIDI into incoming buffer ──
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // ── 1c. MIDI Learn: intercept CCs for learning / mapped params ──
    {
        std::array<juce::AudioProcessorValueTreeState*, 7> allApvts = {
            &jupiter8.getAPVTS(), &moog15.getAPVTS(), &jx3p.getAPVTS(),
            &dx7.getAPVTS(), &linnDrum.getAPVTS(), &mixerAPVTS, &effectsAPVTS
        };

        midiLearn.processMidi(midiMessages,
            [&](const juce::String& paramID) -> juce::RangedAudioParameter*
            {
                for (auto* apvts : allApvts)
                    if (auto* p = apvts->getParameter(paramID))
                        return p;
                return nullptr;
            });
    }

    // ── 2. Route live MIDI to per-module buffers ──────────────
    jup8Midi.clear();
    moogMidi.clear();
    jx3pMidi.clear();
    dx7Midi.clear();
    linnMidi.clear();

    const int active = activeModuleIndex.load();
    std::array<juce::MidiBuffer*, 5> liveMidiBufs = {
        &jup8Midi, &moogMidi, &jx3pMidi, &dx7Midi, &linnMidi
    };

    if (active >= 0 && active < 5)
    {
        for (const auto metadata : midiMessages)
            liveMidiBufs[static_cast<size_t>(active)]->addEvent(metadata.getMessage(),
                                                                metadata.samplePosition);
    }

    // ── 2b. Scene changes based on launch quantize (BEFORE pattern engine) ──
    // Determine whether this block should trigger queued scene switches
    bool shouldApplyQueuedScenes = false;
    {
        auto lq = sceneManager.getLaunchQuantize();
        if (lq == LaunchQuantize::Immediate)
            shouldApplyQueuedScenes = sceneManager.hasQueuedScenes();
        else if (lq == LaunchQuantize::NextBeat)
            shouldApplyQueuedScenes = transport.didBeatCross() || transport.didLoopReset();
        else // NextBar
            shouldApplyQueuedScenes = transport.didLoopReset();
    }

    // Always process chain advance on loop boundary regardless of launch quantize
    bool isLoopReset = transport.didLoopReset();

    if (shouldApplyQueuedScenes || isLoopReset)
    {
        // JAM mode: apply queued per-module scene changes
        if (arrangement.getMode() == ArrangementMode::Jam)
        {
            // Auto-save current patterns to per-module scenes on every loop
            // boundary.  This keeps scene storage in sync with live edits so
            // that Song-mode / REC-ARR always has up-to-date data.
            if (isLoopReset)
            {
                for (int mi = 0; mi < kNumModules; ++mi)
                    sceneManager.saveModuleToScene(sceneManager.getModuleScene(mi), mi, patternEngine);
            }

            if (shouldApplyQueuedScenes)
                sceneManager.applyQueuedScenes(patternEngine);

            // Chain advance (legacy full-scene chain) — always on loop boundary
            if (isLoopReset && sceneManager.isChainMode())
            {
                MixerSnapshot snap;
                for (int i = 0; i < kNumModules; ++i)
                {
                    snap.strips[static_cast<size_t>(i)].level = mixer.getLevel(i);
                    snap.strips[static_cast<size_t>(i)].pan   = mixer.getPan(i);
                    snap.strips[static_cast<size_t>(i)].mute  = mixer.getMute(i);
                    snap.strips[static_cast<size_t>(i)].solo  = mixer.getSolo(i);
                    snap.strips[static_cast<size_t>(i)].send1 = mixer.getSend1(i);
                    snap.strips[static_cast<size_t>(i)].send2 = mixer.getSend2(i);
                }
                if (sceneManager.advanceChain(patternEngine, snap))
                {
                    for (int i = 0; i < kNumModules; ++i)
                    {
                        mixer.setLevel(i, snap.strips[static_cast<size_t>(i)].level);
                        mixer.setPan(i, snap.strips[static_cast<size_t>(i)].pan);
                        mixer.setMute(i, snap.strips[static_cast<size_t>(i)].mute);
                        mixer.setSolo(i, snap.strips[static_cast<size_t>(i)].solo);
                        mixer.setSend1(i, snap.strips[static_cast<size_t>(i)].send1);
                        mixer.setSend2(i, snap.strips[static_cast<size_t>(i)].send2);
                    }
                }
            }
        }
    }

    // ── 2c. SONG mode: load patterns per-module based on timeline bar ──
    if (arrangement.getMode() == ArrangementMode::Song && transport.isPlaying())
    {
        // Force scene reload at arrangement start and on loop.
        // Use -2 = "uninitialized" so the per-module check below can
        // distinguish it from -1 = "already silenced".
        if (transport.didLoopReset())
            songModeActiveScenes.fill(-2);

        int currentBeat = static_cast<int>(transport.getPositionInBeats());
        for (int i = 0; i < kNumModules; ++i)
        {
            int sceneIdx = arrangement.getSceneForModuleAtBeat(i, currentBeat);
            if (sceneIdx >= 0 && sceneIdx != songModeActiveScenes[static_cast<size_t>(i)])
            {
                patternEngine.notifySceneChanged(i);
                sceneManager.loadSceneForModule(sceneIdx, i, patternEngine);
                songModeActiveScenes[static_cast<size_t>(i)] = sceneIdx;
            }
            else if (sceneIdx < 0 && songModeActiveScenes[static_cast<size_t>(i)] != -1)
            {
                // Lane is empty at this beat — silence the module.
                // Triggers on first evaluation after loop reset (-2) and
                // on transitions from an active scene (>= 0).
                patternEngine.notifySceneChanged(i);
                patternEngine.clearPattern(i);
                songModeActiveScenes[static_cast<size_t>(i)] = -1;
            }
        }
    }
    else
    {
        // Reset tracking when not in Song mode so scenes reload on re-entry
        songModeActiveScenes.fill(-2);
    }

    // Sync pattern bar counts from transport (skip in Song mode to preserve scene bar counts)
    if (arrangement.getMode() != ArrangementMode::Song)
        patternEngine.syncBarCounts(transport.getBarCount(), transport.getTimeSignatureNumerator());

    // ── 3. Pattern engine: record + playback → output MIDI ───
    jup8OutMidi.clear();
    moogOutMidi.clear();
    jx3pOutMidi.clear();
    dx7OutMidi.clear();
    linnOutMidi.clear();

    std::array<juce::MidiBuffer*, 5> outputMidiBufs = {
        &jup8OutMidi, &moogOutMidi, &jx3pOutMidi, &dx7OutMidi, &linnOutMidi
    };

    patternEngine.processBlock(transport, liveMidiBufs, outputMidiBufs,
                                numSamples, getSampleRate());

    // ── 3a. Track MIDI activity per module (for UI LEDs) ─────
    for (int i = 0; i < kNumModules; ++i)
    {
        if (!outputMidiBufs[static_cast<size_t>(i)]->isEmpty())
            midiActivity[static_cast<size_t>(i)].store(true, std::memory_order_relaxed);
    }

    // ── 3b. Inject all-notes-off if a module switch occurred ─
    {
        int offModule = pendingAllNotesOff.exchange(-1);
        if (offModule >= 0 && offModule < 5)
        {
            auto* buf = outputMidiBufs[static_cast<size_t>(offModule)];
            // CC 123 = All Notes Off on channel 1
            buf->addEvent(juce::MidiMessage::allNotesOff(1), 0);
            // Also send individual note-offs for safety (some synths ignore CC 123)
            for (int n = 0; n < 128; ++n)
                buf->addEvent(juce::MidiMessage::noteOff(1, n), 0);
        }
    }

    // ── 4. Apply modulation matrix ───────────────────────────
    modMatrix.process(numSamples);

    // ── 5. Process each module with pattern-merged MIDI ──────
    auto processModule = [&](ModuleProcessor& module,
                             juce::AudioBuffer<float>& modBuffer,
                             juce::MidiBuffer& modMidi)
    {
        modBuffer.setSize(2, numSamples, false, false, true);
        modBuffer.clear();
        module.processBlock(modBuffer, modMidi);
    };

    // Set transport info for arpeggiator
    jupiter8.setTransportInfo(transport.getBlockStartBeat(),
                              static_cast<double>(transport.getBpm()));

    processModule(jupiter8, jup8Buffer, jup8OutMidi);
    processModule(moog15, moogBuffer, moogOutMidi);
    processModule(jx3p, jx3pBuffer, jx3pOutMidi);
    processModule(dx7, dx7Buffer, dx7OutMidi);
    processModule(linnDrum, linnBuffer, linnOutMidi);

    // ── 6. Mix all modules to output (with aux sends) ──────
    std::array<juce::AudioBuffer<float>*, 5> moduleBuffers = {
        &jup8Buffer, &moogBuffer, &jx3pBuffer, &dx7Buffer, &linnBuffer
    };

    aux1Buffer.setSize(2, numSamples, false, false, true);
    aux2Buffer.setSize(2, numSamples, false, false, true);
    aux3Buffer.setSize(2, numSamples, false, false, true);
    aux4Buffer.setSize(2, numSamples, false, false, true);
    aux5Buffer.setSize(2, numSamples, false, false, true);
    mixer.process(moduleBuffers, buffer, aux1Buffer, aux2Buffer, aux3Buffer, aux4Buffer, aux5Buffer);

    // ── 6a. Process send effects ─────────────────────────────
    {
        // Read effects parameters from APVTS
        auto safeLoad = [&](const char* id, float def) -> float {
            if (auto* p = effectsAPVTS.getRawParameterValue(id))
                return p->load();
            return def;
        };

        // Reverb params
        plateReverb.setSize(safeLoad("fx_reverb_size", 0.5f));
        plateReverb.setDamping(safeLoad("fx_reverb_damping", 0.5f));
        plateReverb.setWidth(safeLoad("fx_reverb_width", 1.0f));
        plateReverb.setMix(safeLoad("fx_reverb_mix", 1.0f));
        plateReverb.setPreDelay(safeLoad("fx_reverb_predelay", 10.0f));
        plateReverb.process(aux1Buffer);

        // Delay params
        stereoDelay.setTimeL(safeLoad("fx_delay_time_l", 375.0f));
        stereoDelay.setTimeR(safeLoad("fx_delay_time_r", 375.0f));
        stereoDelay.setFeedback(safeLoad("fx_delay_feedback", 0.3f));
        stereoDelay.setMix(safeLoad("fx_delay_mix", 0.5f));
        stereoDelay.setHighCut(safeLoad("fx_delay_highcut", 12000.0f));
        stereoDelay.setPingPong(safeLoad("fx_delay_ping_pong", 0.0f) >= 0.5f);
        stereoDelay.setSyncMode(static_cast<dsp::StereoDelay::SyncMode>(
            static_cast<int>(safeLoad("fx_delay_sync", 0.0f))));
        stereoDelay.setTempoBpm(transport.getBpm());
        stereoDelay.process(aux2Buffer);

        // Chorus params
        stereoChorus.setRate(safeLoad("fx_chorus_rate", 1.5f));
        stereoChorus.setDepth(safeLoad("fx_chorus_depth", 0.5f));
        stereoChorus.setMix(safeLoad("fx_chorus_mix", 0.5f));
        stereoChorus.process(aux3Buffer);

        // Flanger params
        stereoFlanger.setRate(safeLoad("fx_flanger_rate", 0.3f));
        stereoFlanger.setDepth(safeLoad("fx_flanger_depth", 0.5f));
        stereoFlanger.setFeedback(safeLoad("fx_flanger_feedback", 0.5f));
        stereoFlanger.setMix(safeLoad("fx_flanger_mix", 0.5f));
        stereoFlanger.process(aux4Buffer);

        // Distortion params
        distortion.setDrive(safeLoad("fx_distortion_drive", 0.3f));
        distortion.setTone(safeLoad("fx_distortion_tone", 0.5f));
        distortion.setMix(safeLoad("fx_distortion_mix", 0.5f));
        distortion.process(aux5Buffer);

        // Sum send returns into main output (with gain compensation
        // to prevent wet+dry summing from clipping)
        for (int ch = 0; ch < 2; ++ch)
        {
            buffer.addFrom(ch, 0, aux1Buffer, ch, 0, numSamples, 0.7f);   // Reverb
            buffer.addFrom(ch, 0, aux2Buffer, ch, 0, numSamples, 0.5f);   // Delay (-6 dB)
            buffer.addFrom(ch, 0, aux3Buffer, ch, 0, numSamples, 0.7f);   // Chorus
            buffer.addFrom(ch, 0, aux4Buffer, ch, 0, numSamples, 0.7f);   // Flanger
            buffer.addFrom(ch, 0, aux5Buffer, ch, 0, numSamples, 0.7f);   // Distortion
        }
    }

    // ── 6b. Master bus processing (EQ → Comp → Limiter) ─────
    {
        auto safeLoad = [&](const char* id, float def) -> float {
            if (auto* p = effectsAPVTS.getRawParameterValue(id))
                return p->load();
            return def;
        };

        masterBus.setEqLow(safeLoad("fx_master_eq_low", 0.0f));
        masterBus.setEqMid(safeLoad("fx_master_eq_mid", 0.0f));
        masterBus.setEqHigh(safeLoad("fx_master_eq_high", 0.0f));
        masterBus.setCompThreshold(safeLoad("fx_master_comp_threshold", 0.0f));
        masterBus.setCompRatio(safeLoad("fx_master_comp_ratio", 1.0f));
        masterBus.setCompAttack(safeLoad("fx_master_comp_attack", 10.0f));
        masterBus.setCompRelease(safeLoad("fx_master_comp_release", 100.0f));
        masterBus.setLimiterEnabled(safeLoad("fx_master_limiter", 1.0f) >= 0.5f);
        masterBus.process(buffer);
    }

    // ── 6c. Final safety clip — prevent any residual overs ───
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            data[s] = std::clamp(data[s], -1.0f, 1.0f);
    }

    // ── 6c. Render metronome click (count-in + normal playback)
    if (buffer.getNumChannels() >= 2)
        metronome.renderBlock(buffer.getWritePointer(0),
                              buffer.getWritePointer(1), numSamples);

    // ── 7. CPU usage metering ────────────────────────────────
    {
        const auto blockEnd = std::chrono::steady_clock::now();
        const double elapsedUs = std::chrono::duration<double, std::micro>(blockEnd - blockStart).count();
        const double budgetUs = 1.0e6 * static_cast<double>(numSamples) / getSampleRate();
        cpuUsage.store(static_cast<float>(elapsedUs / budgetUs), std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* AxelFProcessor::createEditor()
{
    return new AxelFEditor(*this);
}

void AxelFProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto root = std::make_unique<juce::XmlElement>("AxelFState");
    root->setAttribute("activeModule", activeModuleIndex.load());

    auto addModuleState = [&](const char* tag, juce::AudioProcessorValueTreeState& apvts)
    {
        if (auto xml = apvts.copyState().createXml())
        {
            auto* wrapper = root->createNewChildElement(tag);
            wrapper->addChildElement(xml.release());
        }
    };

    addModuleState("Jupiter8", jupiter8.getAPVTS());
    addModuleState("Moog15",   moog15.getAPVTS());
    addModuleState("JX3P",     jx3p.getAPVTS());
    addModuleState("DX7",      dx7.getAPVTS());
    addModuleState("LinnDrum", linnDrum.getAPVTS());
    addModuleState("Mixer",    mixerAPVTS);
    addModuleState("Effects",  effectsAPVTS);

    // MIDI Learn mappings
    if (auto mlXml = midiLearn.toXml())
        root->addChildElement(mlXml.release());

    // Session data (patterns, scenes, transport, arrangement)
    {
        auto* session = root->createNewChildElement("Session");

        if (auto tXml = transport.toXml())
            session->addChildElement(tXml.release());

        if (auto peXml = patternEngine.toXml())
            session->addChildElement(peXml.release());

        if (auto smXml = sceneManager.toXml())
            session->addChildElement(smXml.release());

        if (auto arrXml = arrangement.toXml())
            session->addChildElement(arrXml.release());
    }

    copyXmlToBinary(*root, destData);
}

void AxelFProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto root = getXmlFromBinary(data, sizeInBytes);
    if (root == nullptr || !root->hasTagName("AxelFState"))
        return;

    activeModuleIndex.store(root->getIntAttribute("activeModule", 0));

    auto restoreModuleState = [](const char* tag, juce::XmlElement& parent,
                                  juce::AudioProcessorValueTreeState& apvts)
    {
        if (auto* wrapper = parent.getChildByName(tag))
        {
            if (auto* child = wrapper->getFirstChildElement())
                apvts.replaceState(juce::ValueTree::fromXml(*child));
        }
    };

    restoreModuleState("Jupiter8", *root, jupiter8.getAPVTS());
    restoreModuleState("Moog15",   *root, moog15.getAPVTS());
    restoreModuleState("JX3P",     *root, jx3p.getAPVTS());
    restoreModuleState("DX7",      *root, dx7.getAPVTS());
    restoreModuleState("LinnDrum", *root, linnDrum.getAPVTS());
    restoreModuleState("Mixer",    *root, mixerAPVTS);
    restoreModuleState("Effects",  *root, effectsAPVTS);

    // MIDI Learn mappings
    if (auto* mlXml = root->getChildByName("MidiLearn"))
        midiLearn.fromXml(*mlXml);

    // Session data (patterns, scenes, transport, arrangement)
    if (auto* session = root->getChildByName("Session"))
    {
        if (auto* tXml = session->getChildByName("Transport"))
            transport.fromXml(tXml);

        if (auto* peXml = session->getChildByName("PatternEngine"))
            patternEngine.fromXml(peXml);

        if (auto* smXml = session->getChildByName("SceneManager"))
            sceneManager.fromXml(smXml);

        if (auto* arrXml = session->getChildByName("Arrangement"))
            arrangement.fromXml(arrXml);
    }
}

// ── Session file management ─────────────────────────────────────

juce::File AxelFProcessor::getSessionsDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("AxelF").getChildFile("Sessions");
    dir.createDirectory();
    return dir;
}

juce::File AxelFProcessor::getAutoSaveFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("AxelF");
    dir.createDirectory();
    return dir.getChildFile("autosave.axelf");
}

juce::File AxelFProcessor::getSettingsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("AxelF");
    dir.createDirectory();
    return dir.getChildFile("settings.xml");
}

void AxelFProcessor::rememberLastSession()
{
    if (currentSessionFile == juce::File()) return;

    auto xml = std::make_unique<juce::XmlElement>("AxelFSettings");
    xml->setAttribute("lastSession", currentSessionFile.getFullPathName());
    xml->writeTo(getSettingsFile());
}

juce::File AxelFProcessor::getLastSessionFile()
{
    auto settingsFile = getSettingsFile();
    if (!settingsFile.existsAsFile()) return {};

    auto xml = juce::XmlDocument::parse(settingsFile);
    if (xml == nullptr || !xml->hasTagName("AxelFSettings")) return {};

    auto path = xml->getStringAttribute("lastSession");
    if (path.isEmpty()) return {};

    juce::File f(path);
    return f.existsAsFile() ? f : juce::File();
}

bool AxelFProcessor::saveSessionToFile(const juce::File& file)
{
    juce::MemoryBlock data;
    getStateInformation(data);

    if (file.replaceWithData(data.getData(), data.getSize()))
    {
        currentSessionFile = file;
        rememberLastSession();
        return true;
    }
    return false;
}

bool AxelFProcessor::loadSessionFromFile(const juce::File& file)
{
    juce::MemoryBlock data;
    if (!file.loadFileAsData(data) || data.getSize() == 0)
        return false;

    setStateInformation(data.getData(), static_cast<int>(data.getSize()));
    currentSessionFile = file;
    rememberLastSession();
    return true;
}

void AxelFProcessor::newSession()
{
    // Reset all APVTS to defaults
    auto resetAPVTS = [](juce::AudioProcessorValueTreeState& apvts)
    {
        auto state = apvts.copyState();
        for (auto* param : apvts.processor.getParameters())
        {
            if (auto* rpwv = dynamic_cast<juce::RangedAudioParameter*>(param))
                rpwv->setValueNotifyingHost(rpwv->getDefaultValue());
        }
    };

    resetAPVTS(jupiter8.getAPVTS());
    resetAPVTS(moog15.getAPVTS());
    resetAPVTS(jx3p.getAPVTS());
    resetAPVTS(dx7.getAPVTS());
    resetAPVTS(linnDrum.getAPVTS());
    resetAPVTS(mixerAPVTS);
    resetAPVTS(effectsAPVTS);

    // Clear patterns, scenes, arrangement
    for (int i = 0; i < 4; ++i)
        patternEngine.getSynthPattern(i).clear();
    patternEngine.getDrumPattern().clear();

    sceneManager = SceneManager();
    arrangement.clear();
    arrangement.setMode(ArrangementMode::Jam);

    transport.setBpm(120.0f);
    transport.setBarCount(4);

    midiLearn.clearAll();

    currentSessionFile = juce::File();
    activeModuleIndex.store(0);
}

} // namespace axelf

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new axelf::AxelFProcessor();
}




// Touched
