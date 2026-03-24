#pragma once

#include <optional>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Common/SceneManager.h"
#include "../Common/PatternEngine.h"
#include "../Common/GlobalTransport.h"
#include "../Common/MasterMixer.h"
#include "../Common/ArrangementTimeline.h"
#include "AxelFColours.h"

namespace axelf::ui
{

// Clip-launcher matrix: scenes (rows) × instruments (columns).
// Each cell = one module's pattern for one scene.
// Click a cell to launch that module into that scene.
// Click the row ▶ button to launch the entire scene.
// Drag a ▶ button to the arranger to place a scene on the timeline.
class ClipMatrixPanel : public juce::Component,
                        private juce::Timer
{
public:
    ClipMatrixPanel(SceneManager& sm, PatternEngine& pe,
                    GlobalTransport& tr, MasterMixer& mx,
                    ArrangementTimeline& arr)
        : sceneManager(sm), patternEngine(pe), transport(tr), mixer(mx),
          arrangement(arr)
    {
        startTimerHz(15);
    }

    // Layout constants
    static constexpr int kSceneHeaderWidth = 22;      // left column: scene letters
    static constexpr int kModuleHeaderHeight = 18;     // top row: module names
    static constexpr int kLaunchBtnWidth = 24;         // right column: ▶ launch buttons
    static constexpr int kCellMinW = 30;
    static constexpr int kCellMinH = 14;
    static constexpr int kDragThreshold = 5;
    static constexpr int kMuteRowHeight = 20;  // bottom row: per-module mute buttons

    // Scene pagination (set by parent TimelinePanel)
    int sceneOffset = 0;
    int numVisibleScenes = 8;

    void setSceneOffset(int offset) { sceneOffset = offset; repaint(); }

    // Drag-to-arranger callback: called on drop with scene index + screen position
    std::function<void(int sceneIndex, juce::Point<int> screenPos)> onSceneDropped;
    // Drag individual cell to arranger: called with module + scene + screen position
    std::function<void(int moduleIndex, int sceneIndex, juce::Point<int> screenPos)> onCellDropped;
    // Called when a cell is clicked — used to switch instrument tab
    std::function<void(int moduleIndex)> onModuleSelected;
    int getDraggingScene() const { return draggingScene; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(Colours::bgDark));

        const bool inactive = (arrangement.getMode() == ArrangementMode::Song);
        const int numModules = kNumModules;
        const int visScenes = std::min(numVisibleScenes,
                                       SceneManager::kMaxScenes - sceneOffset);

        const float headerH = static_cast<float>(kModuleHeaderHeight);
        const float sceneW = static_cast<float>(kSceneHeaderWidth);
        const float launchW = static_cast<float>(kLaunchBtnWidth);
        const float muteH = static_cast<float>(kMuteRowHeight);
        const float gridW = bounds.getWidth() - sceneW - launchW;
        const float gridH = bounds.getHeight() - headerH - muteH;
        const float cellW = std::max(static_cast<float>(kCellMinW),
                                     gridW / static_cast<float>(numModules));
        const float cellH = std::max(static_cast<float>(kCellMinH),
                                     gridH / static_cast<float>(visScenes));

        static constexpr juce::uint32 moduleColours[kNumModules] = {
            Colours::jupiter, Colours::moog, Colours::jx3p, Colours::dx7, Colours::linn
        };
        static const char* moduleNames[kNumModules] = {
            "JUP8", "MOOG", "JX3P", "DX7", "LINN"
        };

        // ── Column headers (module names, top row) ───────────
        g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
        for (int m = 0; m < numModules; ++m)
        {
            float x = sceneW + static_cast<float>(m) * cellW;
            g.setColour(juce::Colour(moduleColours[m]));
            g.drawText(moduleNames[m], static_cast<int>(x), 0,
                       static_cast<int>(cellW), static_cast<int>(headerH),
                       juce::Justification::centred, false);
        }

        // ── Row headers (scene letters, left column) ─────────
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        for (int row = 0; row < visScenes; ++row)
        {
            int s = sceneOffset + row;
            float y = headerH + static_cast<float>(row) * cellH;

            juce::String label = juce::String::charToString(
                static_cast<juce::juce_wchar>('A' + s));
            auto name = sceneManager.getSceneName(s);
            if (!name.startsWith("Scene "))
                label = name.substring(0, 2);

            // Highlight if all modules are in this scene
            bool allPlaying = true;
            for (int m = 0; m < numModules; ++m)
                if (sceneManager.getModuleScene(m) != s)
                    allPlaying = false;

            g.setColour(allPlaying && transport.isPlaying()
                ? juce::Colour(Colours::accentGold)
                : juce::Colour(Colours::textSecondary));
            g.drawText(label, 2, static_cast<int>(y),
                       kSceneHeaderWidth - 4, static_cast<int>(cellH),
                       juce::Justification::centred, false);
        }

        // ── Grid cells (rows=scenes, cols=modules) ──────────
        for (int row = 0; row < visScenes; ++row)
        {
            int s = sceneOffset + row;
            float y = headerH + static_cast<float>(row) * cellH;

            for (int m = 0; m < numModules; ++m)
            {
                float x = sceneW + static_cast<float>(m) * cellW;
                auto modCol = juce::Colour(moduleColours[m]);
                if (inactive)
                    modCol = modCol.withMultipliedSaturation(0.25f).withMultipliedBrightness(0.5f);

                auto cellRect = juce::Rectangle<float>(
                    x + 1.0f, y + 1.0f, cellW - 2.0f, cellH - 2.0f);

                int currentModuleScene = sceneManager.getModuleScene(m);
                int queuedModuleScene = sceneManager.getQueuedModuleScene(m);
                bool hasContent = sceneHasContentForModule(s, m);
                bool isPlaying = (currentModuleScene == s) && transport.isPlaying();
                bool isQueued = (queuedModuleScene == s);
                bool isActive = (currentModuleScene == s) && !transport.isPlaying();

                if (isPlaying)
                {
                    g.setColour(modCol.withAlpha(0.75f));
                    g.fillRoundedRectangle(cellRect, 3.0f);
                    float pulse = 0.5f + 0.5f * std::sin(
                        static_cast<float>(juce::Time::getMillisecondCounter()) * 0.006f);
                    g.setColour(modCol.withAlpha(pulse));
                    g.drawRoundedRectangle(cellRect, 3.0f, 2.0f);
                }
                else if (isQueued)
                {
                    bool flash = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
                    g.setColour(modCol.withAlpha(hasContent ? 0.4f : 0.15f));
                    g.fillRoundedRectangle(cellRect, 3.0f);
                    if (flash)
                    {
                        g.setColour(juce::Colour(Colours::accentGold));
                        g.drawRoundedRectangle(cellRect, 3.0f, 2.0f);
                    }
                }
                else if (isActive)
                {
                    g.setColour(modCol.withAlpha(0.5f));
                    g.fillRoundedRectangle(cellRect, 3.0f);
                    g.setColour(modCol.withAlpha(0.8f));
                    g.drawRoundedRectangle(cellRect, 3.0f, 1.5f);
                }
                else if (hasContent)
                {
                    g.setColour(modCol.withAlpha(0.25f));
                    g.fillRoundedRectangle(cellRect, 3.0f);
                    g.setColour(modCol.withAlpha(0.4f));
                    g.drawRoundedRectangle(cellRect, 3.0f, 1.0f);
                }
                else
                {
                    g.setColour(juce::Colour(Colours::bgSection).withAlpha(0.4f));
                    g.fillRoundedRectangle(cellRect, 3.0f);
                }

                if (hasContent)
                    drawMiniThumbnail(g, s, m, cellRect.reduced(3.0f, 3.0f));
            }
        }

        // ── Launch buttons (right column, per-row ▶) ────────
        float launchX = sceneW + static_cast<float>(numModules) * cellW;
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        for (int row = 0; row < visScenes; ++row)
        {
            int s = sceneOffset + row;
            float y = headerH + static_cast<float>(row) * cellH;

            auto btnRect = juce::Rectangle<float>(
                launchX + 2.0f, y + 2.0f, launchW - 4.0f, cellH - 4.0f);

            bool allPlaying = true;
            for (int m = 0; m < numModules; ++m)
                if (sceneManager.getModuleScene(m) != s)
                    allPlaying = false;

            if (allPlaying && transport.isPlaying())
                g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.5f));
            else if (draggingScene == s)
                g.setColour(juce::Colour(Colours::accentBlue).withAlpha(0.5f));
            else
                g.setColour(juce::Colour(Colours::bgControl));

            g.fillRoundedRectangle(btnRect, 2.0f);
            g.setColour(juce::Colour(Colours::textSecondary));
            g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6")),
                       static_cast<int>(btnRect.getX()), static_cast<int>(btnRect.getY()),
                       static_cast<int>(btnRect.getWidth()), static_cast<int>(btnRect.getHeight()),
                       juce::Justification::centred, false);
        }

        // ── Grid lines ──────────────────────────────────────
        g.setColour(juce::Colour(Colours::borderSubtle).withAlpha(0.3f));
        for (int m = 1; m < numModules; ++m)
        {
            float x = sceneW + static_cast<float>(m) * cellW;
            g.drawLine(x, headerH, x,
                       headerH + static_cast<float>(visScenes) * cellH, 0.5f);
        }
        for (int row = 1; row < visScenes; ++row)
        {
            float y = headerH + static_cast<float>(row) * cellH;
            g.drawLine(sceneW, y, sceneW + gridW, y, 0.5f);
        }

        // ── Mute row (bottom) ───────────────────────────────
        {
            float muteY = bounds.getHeight() - muteH;
            g.setColour(juce::Colour(Colours::bgPanel));
            g.fillRect(0.0f, muteY, bounds.getWidth(), muteH);

            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            for (int m = 0; m < numModules; ++m)
            {
                float x = sceneW + static_cast<float>(m) * cellW;
                auto btnRect = juce::Rectangle<float>(x + 2.0f, muteY + 2.0f,
                                                       cellW - 4.0f, muteH - 4.0f);

                bool muted = patternEngine.isPatternMuted(m);
                auto modCol = juce::Colour(moduleColours[m]);

                if (muted)
                {
                    g.setColour(juce::Colour(Colours::accentRed).withAlpha(0.6f));
                    g.fillRoundedRectangle(btnRect, 3.0f);
                    g.setColour(juce::Colour(Colours::textPrimary));
                    g.drawText("OFF", btnRect.toNearestInt(), juce::Justification::centred, false);
                }
                else
                {
                    g.setColour(modCol.withAlpha(0.3f));
                    g.fillRoundedRectangle(btnRect, 3.0f);
                    g.setColour(modCol.withAlpha(0.6f));
                    g.drawRoundedRectangle(btnRect, 3.0f, 1.0f);
                    g.setColour(juce::Colour(Colours::textSecondary));
                    g.drawText("ON", btnRect.toNearestInt(), juce::Justification::centred, false);
                }
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const float headerH = static_cast<float>(kModuleHeaderHeight);
        const float sceneW = static_cast<float>(kSceneHeaderWidth);
        const float gridW = static_cast<float>(getWidth()) - sceneW
                            - static_cast<float>(kLaunchBtnWidth);
        const float muteH = static_cast<float>(kMuteRowHeight);
        const float muteY = static_cast<float>(getHeight()) - muteH;

        // ── Mute row click ──────────────────────────────────
        if (static_cast<float>(e.y) >= muteY)
        {
            float relX = static_cast<float>(e.x) - sceneW;
            if (relX >= 0.0f && relX < gridW)
            {
                float cellW = std::max(static_cast<float>(kCellMinW),
                    gridW / static_cast<float>(kNumModules));
                int m = static_cast<int>(relX / cellW);
                if (m >= 0 && m < kNumModules)
                    patternEngine.setPatternMute(m, !patternEngine.isPatternMuted(m));
            }
            return;
        }

        const int visScenes = std::min(numVisibleScenes,
                                       SceneManager::kMaxScenes - sceneOffset);
        const float cellH = std::max(static_cast<float>(kCellMinH),
            (static_cast<float>(getHeight()) - headerH - muteH) / static_cast<float>(visScenes));
        float launchX = sceneW + gridW;

        // Check if click is on scene label (left) or launch button (right)
        bool onSceneLabel = static_cast<float>(e.x) < sceneW;
        bool onLaunchBtn = static_cast<float>(e.x) >= launchX;

        if (onSceneLabel || onLaunchBtn)
        {
            float relY = static_cast<float>(e.y) - headerH;
            if (relY >= 0)
            {
                int row = static_cast<int>(relY / cellH);
                if (row >= 0 && row < visScenes)
                {
                    int scene = sceneOffset + row;
                    potentialDragScene = scene;
                    potentialDragModule = -1; // full-scene drag
                    dragStartPos = e.getPosition();
                }
            }
            return;
        }

        auto [module, scene] = getCellAt(e.getPosition());
        if (module < 0 || scene < 0) return;

        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Launch for " + juce::String(getModuleName(module)));
            menu.addItem(2, "Launch Entire Scene");
            menu.addSeparator();
            menu.addItem(3, "Copy Cell");

            bool canPaste = (module == kLinnDrum)
                ? clipboardDrum.has_value()
                : clipboardSynth.has_value();
            menu.addItem(4, "Paste Cell", canPaste);
            menu.addSeparator();
            menu.addItem(5, "Clear Cell");

            const int m = module;
            const int s = scene;
            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this, m, s](int result)
                {
                    switch (result)
                    {
                        case 1: launchCellForModule(s, m); break;
                        case 2: launchFullScene(s); break;
                        case 3: copyCellToClipboard(s, m); break;
                        case 4: pasteCellFromClipboard(s, m); break;
                        case 5: clearScenePattern(s, m); break;
                        default: break;
                    }
                });
            return;
        }

        // Notify parent to switch instrument tab
        if (onModuleSelected)
            onModuleSelected(module);

        // Left-click: start potential cell drag (launch happens on mouseUp if no drag)
        potentialDragScene = scene;
        potentialDragModule = module;
        dragStartPos = e.getPosition();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (potentialDragScene >= 0 && draggingScene < 0)
        {
            auto dist = e.getPosition().getDistanceFrom(dragStartPos);
            if (dist > kDragThreshold)
            {
                draggingScene = potentialDragScene;
                draggingModule = potentialDragModule;
                setMouseCursor(juce::MouseCursor::DraggingHandCursor);
                repaint();
            }
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (draggingScene >= 0)
        {
            if (draggingModule >= 0 && onCellDropped)
                onCellDropped(draggingModule, draggingScene, e.getScreenPosition());
            else if (onSceneDropped)
                onSceneDropped(draggingScene, e.getScreenPosition());
            draggingScene = -1;
            draggingModule = -1;
            potentialDragScene = -1;
            potentialDragModule = -1;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
            return;
        }

        if (potentialDragScene >= 0)
        {
            if (arrangement.getMode() == ArrangementMode::Jam)
            {
                if (potentialDragModule >= 0)
                    launchCellForModule(potentialDragScene, potentialDragModule);
                else
                    launchFullScene(potentialDragScene);
            }
            potentialDragScene = -1;
            potentialDragModule = -1;
        }
    }

private:
    SceneManager& sceneManager;
    PatternEngine& patternEngine;
    GlobalTransport& transport;
    MasterMixer& mixer;
    ArrangementTimeline& arrangement;

    // Drag state
    int potentialDragScene = -1;
    int potentialDragModule = -1;
    int draggingScene = -1;
    int draggingModule = -1;
    juce::Point<int> dragStartPos;

    void timerCallback() override { repaint(); }

    std::pair<int, int> getCellAt(juce::Point<int> pos) const
    {
        // Returns {module (column), scene (absolute index)}
        const float headerH = static_cast<float>(kModuleHeaderHeight);
        const float sceneW = static_cast<float>(kSceneHeaderWidth);
        const float gridW = static_cast<float>(getWidth()) - sceneW
                            - static_cast<float>(kLaunchBtnWidth);
        const int visScenes = std::min(numVisibleScenes,
                                       SceneManager::kMaxScenes - sceneOffset);
        const float cellW = std::max(static_cast<float>(kCellMinW),
                                     gridW / static_cast<float>(kNumModules));
        const float cellH = std::max(static_cast<float>(kCellMinH),
            (static_cast<float>(getHeight()) - headerH - static_cast<float>(kMuteRowHeight))
             / static_cast<float>(visScenes));

        float relX = static_cast<float>(pos.getX()) - sceneW;
        float relY = static_cast<float>(pos.getY()) - headerH;

        if (relX < 0 || relY < 0) return {-1, -1};
        // Click is in the mute row area
        if (static_cast<float>(pos.getY()) >= static_cast<float>(getHeight()) - static_cast<float>(kMuteRowHeight))
            return {-1, -1};

        int module = static_cast<int>(relX / cellW);
        int row = static_cast<int>(relY / cellH);

        if (module < 0 || module >= kNumModules) return {-1, -1};
        if (row < 0 || row >= visScenes) return {-1, -1};

        int scene = sceneOffset + row;
        return {module, scene};
    }

    bool sceneHasContentForModule(int sceneIndex, int moduleIndex) const
    {
        if (sceneIndex < 0 || sceneIndex >= SceneManager::kMaxScenes) return false;
        if (moduleIndex < 0 || moduleIndex >= kNumModules) return false;

        const auto& scene = sceneManager.getScene(sceneIndex);

        if (moduleIndex == kLinnDrum)
        {
            // Check if drum pattern has any steps
            const auto& dp = scene.drumPattern;
            for (int t = 0; t < DrumPattern::kMaxTracks; ++t)
                for (int s = 0; s < dp.getLoopSteps(); ++s)
                    if (dp.getStep(t, s))
                        return true;
            return false;
        }

        // Synth pattern
        return scene.synthPatterns[static_cast<size_t>(moduleIndex)].getNumEvents() > 0;
    }

    void drawMiniThumbnail(juce::Graphics& g, int sceneIndex, int moduleIndex,
                           juce::Rectangle<float> area) const
    {
        if (area.getWidth() < 4.0f || area.getHeight() < 4.0f) return;

        const auto& scene = sceneManager.getScene(sceneIndex);

        g.setColour(juce::Colour(Colours::textPrimary).withAlpha(0.4f));

        if (moduleIndex == kLinnDrum)
        {
            // For drums: draw tiny dots for active steps in the first few tracks
            const auto& dp = scene.drumPattern;
            int maxSteps = std::min(dp.getLoopSteps(), 16);
            int maxTracks = std::min(DrumPattern::kMaxTracks, 6); // show top 6 tracks
            float dotW = area.getWidth() / static_cast<float>(maxSteps);
            float dotH = area.getHeight() / static_cast<float>(maxTracks);

            for (int t = 0; t < maxTracks; ++t)
                for (int s = 0; s < maxSteps; ++s)
                    if (dp.getStep(t, s))
                        g.fillRect(area.getX() + static_cast<float>(s) * dotW,
                                   area.getY() + static_cast<float>(t) * dotH,
                                   std::max(1.0f, dotW - 0.5f),
                                   std::max(1.0f, dotH - 0.5f));
        }
        else
        {
            // For synths: draw tiny bars for events
            const auto& sp = scene.synthPatterns[static_cast<size_t>(moduleIndex)];
            double len = sp.getLengthInBeats();
            if (len <= 0.0 || sp.getNumEvents() == 0) return;

            for (size_t i = 0; i < sp.getNumEvents(); ++i)
            {
                const auto& evt = sp.getEvent(i);
                float xFrac = static_cast<float>(evt.startBeat / len);
                float yFrac = 1.0f - static_cast<float>(evt.noteNumber - 36) / 60.0f;
                yFrac = std::clamp(yFrac, 0.0f, 1.0f);
                float wFrac = static_cast<float>(evt.duration / len);

                g.fillRect(area.getX() + xFrac * area.getWidth(),
                           area.getY() + yFrac * area.getHeight(),
                           std::max(1.0f, wFrac * area.getWidth()),
                           1.0f);
            }
        }
    }

    void launchCellForModule(int sceneIndex, int moduleIndex)
    {
        if (transport.isPlaying())
        {
            sceneManager.queueModuleScene(moduleIndex, sceneIndex);
        }
        else
        {
            // Save only outgoing module's pattern to its current scene
            int outgoingScene = sceneManager.getModuleScene(moduleIndex);
            sceneManager.saveModuleToScene(outgoingScene, moduleIndex, patternEngine);

            sceneManager.loadSceneForModule(sceneIndex, moduleIndex, patternEngine);
            sceneManager.setModuleScene(moduleIndex, sceneIndex);
        }
        repaint();
    }

    void launchFullScene(int sceneIndex)
    {
        if (transport.isPlaying())
        {
            for (int m = 0; m < kNumModules; ++m)
                sceneManager.queueModuleScene(m, sceneIndex);
        }
        else
        {
            // Save each module's pattern to its current per-module scene
            for (int m = 0; m < kNumModules; ++m)
                sceneManager.saveModuleToScene(sceneManager.getModuleScene(m), m, patternEngine);

            MixerSnapshot newSnap;
            sceneManager.loadScene(sceneIndex, patternEngine, newSnap);
            applyMixerSnapshot(newSnap);
            for (int m = 0; m < kNumModules; ++m)
                sceneManager.setModuleScene(m, sceneIndex);
        }
        repaint();
    }

    // Clipboard
    std::optional<SynthPattern> clipboardSynth;
    std::optional<DrumPattern> clipboardDrum;

    void copyCellToClipboard(int sceneIndex, int moduleIndex)
    {
        const auto& scene = sceneManager.getScene(sceneIndex);
        if (moduleIndex == kLinnDrum)
            clipboardDrum = scene.drumPattern;
        else
            clipboardSynth = scene.synthPatterns[static_cast<size_t>(moduleIndex)];
    }

    void pasteCellFromClipboard(int sceneIndex, int moduleIndex)
    {
        auto& scene = sceneManager.getScene(sceneIndex);
        if (moduleIndex == kLinnDrum)
        {
            if (!clipboardDrum.has_value()) return;
            scene.drumPattern = *clipboardDrum;
        }
        else
        {
            if (!clipboardSynth.has_value()) return;
            scene.synthPatterns[static_cast<size_t>(moduleIndex)] = *clipboardSynth;
        }
        // If this cell is currently live, reload into pattern engine
        if (sceneManager.getModuleScene(moduleIndex) == sceneIndex)
            sceneManager.loadSceneForModule(sceneIndex, moduleIndex, patternEngine);
        repaint();
    }

    void clearScenePattern(int sceneIndex, int moduleIndex)
    {
        auto& scene = sceneManager.getScene(sceneIndex);
        if (moduleIndex == kLinnDrum)
            scene.drumPattern.clear();
        else
            scene.synthPatterns[static_cast<size_t>(moduleIndex)].clear();
        // If this cell is currently live, clear the live pattern too
        if (sceneManager.getModuleScene(moduleIndex) == sceneIndex)
            sceneManager.loadSceneForModule(sceneIndex, moduleIndex, patternEngine);
        repaint();
    }

    static const char* getModuleName(int m)
    {
        static const char* names[] = { "Jupiter-8", "Moog 15", "JX-3P", "DX7", "LinnDrum" };
        return (m >= 0 && m < kNumModules) ? names[m] : "?";
    }

    MixerSnapshot getMixerSnapshot() const
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
        return snap;
    }

    void applyMixerSnapshot(const MixerSnapshot& snap)
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
};

} // namespace axelf::ui
