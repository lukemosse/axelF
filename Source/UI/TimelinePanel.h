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

        // ── Play / Stop button ───────────────────────────────
        playBtn.setButtonText(juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6")));
        playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::bgControl));
        playBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::accentGreen));
        playBtn.onClick = [this]
        {
            if (transport.isPlaying())
                transport.stop();
            else
                transport.play();
            repaint();
        };
        addAndMakeVisible(playBtn);

        // ── JAM / SONG toggle ────────────────────────────────
        jamSongBtn.setButtonText("JAM");
        jamSongBtn.setClickingTogglesState(true);
        jamSongBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(Colours::accentGreen).withAlpha(0.5f));
        jamSongBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Colours::accentBlue).withAlpha(0.5f));
        jamSongBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(Colours::textPrimary));
        jamSongBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(Colours::textPrimary));
        jamSongBtn.setToggleState(false, juce::dontSendNotification);  // starts in JAM
        jamSongBtn.onClick = [this]
        {
            bool toSong = jamSongBtn.getToggleState();
            if (toSong)
            {
                // Switching to SONG
                for (int m = 0; m < kNumModules; ++m)
                    sceneManager.saveModuleToScene(sceneManager.getModuleScene(m), m, patternEngine);
                arrangement.setMode(ArrangementMode::Song);
                jamSongBtn.setButtonText("SONG");
            }
            else
            {
                // Switching to JAM
                arrangement.setMode(ArrangementMode::Jam);
                for (int m = 0; m < kNumModules; ++m)
                    sceneManager.loadSceneForModule(sceneManager.getModuleScene(m), m, patternEngine);
                jamSongBtn.setButtonText("JAM");
            }
            resized();
            repaint();
        };
        addAndMakeVisible(jamSongBtn);

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
        clipMatrix = std::make_unique<ClipMatrixPanel>(sceneManager, patternEngine, transport, mixer);
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
                int bars = std::max(1, transport.getBarCount());
                for (int lane = 0; lane < kNumModules; ++lane)
                    arrangement.insertBlock(lane, { sceneIdx, bar, bars });
                // Switch to Song mode to show the result
                if (arrangement.getMode() != ArrangementMode::Song)
                {
                    for (int m = 0; m < kNumModules; ++m)
                        sceneManager.saveModuleToScene(sceneManager.getModuleScene(m), m, patternEngine);
                    arrangement.setMode(ArrangementMode::Song);
                    jamSongBtn.setToggleState(true, juce::dontSendNotification);
                    jamSongBtn.setButtonText("SONG");
                }
                resized();
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
                int bars = std::max(1, transport.getBarCount());
                arrangement.insertBlock(moduleIdx, { sceneIdx, bar, bars });
                // Switch to Song mode to show the result
                if (arrangement.getMode() != ArrangementMode::Song)
                {
                    for (int m = 0; m < kNumModules; ++m)
                        sceneManager.saveModuleToScene(sceneManager.getModuleScene(m), m, patternEngine);
                    arrangement.setMode(ArrangementMode::Song);
                    jamSongBtn.setToggleState(true, juce::dontSendNotification);
                    jamSongBtn.setButtonText("SONG");
                }
                resized();
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

        const int totalBars = std::max(arrangement.getTotalBars() + 4, transport.getBarCount());
        if (totalBars <= 0) return;

        const float timelineX = static_cast<float>(arrangeX + kLaneLabelWidth);
        const float arrangeWidth = bounds.getWidth() - static_cast<float>(arrangeX);
        const float timelineW = arrangeWidth - static_cast<float>(kLaneLabelWidth) - 4.0f;
        const float barWidth = static_cast<float>(kFixedBarWidth);
        const float scrollOff = static_cast<float>(songScrollbar.getCurrentRangeStart());

        for (int lane = 0; lane < kNumModules; ++lane)
        {
            const float y = static_cast<float>(laneAreaTop + lane * laneH);
            const auto laneCol = juce::Colour(laneColours[lane]);

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
                    float bx = timelineX + static_cast<float>(block.startBar) * barWidth - scrollOff;
                    float bw = static_cast<float>(block.lengthBars) * barWidth - 2.0f;
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
                    g.setColour(juce::Colour(Colours::textPrimary));
                    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
                    g.drawText(label, static_cast<int>(bx) + 3, static_cast<int>(blockY),
                               static_cast<int>(bw) - 4, static_cast<int>(blockH),
                               juce::Justification::centredLeft, false);
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

        // ── Playhead cursor ──────────────────────────────────
        if (transport.isPlaying())
        {
            double posBeats = transport.getPositionInBeats();
            double beatsPerBar = static_cast<double>(transport.getTimeSignatureNumerator());
            float barPos = static_cast<float>(posBeats / beatsPerBar);
            float px = timelineX + barPos * barWidth - scrollOff;

            if (px >= timelineX && px <= timelineX + timelineW)
            {
                g.setColour(juce::Colour(Colours::accentGold).withAlpha(0.9f));
                g.fillRect(px - 1.0f, static_cast<float>(kHeaderHeight),
                           2.0f, static_cast<float>(laneAreaBottom - kHeaderHeight));
            }
        }

        } // end of lane area scope

        // ── Vertical separator between matrix and arrange ────
        g.setColour(juce::Colour(Colours::borderSubtle));
        g.fillRect(static_cast<float>(arrangeAreaLeft) - 1.0f,
                   static_cast<float>(kHeaderHeight),
                   1.0f,
                   static_cast<float>(getHeight() - kHeaderHeight));


    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Header row: Play | JAM/SONG | Q: [...] | < >
        auto header = area.removeFromTop(kHeaderHeight).reduced(4, 2);
        playBtn.setBounds(header.removeFromLeft(28));
        header.removeFromLeft(2);
        jamSongBtn.setBounds(header.removeFromLeft(48));
        header.removeFromLeft(8);
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
        int clickedBar = static_cast<int>(clickXInContent / static_cast<float>(kFixedBarWidth));
        clickedBar = std::max(0, clickedBar);

        // Find block under cursor
        auto& blocks = arrangement.lanes[static_cast<size_t>(clickedLane)].blocks;
        int hitBlock = -1;
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
        {
            if (clickedBar >= blocks[static_cast<size_t>(i)].startBar &&
                clickedBar < blocks[static_cast<size_t>(i)].startBar + blocks[static_cast<size_t>(i)].lengthBars)
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
                            arrangement.removeBlock(lane, bi);
                        else if (result == 2)
                        {
                            auto& b = arrangement.lanes[static_cast<size_t>(lane)].blocks;
                            if (bi >= 0 && bi < static_cast<int>(b.size()))
                                clipboardBlock = b[static_cast<size_t>(bi)];
                        }
                        else if (result >= 100 && result < 100 + SceneManager::kMaxScenes)
                        {
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
                            TimelineBlock pasted = clipboardBlock.value();
                            pasted.startBar = bar;
                            arrangement.insertBlock(lane, pasted);
                        }
                        repaint();
                    });
            }
            return;
        }

        // Left-click on existing block → check for resize (right edge) or drag
        if (hitBlock >= 0)
        {
            const auto& blk = blocks[static_cast<size_t>(hitBlock)];
            float blockEndX = static_cast<float>(blk.startBar + blk.lengthBars)
                              * static_cast<float>(kFixedBarWidth);
            float distToRightEdge = blockEndX - clickXInContent;

            if (distToRightEdge >= 0.0f && distToRightEdge <= static_cast<float>(kResizeEdgePx))
            {
                // Near right edge → resize mode
                isResizing = true;
                dragLane = clickedLane;
                dragBlockIndex = hitBlock;
                return;
            }

            // Otherwise → drag mode
            isDragging = true;
            dragLane = clickedLane;
            dragBlockIndex = hitBlock;
            float blockStartX = static_cast<float>(blk.startBar)
                                * static_cast<float>(kFixedBarWidth);
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
            // Resize: compute new length from cursor position
            auto& blocks = arrangement.lanes[static_cast<size_t>(dragLane)].blocks;
            if (dragBlockIndex >= 0 && dragBlockIndex < static_cast<int>(blocks.size()))
            {
                int startBar = blocks[static_cast<size_t>(dragBlockIndex)].startBar;
                int endBar = static_cast<int>(clickXInContent / static_cast<float>(kFixedBarWidth) + 0.5f);
                int newLength = std::max(1, endBar - startBar);
                arrangement.resizeBlock(dragLane, dragBlockIndex, newLength);
                repaint();
            }
            return;
        }

        if (!isDragging) return;

        int newStart = static_cast<int>((clickXInContent - dragClickOffsetX)
                                        / static_cast<float>(kFixedBarWidth) + 0.5f);
        newStart = std::max(0, newStart);
        arrangement.moveBlock(dragLane, dragBlockIndex, newStart);
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
        for (const auto& blk : blocks)
        {
            float blockEndX = static_cast<float>(blk.startBar + blk.lengthBars)
                              * static_cast<float>(kFixedBarWidth);
            float dist = blockEndX - xInContent;
            if (dist >= 0.0f && dist <= static_cast<float>(kResizeEdgePx))
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
        if (selectedLane < 0 || selectedBlockIndex < 0) return false;

        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
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
                auto& blocks = arrangement.lanes[static_cast<size_t>(selectedLane)].blocks;
                TimelineBlock pasted = clipboardBlock.value();
                if (selectedBlockIndex >= 0 && selectedBlockIndex < static_cast<int>(blocks.size()))
                    pasted.startBar = blocks[static_cast<size_t>(selectedBlockIndex)].startBar
                                     + blocks[static_cast<size_t>(selectedBlockIndex)].lengthBars;
                else
                    pasted.startBar = 0;
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

    juce::TextButton playBtn, jamSongBtn;
    juce::TextButton prevPageBtn, nextPageBtn;

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
        // Sync JAM/SONG button state from arrangement
        bool isSong = arrangement.getMode() == ArrangementMode::Song;
        if (isSong != jamSongBtn.getToggleState())
        {
            jamSongBtn.setToggleState(isSong, juce::dontSendNotification);
            jamSongBtn.setButtonText(isSong ? "SONG" : "JAM");
            resized();
        }

        // Update play button text
        playBtn.setButtonText(transport.isPlaying()
            ? juce::String(juce::CharPointer_UTF8("\xe2\x96\xa0"))
            : juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6")));

        // Update scrollbar range
        {
            float timelineW = static_cast<float>(getWidth() - arrangeAreaLeft - kLaneLabelWidth - 4);
            int totalBars = std::max(arrangement.getTotalBars() + 4, transport.getBarCount());
            float contentW = static_cast<float>(totalBars * kFixedBarWidth);
            songScrollbar.setRangeLimits(0.0, static_cast<double>(contentW));
            songScrollbar.setCurrentRange(songScrollbar.getCurrentRangeStart(),
                                          static_cast<double>(timelineW));

            // Auto-scroll to keep playhead visible during playback
            if (transport.isPlaying())
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
