#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>
#include "../Common/SceneManager.h"
#include "../Common/PatternEngine.h"
#include "../Common/GlobalTransport.h"
#include "../Common/ArrangementTimeline.h"
#include "../Common/MasterMixer.h"
#include "AxelFColours.h"
#include "ClipMatrixPanel.h"

namespace axelf::ui
{

// ── Lozenge-style JAM / SONG switch ──────────────────
class JamSongSwitch : public juce::Component
{
public:
    std::function<void(bool /*isSong*/)> onChange;

    void setSong(bool isSong, bool notify = false)
    {
        if (songActive == isSong) return;
        songActive = isSong;
        repaint();
        if (notify && onChange) onChange(songActive);
    }
    bool isSong() const { return songActive; }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        float r = b.getHeight() * 0.5f;

        // Track background
        g.setColour(juce::Colour(Colours::bgDark));
        g.fillRoundedRectangle(b, r);
        g.setColour(juce::Colour(Colours::borderSubtle));
        g.drawRoundedRectangle(b, r, 1.0f);

        float half = b.getWidth() * 0.5f;

        // Active lozenge pill
        auto pill = songActive
            ? b.withLeft(b.getX() + half).withWidth(half)
            : b.withWidth(half);
        juce::Colour pillCol = songActive
            ? juce::Colour(Colours::accentBlue)
            : juce::Colour(Colours::accentGreen);
        g.setColour(pillCol.withAlpha(0.7f));
        g.fillRoundedRectangle(pill.reduced(1.5f), r - 1.5f);

        // Labels
        auto font = juce::Font(juce::FontOptions(11.0f, juce::Font::bold));
        g.setFont(font);
        auto leftRect  = b.withWidth(half);
        auto rightRect = b.withLeft(b.getX() + half).withWidth(half);

        g.setColour(!songActive ? juce::Colour(0xFFFFFFFF) : juce::Colour(Colours::textSecondary));
        g.drawText("JAM", leftRect, juce::Justification::centred, false);
        g.setColour(songActive ? juce::Colour(0xFFFFFFFF) : juce::Colour(Colours::textSecondary));
        g.drawText("SONG", rightRect, juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        bool clickedRight = e.x > getWidth() / 2;
        if (clickedRight != songActive)
        {
            songActive = clickedRight;
            repaint();
            if (onChange) onChange(songActive);
        }
    }

private:
    bool songActive = false;
};

// Helper button that supports double-click to rename a scene
class SceneButton : public juce::TextButton
{
public:
    int sceneIndex = 0;
    std::function<juce::String(int)> onGetName;
    std::function<void(int, const juce::String&)> onSetName;

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (!onGetName || !onSetName) return;
        auto* aw = new juce::AlertWindow ("Rename Scene",
            "Enter a name for scene " +
            juce::String::charToString (static_cast<juce::juce_wchar> ('A' + sceneIndex)),
            juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", onGetName (sceneIndex), "Name:");
        aw->addButton ("OK", 1);
        aw->addButton ("Cancel", 0);
        const int idx = sceneIndex;
        auto setNameCb = onSetName;
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [aw, idx, setNameCb] (int result)
            {
                if (result == 1)
                {
                    auto text = aw->getTextEditorContents ("name").trim();
                    if (text.isNotEmpty())
                        setNameCb (idx, text);
                }
                delete aw;
            }), false);
    }
};

class TimelinePanel : public juce::Component,
                      private juce::Timer
{
public:
    static constexpr int kDefaultHeight = 240;
    static constexpr int kMinHeight = 180;
    static constexpr int kMaxHeight = 350;
    static constexpr int kHeaderHeight = 24;
    static constexpr int kLaneHeight = 20;
    static constexpr int kLaneLabelWidth = 42;

    TimelinePanel(SceneManager& sm, PatternEngine& pe, GlobalTransport& tr,
                  ArrangementTimeline& arr, MasterMixer& mx)
        : sceneManager(sm), patternEngine(pe), transport(tr),
          arrangement(arr), mixer(mx)
    {
        setWantsKeyboardFocus(true);
        startTimerHz(30);

        // ── JAM / SONG lozenge switch ─────────────────────────
        jamSongSwitch.onChange = [this](bool toSong)
        {
            if (toSong)
            {
                // Switching to SONG
                for (int m = 0; m < kNumModules; ++m)
                    sceneManager.saveModuleToScene(sceneManager.getModuleScene(m), m, patternEngine);
                arrangement.setMode(ArrangementMode::Song);
            }
            else
            {
                // Switching to JAM
                arrangement.setMode(ArrangementMode::Jam);
                for (int m = 0; m < kNumModules; ++m)
                    sceneManager.loadSceneForModule(sceneManager.getModuleScene(m), m, patternEngine);
            }
            resized();
            repaint();
        };
        addAndMakeVisible(jamSongSwitch);

        // ── Launch quantize dropdown ─────────────────────────
        launchQBox.addItem("Immediate", 1);
        launchQBox.addItem("Next Beat", 2);
        launchQBox.addItem("Next Bar", 3);
        launchQBox.setSelectedId(3, juce::dontSendNotification); // Default: NextBar
        launchQBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(Colours::bgControl));
        launchQBox.setColour(juce::ComboBox::textColourId, juce::Colour(Colours::textPrimary));
        launchQBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(Colours::borderSubtle));
        launchQBox.setTooltip("Scene launch quantize: when queued scenes take effect");
        launchQBox.onChange = [this]
        {
            switch (launchQBox.getSelectedId())
            {
                case 1: sceneManager.setLaunchQuantize(LaunchQuantize::Immediate); break;
                case 2: sceneManager.setLaunchQuantize(LaunchQuantize::NextBeat); break;
                case 3: sceneManager.setLaunchQuantize(LaunchQuantize::NextBar); break;
                default: break;
            }
        };
        addAndMakeVisible(launchQBox);

        launchQLabel.setText("Q:", juce::dontSendNotification);
        launchQLabel.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        launchQLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textLabel));
        launchQLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(launchQLabel);

        // ── Clip matrix panel (initially hidden) ─────────────
        clipMatrix = std::make_unique<ClipMatrixPanel>(sceneManager, patternEngine, transport, mixer, arrangement);
        clipMatrix->setVisible(false);
        clipMatrix->sceneOffset = scenePage * kScenesPerPage;
        clipMatrix->numVisibleScenes = kScenesPerPage;
        clipMatrix->onSceneDropped = [this](int sceneIdx, juce::Point<int> screenPos)
        {
            auto localPos = getLocalPoint(nullptr, screenPos);
            if (localPos.x >= arrangeAreaLeft + kLaneLabelWidth)
            {
                float scrollOff = static_cast<float>(songScrollbar.getCurrentRangeStart());
                float contentX = static_cast<float>(localPos.x - arrangeAreaLeft - kLaneLabelWidth) + scrollOff;
                int bar = std::max(0, static_cast<int>(contentX / static_cast<float>(kFixedBarWidth)));
                int bpb = std::max(1, transport.getTimeSignatureNumerator());
                int bars = std::max(1, transport.getBarCount());
                arrangement.pushUndoState();
                for (int lane = 0; lane < kNumModules; ++lane)
                    arrangement.insertBlock(lane, { sceneIdx, bar * bpb, bars * bpb });
                repaint();
            }
        };
        clipMatrix->onModuleSelected = [this](int moduleIdx)
        {
            if (onModuleSelected)
                onModuleSelected(moduleIdx);
        };
        clipMatrix->onCellDropped = [this](int moduleIdx, int sceneIdx, juce::Point<int> screenPos)
        {
            auto localPos = getLocalPoint(nullptr, screenPos);
            if (localPos.x >= arrangeAreaLeft + kLaneLabelWidth)
            {
                float scrollOff = static_cast<float>(songScrollbar.getCurrentRangeStart());
                float contentX = static_cast<float>(localPos.x - arrangeAreaLeft - kLaneLabelWidth) + scrollOff;
                int bar = std::max(0, static_cast<int>(contentX / static_cast<float>(kFixedBarWidth)));
                int bpb = std::max(1, transport.getTimeSignatureNumerator());
                int bars = std::max(1, transport.getBarCount());
                arrangement.pushUndoState();
                arrangement.insertBlock(moduleIdx, { sceneIdx, bar * bpb, bars * bpb });
                repaint();
            }
        };
        addChildComponent(*clipMatrix);

        // ── Song mode horizontal scrollbar ───────────────────
        songScrollbar.setAutoHide(true);
        addAndMakeVisible(songScrollbar);

        // ── Scene page arrows (for clip matrix rows) ─────────
        prevPageBtn.setButtonText("<");
        prevPageBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        prevPageBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        prevPageBtn.onClick = [this] { setScenePage(scenePage - 1); };
        addAndMakeVisible(prevPageBtn);

        nextPageBtn.setButtonText(">");
        nextPageBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        nextPageBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textSecondary));
        nextPageBtn.onClick = [this] { setScenePage(scenePage + 1); };
        addAndMakeVisible(nextPageBtn);

        updateScenePageVisibility();
    }

    void setActiveModule(int idx) { activeModuleIndex = std::clamp(idx, 0, kNumModules - 1); }

    // Called when a matrix cell is clicked — parent hooks this to switch tabs
    std::function<void(int moduleIndex)> onModuleSelected;

    int getDesiredHeight() const { return currentHeight; }

    void setHeight(int h) { currentHeight = std::clamp(h, kMinHeight, kMaxHeight); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(Colours::bgDark));
        g.fillRect(bounds);

        // Bottom border
        g.setColour(juce::Colour(Colours::borderSubtle));
        g.fillRect(bounds.getX(), bounds.getBottom() - 1.0f, bounds.getWidth(), 1.0f);

        // ── Lane area (right half) ─────────────────────────────
        {
        const bool isJamMode = arrangement.getMode() == ArrangementMode::Jam;
        const int arrangeX = arrangeAreaLeft;
        const int laneAreaTop = kHeaderHeight;
        const int laneAreaBottom = getHeight()
                                   - (songScrollbar.isVisible() ? kScrollBarHeight : 0);
        const int laneAreaHeight = laneAreaBottom - laneAreaTop;
        if (laneAreaHeight < kNumModules * 10) return;

        const int laneH = laneAreaHeight / kNumModules;

        static constexpr juce::uint32 laneColours[kNumModules] = {
            Colours::jupiter, Colours::moog, Colours::jx3p, Colours::dx7, Colours::linn
        };
        static const char* laneNames[kNumModules] = {
            "JUP8", "MOOG", "JX3P", "DX7", "LINN"
        };

        const int beatsPerBar = std::max(1, transport.getTimeSignatureNumerator());
        const int totalBars = std::max(arrangement.getTotalBars(beatsPerBar) + 4, transport.getBarCount());
        if (totalBars <= 0) return;

        const float timelineX = static_cast<float>(arrangeX + kLaneLabelWidth);
        const float arrangeWidth = bounds.getWidth() - static_cast<float>(arrangeX);
        const float timelineW = arrangeWidth - static_cast<float>(kLaneLabelWidth) - 4.0f;
        const float barWidth = static_cast<float>(kFixedBarWidth);
        const float beatWidth = barWidth / static_cast<float>(beatsPerBar);
        const float scrollOff = static_cast<float>(songScrollbar.getCurrentRangeStart());

        for (int lane = 0; lane < kNumModules; ++lane)
        {
            const float y = static_cast<float>(laneAreaTop + lane * laneH);
            auto laneCol = juce::Colour(laneColours[lane]);
            if (isJamMode)
                laneCol = laneCol.withMultipliedSaturation(0.25f).withMultipliedBrightness(0.5f);

            // Lane background
            g.setColour(juce::Colour(lane % 2 == 0 ? Colours::bgDark : Colours::bgPanel).withAlpha(0.5f));
            g.fillRect(static_cast<float>(arrangeX), y, arrangeWidth, static_cast<float>(laneH));

            // Lane label
            g.setColour(juce::Colour(Colours::textSecondary));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(laneNames[lane], arrangeX + 2, static_cast<int>(y) + 1, kLaneLabelWidth - 4, laneH - 2,
                       juce::Justification::centredLeft, false);

            // Per-module scene indicator (JAM mode)
            if (isJamMode)
            {
                int moduleScene = sceneManager.getModuleScene(lane);
                int queued = sceneManager.getQueuedModuleScene(lane);
                juce::String sceneLabel = juce::String::charToString(
                    static_cast<juce::juce_wchar>('A' + moduleScene));
                if (queued >= 0)
                    sceneLabel += ">" + juce::String::charToString(
                        static_cast<juce::juce_wchar>('A' + queued));

                g.setColour(laneCol);
                g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
                g.drawText(sceneLabel, arrangeX + kLaneLabelWidth - 22, static_cast<int>(y) + 1, 20, laneH - 2,
                           juce::Justification::centredRight, false);
            }

            // ── Draw arrangement blocks (always visible) ─────
            {
                const auto& blocks = arrangement.lanes[static_cast<size_t>(lane)].blocks;
                for (size_t bi = 0; bi < blocks.size(); ++bi)
                {
                    const auto& block = blocks[bi];
                    float bx = timelineX + static_cast<float>(block.startBeat) * beatWidth - scrollOff;
                    float bw = static_cast<float>(block.lengthBeats) * beatWidth - 2.0f;
                    if (bx + bw < timelineX || bx > timelineX + timelineW) continue;
                    float blockY = y + 2.0f;
                    float blockH = static_cast<float>(laneH) - 4.0f;

                    bool isSelected = (lane == selectedLane && static_cast<int>(bi) == selectedBlockIndex);

                    g.setColour(laneCol.withAlpha(isSelected ? 0.85f : 0.6f));
                    g.fillRoundedRectangle(bx, blockY, bw, blockH, 3.0f);

                    if (isSelected)
                    {
                        g.setColour(juce::Colour(Colours::accentGold));
                        g.drawRoundedRectangle(bx, blockY, bw, blockH, 3.0f, 2.0f);
                    }
                    else
                    {
                        g.setColour(laneCol.withAlpha(0.9f));
                        g.drawRoundedRectangle(bx, blockY, bw, blockH, 3.0f, 1.0f);
                    }

                    // Block label
                    juce::String label = juce::String::charToString(
                        static_cast<juce::juce_wchar>('A' + block.sceneIndex));
                    g.setColour(isJamMode ? juce::Colour(Colours::textPrimary).withAlpha(0.35f)
                                         : juce::Colour(Colours::textPrimary));
                    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
                    float labelInset = (block.trimStartBeats > 0) ? 10.0f : 3.0f;
                    g.drawText(label, static_cast<int>(bx + labelInset), static_cast<int>(blockY),
                               static_cast<int>(bw - labelInset - 1.0f), static_cast<int>(blockH),
                               juce::Justification::centredLeft, false);

                    // Trim-start indicator: diagonal hash marks on the left edge
                    if (block.trimStartBeats > 0)
                    {
                        g.saveState();
                        g.reduceClipRegion(static_cast<int>(bx), static_cast<int>(blockY),
                                           static_cast<int>(bw), static_cast<int>(blockH));
                        g.setColour(laneCol.brighter(0.3f).withAlpha(0.7f));
                        float hashW = 7.0f;
                        float spacing = 4.0f;
                        for (float hy = blockY - hashW; hy < blockY + blockH + hashW; hy += spacing)
                        {
                            g.drawLine(bx, hy + hashW, bx + hashW, hy, 1.2f);
                        }
                        // Vertical accent line at the trim edge
                        g.setColour(laneCol.brighter(0.5f).withAlpha(0.9f));
                        g.drawLine(bx + 0.5f, blockY, bx + 0.5f, blockY + blockH, 1.5f);
                        g.restoreState();
                    }
                }
            }
        }

        // ── Bar number ruler ─────────────────────────────────
        g.setColour(juce::Colour(Colours::textSecondary).withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        for (int bar = 0; bar < totalBars; ++bar)
        {
            float bx = timelineX + static_cast<float>(bar) * barWidth - scrollOff;
            if (bx + barWidth < timelineX || bx > timelineX + timelineW) continue;
            g.drawText(juce::String(bar + 1), static_cast<int>(bx), 2,
                       static_cast<int>(barWidth), kHeaderHeight - 4,
                       juce::Justification::centredTop, false);
            // Grid line
            g.setColour(juce::Colour(Colours::borderSubtle).withAlpha(0.3f));
            g.fillRect(bx, static_cast<float>(kHeaderHeight),
                       1.0f, static_cast<float>(laneAreaBottom - kHeaderHeight));
            g.setColour(juce::Colour(Colours::textSecondary).withAlpha(0.5f));
        }

        // ── Playhead cursor (Song mode: always show position) ─────────
        if (!isJamMode)
        {
            double posBeats = transport.getPositionInBeats();
            float barPos = static_cast<float>(posBeats / static_cast<double>(beatsPerBar));
            float px = timelineX + barPos * barWidth - scrollOff;

            if (px >= timelineX && px <= timelineX + timelineW)
            {
                auto headCol = transport.isPlaying()
                    ? juce::Colour(Colours::accentGold).withAlpha(0.9f)
                    : juce::Colour(Colours::accentGold).withAlpha(0.5f);
                g.setColour(headCol);
                g.fillRect(px - 1.0f, 0.0f,
                           2.0f, static_cast<float>(laneAreaBottom));
                // Small triangle on ruler
                juce::Path tri;
                tri.addTriangle(px - 4.0f, 0.0f, px + 4.0f, 0.0f, px, 6.0f);
                g.fillPath(tri);
            }
        }

        // ── Pending cue indicator (red) ──────────────────
        if (!isJamMode && transport.hasCuePending())
        {
            double cueBeat = transport.getPendingCueBeat();
            float cueBarPos = static_cast<float>(cueBeat / static_cast<double>(beatsPerBar));
            float cx = timelineX + cueBarPos * barWidth - scrollOff;

            if (cx >= timelineX && cx <= timelineX + timelineW)
            {
                g.setColour(juce::Colour(Colours::accentRed).withAlpha(0.85f));
                g.fillRect(cx - 1.0f, 0.0f,
                           2.0f, static_cast<float>(laneAreaBottom));
                // Red triangle on ruler
                juce::Path tri;
                tri.addTriangle(cx - 5.0f, 0.0f, cx + 5.0f, 0.0f, cx, 8.0f);
                g.fillPath(tri);
            }
        }

        // ── Song markers ─────────────────────────────────
        if (!isJamMode)
        {
            const auto& markers = arrangement.getMarkers();
            g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            for (int mi = 0; mi < static_cast<int>(markers.size()); ++mi)
            {
                float mBarPos = static_cast<float>(static_cast<double>(markers[static_cast<size_t>(mi)].beat) / static_cast<double>(beatsPerBar));
                float mx = timelineX + mBarPos * barWidth - scrollOff;
                if (mx < timelineX || mx > timelineX + timelineW) continue;

                // Marker triangle (teal/cyan)
                g.setColour(juce::Colour(0xFF4ECDC4));
                juce::Path tri;
                tri.addTriangle(mx - 4.0f, static_cast<float>(kHeaderHeight),
                                mx + 4.0f, static_cast<float>(kHeaderHeight),
                                mx, static_cast<float>(kHeaderHeight) - 6.0f);
                g.fillPath(tri);

                // Marker label
                g.setColour(juce::Colour(0xFF4ECDC4).withAlpha(0.9f));
                g.drawText(markers[static_cast<size_t>(mi)].name,
                           static_cast<int>(mx + 2), kHeaderHeight - 12, 30, 10,
                           juce::Justification::centredLeft, false);

                // Thin vertical line through lanes
                g.setColour(juce::Colour(0xFF4ECDC4).withAlpha(0.3f));
                g.fillRect(mx - 0.5f, static_cast<float>(kHeaderHeight),
                           1.0f, static_cast<float>(laneAreaBottom - kHeaderHeight));
            }
        }

        } // end of lane area scope

        // ── Vertical divider between JAM and SONG panels ─────
        {
            float divX = static_cast<float>(arrangeAreaLeft);
            // Dark background stripe
            g.setColour(juce::Colour(0xFF0d0d1a));
            g.fillRect(divX - 2.0f, 0.0f, 4.0f, bounds.getHeight());
            // Bright center line
            g.setColour(juce::Colour(Colours::borderSubtle).brighter(0.3f));
            g.fillRect(divX - 0.5f, 0.0f, 1.0f, bounds.getHeight());
        }


    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Header row: Q: [...] | < >
        auto header = area.removeFromTop(kHeaderHeight).reduced(4, 2);
        launchQLabel.setBounds(header.removeFromLeft(16));
        header.removeFromLeft(2);
        launchQBox.setBounds(header.removeFromLeft(80));
        header.removeFromLeft(8);
        prevPageBtn.setBounds(header.removeFromLeft(18));
        header.removeFromLeft(2);
        nextPageBtn.setBounds(header.removeFromLeft(18));

        // Split remaining area: left = clip matrix, right = timeline lanes
        auto leftArea = area.removeFromLeft(area.getWidth() / 2);
        auto rightArea = area;

        arrangeAreaLeft = rightArea.getX();

        // Center JAM/SONG button on the divider line, in the header row
        {
            int btnW = 100;
            int btnH = kHeaderHeight - 4;
            jamSongSwitch.setBounds(arrangeAreaLeft - btnW / 2, 2, btnW, btnH);
        }

        // Scrollbar at bottom of right area (always visible)
        {
            auto scrollArea = rightArea.removeFromBottom(kScrollBarHeight);
            songScrollbar.setBounds(scrollArea.withLeft(arrangeAreaLeft + kLaneLabelWidth).withTrimmedRight(4));
            songScrollbar.setVisible(true);
        }

        // Clip matrix always visible on the left
        if (clipMatrix != nullptr)
        {
            clipMatrix->setBounds(leftArea);
            clipMatrix->setVisible(true);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Ignore clicks in the left (matrix) area — handled by clipMatrix child
        if (e.x < arrangeAreaLeft) return;

        // ── Ruler click: cue point / marker ──────────────
        if (e.y < kHeaderHeight && e.x >= arrangeAreaLeft + kLaneLabelWidth)
        {
            float scrollOff = static_cast<float>(songScrollbar.getCurrentRangeStart());
            float clickXInContent = static_cast<float>(e.x - arrangeAreaLeft - kLaneLabelWidth) + scrollOff;
            int bpb = std::max(1, transport.getTimeSignatureNumerator());
            float barWidth = static_cast<float>(kFixedBarWidth);
            float beatW = barWidth / static_cast<float>(bpb);
            int clickedBeat = std::max(0, static_cast<int>(clickXInContent / beatW));

            // Snap to bar
            int barBeat = (clickedBeat / bpb) * bpb;

            if (e.mods.isCtrlDown())
            {
                // Ctrl+click: add marker
                if (e.mods.isPopupMenu())
                {
                    // Ctrl+right-click: remove nearest marker
                    int bestIdx = -1;
                    int bestDist = 999999;
                    const auto& markers = arrangement.getMarkers();
                    for (int i = 0; i < static_cast<int>(markers.size()); ++i)
                    {
                        int dist = std::abs(markers[static_cast<size_t>(i)].beat - barBeat);
                        if (dist < bestDist) { bestDist = dist; bestIdx = i; }
                    }
                    if (bestIdx >= 0 && bestDist <= bpb)
                        arrangement.removeMarker(bestIdx);
                }
                else
                {
                    arrangement.addMarker(barBeat);
                }
                repaint();
                return;
            }

            // Regular click: set cue point
            if (transport.isPlaying())
            {
                // While playing: set pending cue (will jump at next bar)
                transport.setPendingCue(static_cast<double>(barBeat));
            }
            else
            {
                // While stopped: seek directly
                transport.seekToPosition(static_cast<double>(barBeat));
            }
            repaint();
            return;
        }

        int laneAreaTop = kHeaderHeight;
        int laneAreaBottom = getHeight()
                             - (songScrollbar.isVisible() ? kScrollBarHeight : 0);
        int laneAreaHeight = laneAreaBottom - laneAreaTop;
        if (laneAreaHeight <= 0) return;

        int laneH = laneAreaHeight / kNumModules;
        int clickY = e.y - laneAreaTop;
        int clickedLane = clickY / laneH;
        if (clickedLane < 0 || clickedLane >= kNumModules) return;

        bool isJam = arrangement.getMode() == ArrangementMode::Jam;

        // ── JAM mode: right-click lane label → "Fetch pattern from scene" ──
        if (isJam && e.mods.isPopupMenu() && e.x >= arrangeAreaLeft && e.x < arrangeAreaLeft + kLaneLabelWidth)
        {
            static const char* modNames[kNumModules] = {
                "Jupiter-8", "Moog 15", "JX-3P", "DX7", "LinnDrum"
            };
            juce::PopupMenu fetchMenu;
            for (int s = 0; s < SceneManager::kMaxScenes; ++s)
            {
                fetchMenu.addItem(200 + s,
                    juce::String("Scene ") +
                    juce::String::charToString(static_cast<juce::juce_wchar>('A' + s)));
            }
            juce::PopupMenu menu;
            menu.addSubMenu(juce::String("Fetch ") + modNames[clickedLane] + " from", fetchMenu);

            const int lane = clickedLane;
            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this, lane](int result)
                {
                    if (result >= 200 && result < 200 + SceneManager::kMaxScenes)
                    {
                        int srcScene = result - 200;
                        const auto& src = sceneManager.getScene(srcScene);
                        if (lane < 4)
                            patternEngine.getSynthPattern(lane) =
                                src.synthPatterns[static_cast<size_t>(lane)];
                        else
                            patternEngine.getDrumPattern() = src.drumPattern;
                    }
                    repaint();
                });
            return;
        }

        // ── Block interactions (both JAM and SONG) ─────────
        if (e.x < arrangeAreaLeft + kLaneLabelWidth) return;

        float scrollOff = static_cast<float>(songScrollbar.getCurrentRangeStart());
        float clickXInContent = static_cast<float>(e.x - arrangeAreaLeft - kLaneLabelWidth) + scrollOff;
        int bpb = std::max(1, transport.getTimeSignatureNumerator());
        float beatW = static_cast<float>(kFixedBarWidth) / static_cast<float>(bpb);
        int clickedBeat = static_cast<int>(clickXInContent / beatW);
        clickedBeat = std::max(0, clickedBeat);
        int clickedBar = clickedBeat / bpb;  // bar-snapped version for paste/insert

        // Find block under cursor
        auto& blocks = arrangement.lanes[static_cast<size_t>(clickedLane)].blocks;
        int hitBlock = -1;
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
        {
            if (clickedBeat >= blocks[static_cast<size_t>(i)].startBeat &&
                clickedBeat < blocks[static_cast<size_t>(i)].startBeat + blocks[static_cast<size_t>(i)].lengthBeats)
            {
                hitBlock = i;
                break;
            }
        }

        if (e.mods.isPopupMenu())
        {
            // Right-click: context menu
            if (hitBlock >= 0)
            {
                juce::PopupMenu menu;
                menu.addItem(1, "Delete Block");
                menu.addItem(2, "Copy Block");

                // Scene sub-menu
                juce::PopupMenu sceneMenu;
                for (int s = 0; s < SceneManager::kMaxScenes; ++s)
                    sceneMenu.addItem(100 + s, juce::String::charToString(
                        static_cast<juce::juce_wchar>('A' + s)));
                menu.addSubMenu("Change Scene", sceneMenu);

                const int lane = clickedLane;
                const int bi = hitBlock;
                menu.showMenuAsync(juce::PopupMenu::Options(),
                    [this, lane, bi](int result)
                    {
                        if (result == 1)
                        {
                            arrangement.pushUndoState();
                            arrangement.removeBlock(lane, bi);
                        }
                        else if (result == 2)
                        {
                            auto& b = arrangement.lanes[static_cast<size_t>(lane)].blocks;
                            if (bi >= 0 && bi < static_cast<int>(b.size()))
                                clipboardBlock = b[static_cast<size_t>(bi)];
                        }
                        else if (result >= 100 && result < 100 + SceneManager::kMaxScenes)
                        {
                            arrangement.pushUndoState();
                            auto& b = arrangement.lanes[static_cast<size_t>(lane)].blocks;
                            if (bi >= 0 && bi < static_cast<int>(b.size()))
                                b[static_cast<size_t>(bi)].sceneIndex = result - 100;
                        }
                        repaint();
                    });
            }
            else if (clipboardBlock.has_value())
            {
                // Right-click on empty space with clipboard → paste
                juce::PopupMenu menu;
                menu.addItem(3, "Paste Block");

                const int lane = clickedLane;
                const int bar = clickedBar;
                menu.showMenuAsync(juce::PopupMenu::Options(),
                    [this, lane, bar](int result)
                    {
                        if (result == 3 && clipboardBlock.has_value())
                        {
                            arrangement.pushUndoState();
                            TimelineBlock pasted = clipboardBlock.value();
                            pasted.startBeat = bar * transport.getTimeSignatureNumerator();
                            arrangement.insertBlock(lane, pasted);
                        }
                        repaint();
                    });
            }
            return;
        }

        // Left-click on existing block → check for resize edges or drag
        if (hitBlock >= 0)
        {
            const auto& blk = blocks[static_cast<size_t>(hitBlock)];
            float blockStartX = static_cast<float>(blk.startBeat) * beatW;
            float blockEndX = static_cast<float>(blk.startBeat + blk.lengthBeats) * beatW;
            float distToRightEdge = blockEndX - clickXInContent;
            float distToLeftEdge = clickXInContent - blockStartX;

            // Near right edge → resize-end mode
            if (distToRightEdge >= 0.0f && distToRightEdge <= static_cast<float>(kResizeEdgePx)
                && distToLeftEdge > static_cast<float>(kResizeEdgePx))
            {
                arrangement.pushUndoState();
                isResizing = true;
                dragLane = clickedLane;
                dragBlockIndex = hitBlock;
                return;
            }

            // Near left edge → trim-start mode
            if (distToLeftEdge >= 0.0f && distToLeftEdge <= static_cast<float>(kResizeEdgePx)
                && distToRightEdge > static_cast<float>(kResizeEdgePx))
            {
                arrangement.pushUndoState();
                isTrimmingStart = true;
                dragLane = clickedLane;
                dragBlockIndex = hitBlock;
                return;
            }

            // Otherwise → drag mode
            arrangement.pushUndoState();
            isDragging = true;
            dragLane = clickedLane;
            dragBlockIndex = hitBlock;
            dragClickOffsetX = clickXInContent - blockStartX;
            return;
        }

        // Left-click on empty space → deselect
        selectedLane = -1;
        selectedBlockIndex = -1;
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragLane < 0) return;

        float scrollOff = static_cast<float>(songScrollbar.getCurrentRangeStart());
        float clickXInContent = static_cast<float>(e.x - arrangeAreaLeft - kLaneLabelWidth) + scrollOff;

        if (isResizing)
        {
            // Resize from right edge: compute new length from cursor position
            auto& blocks = arrangement.lanes[static_cast<size_t>(dragLane)].blocks;
            if (dragBlockIndex >= 0 && dragBlockIndex < static_cast<int>(blocks.size()))
            {
                int bpb = std::max(1, transport.getTimeSignatureNumerator());
                float beatW = static_cast<float>(kFixedBarWidth) / static_cast<float>(bpb);
                int startBeat = blocks[static_cast<size_t>(dragBlockIndex)].startBeat;
                int endBeat;
                if (e.mods.isShiftDown())
                    endBeat = static_cast<int>(clickXInContent / beatW + 0.5f);
                else
                {
                    int endBar = static_cast<int>(clickXInContent / static_cast<float>(kFixedBarWidth) + 0.5f);
                    endBeat = endBar * bpb;
                }
                int newLength = std::max(1, endBeat - startBeat);
                arrangement.resizeBlock(dragLane, dragBlockIndex, newLength);
                repaint();
            }
            return;
        }

        if (isTrimmingStart)
        {
            // Trim from left edge: move start forward, shrink length, track trim offset
            auto& blocks = arrangement.lanes[static_cast<size_t>(dragLane)].blocks;
            if (dragBlockIndex >= 0 && dragBlockIndex < static_cast<int>(blocks.size()))
            {
                int bpb = std::max(1, transport.getTimeSignatureNumerator());
                float beatW = static_cast<float>(kFixedBarWidth) / static_cast<float>(bpb);
                int newStartBeat;
                if (e.mods.isShiftDown())
                    newStartBeat = static_cast<int>(clickXInContent / beatW + 0.5f);
                else
                {
                    int newStartBar = static_cast<int>(clickXInContent / static_cast<float>(kFixedBarWidth) + 0.5f);
                    newStartBeat = newStartBar * bpb;
                }
                arrangement.trimBlockStart(dragLane, dragBlockIndex, newStartBeat);
                repaint();
            }
            return;
        }

        if (!isDragging) return;

        int bpbM = std::max(1, transport.getTimeSignatureNumerator());
        float beatWM = static_cast<float>(kFixedBarWidth) / static_cast<float>(bpbM);
        int newStartBeat;
        if (e.mods.isShiftDown())
        {
            // Beat-level snap
            newStartBeat = static_cast<int>((clickXInContent - dragClickOffsetX) / beatWM + 0.5f);
        }
        else
        {
            // Bar-level snap (default)
            int newStartBar = static_cast<int>((clickXInContent - dragClickOffsetX)
                                               / static_cast<float>(kFixedBarWidth) + 0.5f);
            newStartBeat = newStartBar * bpbM;
        }
        newStartBeat = std::max(0, newStartBeat);
        arrangement.moveBlock(dragLane, dragBlockIndex, newStartBeat);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragLane >= 0 && dragBlockIndex >= 0)
        {
            selectedLane = dragLane;
            selectedBlockIndex = dragBlockIndex;
            grabKeyboardFocus();
        }
        isDragging = false;
        isResizing = false;
        isTrimmingStart = false;
        dragLane = -1;
        dragBlockIndex = -1;
        repaint();
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (e.x < arrangeAreaLeft + kLaneLabelWidth) { setMouseCursor(juce::MouseCursor::NormalCursor); return; }

        int laneAreaTop = kHeaderHeight;
        int laneAreaBottom = getHeight()
                             - (songScrollbar.isVisible() ? kScrollBarHeight : 0);
        int laneAreaHeight = laneAreaBottom - laneAreaTop;
        if (laneAreaHeight <= 0) { setMouseCursor(juce::MouseCursor::NormalCursor); return; }

        int laneH = laneAreaHeight / kNumModules;
        int clickedLane = (e.y - laneAreaTop) / laneH;
        if (clickedLane < 0 || clickedLane >= kNumModules)
        { setMouseCursor(juce::MouseCursor::NormalCursor); return; }

        float scrollOff = static_cast<float>(songScrollbar.getCurrentRangeStart());
        float xInContent = static_cast<float>(e.x - arrangeAreaLeft - kLaneLabelWidth) + scrollOff;

        const auto& blocks = arrangement.lanes[static_cast<size_t>(clickedLane)].blocks;
        int bpbC = std::max(1, transport.getTimeSignatureNumerator());
        float beatWC = static_cast<float>(kFixedBarWidth) / static_cast<float>(bpbC);
        for (const auto& blk : blocks)
        {
            float blockStartX = static_cast<float>(blk.startBeat) * beatWC;
            float blockEndX = static_cast<float>(blk.startBeat + blk.lengthBeats) * beatWC;
            float distRight = blockEndX - xInContent;
            float distLeft = xInContent - blockStartX;
            // Right edge resize cursor
            if (distRight >= 0.0f && distRight <= static_cast<float>(kResizeEdgePx)
                && distLeft > static_cast<float>(kResizeEdgePx))
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
            // Left edge trim cursor
            if (distLeft >= 0.0f && distLeft <= static_cast<float>(kResizeEdgePx)
                && distRight > static_cast<float>(kResizeEdgePx))
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
        }
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (!songScrollbar.isVisible()) return;

        // Horizontal scroll with mouse wheel (Shift+wheel or trackpad horizontal)
        float delta = (wheel.deltaX != 0.0f) ? wheel.deltaX : -wheel.deltaY;
        songScrollbar.setCurrentRangeStart(
            songScrollbar.getCurrentRangeStart() - static_cast<double>(delta * 80.0f));
        repaint();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        // Undo / Redo (always available, no selection needed)
        if (key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
        {
            if (arrangement.undo())
            {
                selectedLane = -1;
                selectedBlockIndex = -1;
                repaint();
            }
            return true;
        }
        if (key == juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0))
        {
            if (arrangement.redo())
            {
                selectedLane = -1;
                selectedBlockIndex = -1;
                repaint();
            }
            return true;
        }

        if (selectedLane < 0 || selectedBlockIndex < 0) return false;

        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            arrangement.pushUndoState();
            arrangement.removeBlock(selectedLane, selectedBlockIndex);
            selectedLane = -1;
            selectedBlockIndex = -1;
            repaint();
            return true;
        }

        if (key == juce::KeyPress('c', juce::ModifierKeys::ctrlModifier, 0))
        {
            auto& blocks = arrangement.lanes[static_cast<size_t>(selectedLane)].blocks;
            if (selectedBlockIndex >= 0 && selectedBlockIndex < static_cast<int>(blocks.size()))
                clipboardBlock = blocks[static_cast<size_t>(selectedBlockIndex)];
            return true;
        }

        if (key == juce::KeyPress('v', juce::ModifierKeys::ctrlModifier, 0))
        {
            if (clipboardBlock.has_value())
            {
                arrangement.pushUndoState();
                auto& blocks = arrangement.lanes[static_cast<size_t>(selectedLane)].blocks;
                TimelineBlock pasted = clipboardBlock.value();
                if (selectedBlockIndex >= 0 && selectedBlockIndex < static_cast<int>(blocks.size()))
                    pasted.startBeat = blocks[static_cast<size_t>(selectedBlockIndex)].startBeat
                                     + blocks[static_cast<size_t>(selectedBlockIndex)].lengthBeats;
                else
                    pasted.startBeat = 0;
                arrangement.insertBlock(selectedLane, pasted);
                repaint();
            }
            return true;
        }

        return false;
    }

private:
    static constexpr int kNumLanes = kNumModules;
    static constexpr int kScenesPerPage = 8;

    SceneManager& sceneManager;
    PatternEngine& patternEngine;
    GlobalTransport& transport;
    ArrangementTimeline& arrangement;
    MasterMixer& mixer;

    juce::TextButton prevPageBtn, nextPageBtn;

    // JAM / SONG lozenge switch
    JamSongSwitch jamSongSwitch;

    // Launch quantize
    juce::ComboBox launchQBox;
    juce::Label launchQLabel;

    // Clip matrix
    std::unique_ptr<ClipMatrixPanel> clipMatrix;
    int arrangeAreaLeft = 0;

    int activeModuleIndex = 0;
    int scenePage = 0;
    int currentHeight = 240;  // Matrix default

    // Song mode scrollbar & paint-scene selector
    juce::ScrollBar songScrollbar { false }; // horizontal
    static constexpr int kFixedBarWidth = 50;
    static constexpr int kScrollBarHeight = 12;
    int selectedPaintScene = 0;

    // Drag state for SONG mode blocks
    bool isDragging = false;
    bool isResizing = false;
    bool isTrimmingStart = false;
    int dragLane = -1;
    int dragBlockIndex = -1;
    float dragClickOffsetX = 0.0f;

    // Selection state for SONG mode blocks
    int selectedLane = -1;
    int selectedBlockIndex = -1;

    // Clipboard for copy/paste
    std::optional<TimelineBlock> clipboardBlock;
    static constexpr int kResizeEdgePx = 6;

    void setScenePage(int newPage)
    {
        scenePage = std::clamp(newPage, 0, (SceneManager::kMaxScenes - 1) / kScenesPerPage);
        updateScenePageVisibility();
        if (clipMatrix)
            clipMatrix->setSceneOffset(scenePage * kScenesPerPage);
        resized();
    }

    void updateScenePageVisibility()
    {
        prevPageBtn.setEnabled(scenePage > 0);
        nextPageBtn.setEnabled(scenePage < (SceneManager::kMaxScenes - 1) / kScenesPerPage);
    }

    MixerSnapshot getMixerSnapshot()
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

    void saveCurrentScene()
    {
        for (int m = 0; m < kNumModules; ++m)
            sceneManager.saveModuleToScene(sceneManager.getModuleScene(m), m, patternEngine);
    }

    void saveToNextEmpty()
    {
        // Find first scene with empty name or "Scene X" default and save there
        for (int i = 0; i < SceneManager::kMaxScenes; ++i)
        {
            auto name = sceneManager.getSceneName(i);
            if (name.startsWith("Scene "))
            {
                auto snap = getMixerSnapshot();
                sceneManager.saveToScene(i, patternEngine, snap);
                // Also sync per-module scenes to this slot
                for (int m = 0; m < kNumModules; ++m)
                    sceneManager.setModuleScene(m, i);
                break;
            }
        }
    }

    void timerCallback() override
    {
        // Sync JAM/SONG switch state from arrangement
        bool isSong = arrangement.getMode() == ArrangementMode::Song;
        if (isSong != jamSongSwitch.isSong())
        {
            jamSongSwitch.setSong(isSong);
            resized();
        }

        // Update scrollbar range
        {
            float timelineW = static_cast<float>(getWidth() - arrangeAreaLeft - kLaneLabelWidth - 4);
            int bpbScroll = std::max(1, transport.getTimeSignatureNumerator());
            int totalBars = std::max(arrangement.getTotalBars(bpbScroll) + 4, transport.getBarCount());
            float contentW = static_cast<float>(totalBars * kFixedBarWidth);
            songScrollbar.setRangeLimits(0.0, static_cast<double>(contentW));
            songScrollbar.setCurrentRange(songScrollbar.getCurrentRangeStart(),
                                          static_cast<double>(timelineW));

            // Auto-scroll to keep playhead visible during Song mode playback
            if (transport.isPlaying() && arrangement.getMode() == ArrangementMode::Song)
            {
                double posBeats = transport.getPositionInBeats();
                double beatsPerBar = static_cast<double>(transport.getTimeSignatureNumerator());
                float playheadX = static_cast<float>(posBeats / beatsPerBar) * static_cast<float>(kFixedBarWidth);
                float viewStart = static_cast<float>(songScrollbar.getCurrentRangeStart());
                float viewEnd = viewStart + timelineW;
                if (playheadX < viewStart || playheadX > viewEnd - 30.0f)
                    songScrollbar.setCurrentRangeStart(static_cast<double>(playheadX - timelineW * 0.25f));
            }
        }

        repaint();
    }
};

} // namespace axelf::ui
