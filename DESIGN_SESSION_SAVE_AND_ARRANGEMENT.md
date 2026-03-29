# Design: Session Save, Arrangement Rework, and LinnDrum Knobs

*Design session — March 22, 2026*

---

## Problem Summary

Three interconnected issues:

1. **No session persistence.** Patterns, scenes, transport BPM, bar count, chain — all lost on app close / DAW project reload. Only APVTS knob positions and MIDI learn survive.
2. **The arrangement/composition workflow is weak.** The current ArrangementView is a read-only 22–140px collapsible panel buried at the bottom. Scenes are snapshots with no visual connection to a timeline. There's no way to build a song from patterns without manually switching scenes during playback.
3. **LinnDrum knobs are 34×34px** — below the JUCE-recommended 40–48px minimum. Labels at 34px wide truncate. 60 tiny knobs in a grid is hard to use.

---

## 1. Session Save/Restore

### What Needs Saving (currently missing from state serialization)

| Data | Owner | Format |
|------|-------|--------|
| 4 × SynthPattern (vector of MidiEvents) | PatternEngine | JSON array |
| 1 × DrumPattern (15 tracks × 256 steps) | PatternEngine | JSON array |
| 16 × Scene slots (name + patterns + mixer snapshot) | SceneManager | JSON array |
| Transport BPM | GlobalTransport | float |
| Transport bar count | GlobalTransport | int |
| Per-module swing % | PatternEngine | float[5] |
| Per-module quantize grid | PatternEngine | int[5] |
| Chain definition | SceneManager | JSON array of {scene, repeats} |
| Active scene index | SceneManager | int |
| **Arrangement timeline** (new, see §2) | ArrangementEngine | JSON |

### Implementation

Extend `getStateInformation()` / `setStateInformation()` in PluginProcessor:

```xml
<AxelFState activeModule="0">
  <!-- Existing: APVTS per module, mixer, effects, MIDI learn -->
  <Jupiter8>...</Jupiter8>
  <Mixer>...</Mixer>
  <Effects>...</Effects>
  <MidiLearn>...</MidiLearn>

  <!-- NEW: Session data -->
  <Session>
    <Transport bpm="120" barCount="4" />
    <PatternEngine>
      <SynthPattern module="0">
        <Event note="60" vel="0.8" start="0.0" dur="0.5" />
        ...
      </SynthPattern>
      <!-- × 4 synth modules -->
      <DrumPattern>
        <Track index="0" steps="1001001000100100" velocities="0.8,0,0,0.7,..." />
        <!-- × 15 tracks -->
      </DrumPattern>
      <Swing values="50,50,50,50,55" />
      <Quantize values="3,3,3,3,3" />
    </PatternEngine>
    <SceneManager activeScene="0">
      <Scene index="0" name="Intro">
        <!-- Full pattern + mixer snapshot per scene slot -->
      </Scene>
      <!-- × 16 scenes -->
      <Chain entries="0:2,1:1,2:4" />
    </SceneManager>
    <Arrangement>
      <!-- NEW: timeline blocks (see §2) -->
    </Arrangement>
  </Session>
</AxelFState>
```

### Serialization Methods to Add

| Class | Method | Purpose |
|-------|--------|---------|
| `PatternEngine` | `toXml()` / `fromXml()` | Serialize all patterns, swing, quantize |
| `SceneManager` | `toXml()` / `fromXml()` | Serialize 16 scenes + chain + active index |
| `GlobalTransport` | `toXml()` / `fromXml()` | BPM, bar count |
| `PluginProcessor` | Extend existing methods | Call all the above, nest under `<Session>` |

### Standalone Session Files (.axf)

For standalone mode, add File → Save Session / Load Session:
- Uses the same XML format, written to a `.axf` file
- Recent sessions list in a "Session" menu or browser
- Auto-save on close (optional, user preference)

---

## 2. Arrangement & Composition — The Core Rethink

### The Problem with the Current Design

The current system has three separate concepts that don't connect well:

1. **Patterns** — per-module MIDI data (1–16 bars, looping)
2. **Scenes** — frozen snapshots of patterns + mixer (16 slots, A–P)
3. **Chain** — flat list of scene-index + repeat-count pairs

This gives you "scene A plays 2×, then scene B plays 1×" — but:
- No visual timeline
- No per-module independence (scene switch changes ALL modules at once)
- No way to see the big picture of a composition
- Chain is hidden behind a toggle with no visual feedback
- ArrangementView is read-only, tiny, and shows only the current playing pattern

### Design Goals

1. **Always visible** — the arrangement panel should be a constant reference point, not hidden
2. **Immediate** — drag, click, done. No modal dialogs or deep menus
3. **Flexible** — support both "jam mode" (live scene switching) and "song mode" (pre-arranged timeline)
4. **Per-module lanes** — different modules can play different scenes/patterns at different points
5. **Visual** — colour-coded blocks, playhead, loop markers, zoom

### The Proposed Solution: Top Timeline Panel

#### Layout Restructure

Move the arrangement from a collapsible bottom panel to a **permanent top panel** below the TopBar:

```
┌──────────────────────────────────────────────────────────────────┐
│ TOPBAR (52px) — logo, transport, BPM, preset, CPU               │
├──────────────────────────────────────────────────────────────────┤
│ TIMELINE PANEL (120-200px, resizable via drag handle)            │
│ ┌────────────────────────────────────────────────────────────┐   │
│ │ [JAM] [SONG] [LOOP]  Bars: 1  2  3  4  5  6  7  8 ...    │   │
│ │ ─────────────────────────────────────────── → scrollable   │   │
│ │ JUP8 ▐██ A ██▐██ A ██▐██ B ██▐██ B ██▐░░░░░░░░▐          │   │
│ │ MOOG ▐██ A ██▐██ A ██▐██ A ██▐██ C ██▐░░░░░░░░▐          │   │
│ │ JX3P ▐██ A ██▐██ B ██▐██ B ██▐██ B ██▐░░░░░░░░▐          │   │
│ │ DX7  ▐██ A ██▐██ A ██▐██ C ██▐██ C ██▐░░░░░░░░▐          │   │
│ │ LINN ▐██ A ██▐██ A ██▐██ B ██▐██ D ██▐░░░░░░░░▐          │   │
│ │                    ▲ playhead                               │   │
│ │ [SCENE: A B C D E F G H ...]  [SAVE] [+]                  │   │
│ └────────────────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│ MODULE TABS (remaining height)                                   │
│ ┌────────────────────────────────────────────────────────────┐   │
│ │ [Jupiter-8] [Moog 15] [JX-3P] [DX7] [LinnDrum]           │   │
│ │                                                            │   │
│ │            (synth editor fills this area)                   │   │
│ │                                                            │   │
│ └────────────────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│ MIXER STRIP (108px) — faders, pan, mute, solo                    │
├──────────────────────────────────────────────────────────────────┤
│ KEYBOARD (50px) — virtual piano                                  │
└──────────────────────────────────────────────────────────────────┘
```

#### Two Modes: JAM and SONG

**JAM Mode** (default — what exists today, but better):
- Timeline shows the **current looping pattern** as repeating blocks
- Scene buttons (A–P) along the bottom of the timeline panel — click to switch live
- Next-scene queuing: click a scene while playing → border flashes → switches at next loop point
- Per-module mute buttons in the lane headers for instant drop-in/drop-out
- Loop length shown visually (e.g. 4 bars highlighted, repeating)
- This is the "perform and experiment" mode — like Ableton Session View but linear

**SONG Mode** (the composition builder):
- Timeline becomes an **editable arrangement**
- Each lane (module) has independent blocks you can:
  - **Drag scenes onto lanes** from the scene palette at the bottom
  - **Resize blocks** by dragging their right edge (repeat a scene for N bars)
  - **Split/join blocks** (right-click context menu)
  - **Copy blocks** (alt+drag to duplicate)
  - **Delete blocks** (select → Delete key)
  - **Reorder** within a lane by dragging left/right
- Playback follows the timeline linearly (no looping unless loop markers set)
- Loop markers: drag-selectable range for looping a section during composition
- Empty lane sections = silence for that module (rest)
- This is the "build the song" mode — like Ableton Arrangement View

#### The Key Insight: Scenes Become Building Blocks

Instead of scenes being "full state snapshots that switch everything at once," scenes become **reusable pattern blocks** that you place on individual module lanes:

```
Scene A = { Jupiter lead pattern, Moog bass pattern, JX3P chords, DX7 bells, LinnDrum beat }
```

When you drag Scene A onto the Jupiter-8 lane, it places **only the Jupiter-8 pattern from Scene A**. When you drag Scene A onto the LinnDrum lane, it places the LinnDrum pattern. This means:

- Bar 1–4: Jupiter plays Scene A's lead, LinnDrum plays Scene B's beat → mix and match
- Full-scene placement also available: drag to a "master header" row → fills all 5 lanes

This turns 16 scene slots into a **pattern library** that feeds the arrangement.

#### Scene Palette (Bottom of Timeline Panel)

```
┌──────────────────────────────────────────────────────────────┐
│ SCENES: [A] [B] [C] [D] [E] [F] [G] [H] [→]  [SAVE] [+]  │
│                                                              │
│ In JAM mode: click to switch live (queued at loop boundary)  │
│ In SONG mode: drag onto lanes to place blocks                │
└──────────────────────────────────────────────────────────────┘
```

- **[SAVE]** — save current working patterns into the selected scene slot
- **[+]** — save current patterns to the next empty slot
- Hover tooltip shows scene name and which modules have data
- Right-click → Rename, Clear, Duplicate
- Active scene highlighted with gold border

#### Data Model: ArrangementTimeline

New class `ArrangementTimeline` in `Source/Common/`:

```cpp
struct TimelineBlock
{
    int sceneIndex;        // Which scene's pattern to use
    int startBar;          // Where this block starts (0-based)
    int lengthBars;        // How many bars this block spans
};

struct ModuleLane
{
    std::vector<TimelineBlock> blocks;  // Sorted by startBar, non-overlapping
};

class ArrangementTimeline
{
public:
    std::array<ModuleLane, 5> lanes;     // One per module

    int getTotalBars() const;            // Max end bar across all lanes
    void insertBlock(int module, TimelineBlock block);
    void removeBlock(int module, int blockIndex);
    void moveBlock(int module, int blockIndex, int newStartBar);
    void resizeBlock(int module, int blockIndex, int newLength);

    // For processBlock: "what scene pattern should module M play at bar B?"
    int getSceneForModuleAtBar(int module, int bar) const;  // -1 = silence

    juce::XmlElement* toXml() const;
    void fromXml(const juce::XmlElement* xml);
};
```

#### Playback Integration

In `PluginProcessor::processBlock()`, when SONG mode is active:
1. Check current bar position from transport
2. For each module, query `arrangement.getSceneForModuleAtBar(module, currentBar)`
3. If scene changed from last block → load that scene's pattern for that module from SceneManager
4. Pattern engine plays the loaded pattern
5. At end of arrangement → stop or loop (user preference)

In JAM mode: works as today — single pattern loops, scene switch replaces all patterns.

#### Timeline Panel Sizing

- **Default height: 160px** (enough for 5 lanes at 20px each + header + scene palette)
- **Min: 120px** (compressed lanes, palette collapses to icon row)
- **Max: 280px** (expanded lanes with waveform/event previews)
- **Resize handle:** drag the bottom edge of the timeline panel
- Always visible — no collapse toggle. This IS the composition hub

#### Height Budget at Default Size (1500×950)

```
TopBar:          52px
Timeline Panel: 160px  ← NEW (replaces PatternBankBar 28px + ArrangementView 22-140px)
Module Tabs:    ~370px  (was ~462px; loses ~90px but gains by removing arrangement bottom)
Mixer Strip:    108px
Keyboard:        50px
─────────────────────
Total:          ~740px + margins
```

Net change: currently PatternBankBar (28) + ArrangementView (22–140) = 50–168px.
New: 160px. At the expanded arrangement size, this is actually LESS space than before.
The key difference: it's at the **top** where it's prominent and always visible, not a forgotten bottom panel.

---

## 3. LinnDrum Knob Sizing

### Current Problem

| Dimension | Current | Recommended |
|-----------|---------|-------------|
| Knob diameter | 34px | 44px |
| Label width | 34px | 48px |
| Pad width | 170px | 210px |
| Pad height | 80px | 96px |
| Knob-to-knob spacing | 40px | 50px |

At 34px, rotary knobs are below JUCE's usability threshold. Labels truncate ("Decay" just fits, "Level" just fits, but drum names like "Tambourine" definitely don't).

### Fix

Increase to **44×44px knobs** with **48px horizontal spacing**:

```
New pad dimensions:
  4 knobs × 50px spacing = 200px + 10px padding = 210px wide
  44px knob + 12px label + 18px header = ~96px high (was 80)

5 columns × (210 + 6) = 1080px → fits comfortably in 1200px min width
3 rows × (96 + 4) = 300px drum grid height (was 252px, +48px)
```

Also:
- Increase knob label font from 9pt to 10pt
- Increase drum name font from 9pt bold to 10pt bold
- Add `setTextBoxIsEditable(true)` so users can type exact values
- Consider `setPopupDisplayEnabled(true, true, this)` for value popup on drag

### Implementation Changes

File: `LinnDrumEditor.h` — update constants:
```cpp
int knobW = 44, knobH = 44;   // was 34, 34
int padW = 210, padH = 96;    // was 170, 80
// Adjust spacing: p * 50 instead of p * 40
```

---

## 4. Implementation Phases

### Phase A: Session Save (can ship independently)
1. Add `toXml()`/`fromXml()` to PatternEngine
2. Add `toXml()`/`fromXml()` to SceneManager
3. Add `toXml()`/`fromXml()` to GlobalTransport
4. Extend PluginProcessor state serialization
5. Test: save DAW project → reload → all patterns/scenes preserved

### Phase B: LinnDrum Knob Resize (quick fix, ship immediately)
1. Update 4 constants in LinnDrumEditor.h
2. Adjust font sizes
3. Test at min window size (1200×700)

### Phase C: Arrangement Rework (larger effort)
1. Create `ArrangementTimeline` data model + serialization
2. Create `TimelinePanel` UI component (replaces ArrangementView + PatternBankBar)
3. Implement JAM mode (enhanced live scene switching with queuing)
4. Implement SONG mode (drag-and-drop block arrangement)
5. Wire playback: SONG mode reads timeline, feeds PatternEngine per bar
6. Restructure PluginEditor layout (timeline at top, remove old components)
7. Scene palette with drag source behavior
8. Undo/redo for arrangement edits

### Dependencies
- Phase A is independent — do first
- Phase B is independent — do anytime
- Phase C depends on Phase A (arrangement needs serialization infrastructure)

---

## 5. Design Decisions (Resolved)

1. **JAM → SONG capture: YES** — see §6
2. **Per-module independence in JAM mode: YES** — see §7
3. **Pattern length independence: YES** — see §8
4. **Maximum song length: 999 bars** — sufficient, matches existing spec
5. **Render to WAV: YES, future phase** — not in initial implementation, planned for Phase D

---

## 6. JAM → SONG Capture Workflow

### Concept

When the user is in JAM mode, jamming and switching scenes live, they can press a **[REC ARR]** (Record Arrangement) button. From that moment, every scene switch and its timing is logged. When they stop or press [REC ARR] again, the captured sequence is converted into SONG mode timeline blocks — a first draft of an arrangement they can then edit.

### UI

- **[REC ARR]** toggle button in the timeline panel header, next to [JAM] [SONG]
- When armed: button pulses red, "CAPTURING" label appears
- When stopped: switches to SONG mode automatically, timeline populated with captured blocks

### Capture Mechanics

```cpp
struct CaptureEvent
{
    int module;          // 0-4
    int sceneIndex;      // which scene was active
    double startBeat;    // absolute beat position when this scene became active
};

class ArrangementCapture
{
    bool capturing = false;
    std::vector<CaptureEvent> events;

public:
    void startCapture(double currentBeat);
    void recordSceneChange(int module, int sceneIndex, double beat);
    void stopCapture(double endBeat);
    ArrangementTimeline convertToTimeline(double beatsPerBar) const;
};
```

### Flow

1. User is in JAM mode, playing
2. Presses [REC ARR] → `capture.startCapture(currentBeat)`
3. Every scene switch (per-module or full-scene) calls `capture.recordSceneChange(...)`
4. User stops or presses [REC ARR] again → `capture.stopCapture(currentBeat)`
5. `convertToTimeline()` snaps events to bar boundaries, creates TimelineBlocks
6. SONG mode activated with the captured arrangement loaded
7. User can now edit: move blocks, extend, delete, fill gaps

### Bar-Snapping Rules

- Scene changes are snapped to the **nearest bar boundary** (since scenes queue to loop points anyway, this should be exact in most cases)
- If a scene was active for < 1 bar, it still gets 1 bar minimum
- Gaps between blocks = silence (empty lane section)

---

## 7. Per-Module Independence in JAM Mode

### Concept

Instead of scene switches replacing ALL five modules at once, the user can switch scenes independently per module. This enables "keep the drum beat from Scene A but switch the bass to Scene C" — essential for creative jamming.

### UI: Module Scene Selectors

In the timeline panel, each module lane header gets a **scene indicator**:

```
┌──────────────────────────────────────────────────────────────┐
│ [JAM] [SONG] [REC ARR]    1  2  3  4  5  6  7  8  ...      │
│ ───────────────────────────────────────────────────────      │
│ JUP8 [A▾] ▐██████████████████████████████████████████▐      │
│ MOOG [C▾] ▐██████████████████████████████████████████▐      │
│ JX3P [A▾] ▐██████████████████████████████████████████▐      │
│ DX7  [A▾] ▐██████████████████████████████████████████▐      │
│ LINN [B▾] ▐██████████████████████████████████████████▐      │
│                                                              │
│ SCENES: [A] [B] [C] [D] [E] [F] [G] [H] [→]  [SAVE] [+]  │
└──────────────────────────────────────────────────────────────┘
```

- **[A▾]** dropdown per lane: shows which scene's pattern this module is currently playing
- Click dropdown → pick a different scene → that module switches at next loop point
- **Scene palette buttons** (bottom) still work as "switch ALL modules" — a convenience shortcut
- Hold **Shift + click** a scene button → switch only the currently-selected module tab

### State Model

```cpp
// In PatternEngine or a new JamState class
struct JamModuleState
{
    int activeSceneIndex;     // Which scene's pattern this module is playing
    int queuedSceneIndex;     // -1 if no change queued
};

std::array<JamModuleState, 5> jamState;
```

### Queuing Behavior

- Scene changes queue to the next loop boundary (same as current full-scene switching)
- Visual feedback: queued scene letter flashes in the lane header dropdown
- When loop resets, queued scene loads for that module only
- Other modules unaffected

### Full-Scene vs Per-Module Switching

| Action | Effect |
|--------|--------|
| Click scene button in palette | Queue ALL 5 modules to that scene |
| Shift+click scene button | Queue only the active module tab to that scene |
| Click lane dropdown → pick scene | Queue only that specific module |
| Right-click scene button → "Apply to..." | Submenu to pick which modules |

---

## 8. Pattern Length Independence (Polymetric Blocks)

### Concept

In SONG mode, different modules can have blocks of different lengths. A 4-bar drum pattern can loop against an 8-bar chord progression. This enables polymetric compositions — essential for interesting arrangements.

### How It Works

Each `TimelineBlock` already has its own `lengthBars`. The key insight: when a block is shorter than adjacent blocks on other lanes, it **loops within its time slot** on the timeline.

Example — 8 bars of music:
```
Bar:  1     2     3     4     5     6     7     8
JUP8: ▐████ Scene A (8 bars) ████████████████████▐
MOOG: ▐██ Scene B (4 bars) ██▐██ Scene B (4b) ██▐   ← same block, loops
JX3P: ▐██ Scene A (4 bars) ██▐██ Scene C (4b) ██▐
DX7:  ▐████ Scene A (8 bars) ████████████████████▐
LINN: ▐ Sc.A 2b ▐ Sc.A 2b ▐ Sc.A 2b ▐ Sc.A 2b ▐   ← 2-bar loop repeats 4×
```

### Block Placement Rules

1. **Blocks have explicit start bar and length** — no implicit looping in the data model
2. **The user places blocks manually** — drag Scene B onto the Moog lane, it defaults to that scene's bar count
3. **Resize to extend** — drag the right edge of a block to repeat it for more bars
4. **Resize to shorten** — drag to truncate (pattern plays partial, then stops or loops within the block)
5. **Snap to bar boundaries** — blocks always align to bars (no sub-bar placement)
6. **No overlap within a lane** — blocks on the same lane cannot overlap; placing a new block pushes or replaces existing ones

### Looping Within a Block

When a block's `lengthBars` is greater than the source pattern's `barCount`:

```
Block: Scene A on Moog, lengthBars = 8
Scene A's Moog pattern: 4 bars

Playback: bars 1-4 play the pattern, bars 5-8 play it again from the start
```

This is the natural looping behavior — the pattern repeats to fill the block duration.

When a block's `lengthBars` is less than the source pattern's `barCount`:

```
Block: Scene A on Moog, lengthBars = 2
Scene A's Moog pattern: 4 bars

Playback: only bars 1-2 of the pattern play, then the next block starts
```

### Data Model (Updated)

```cpp
struct TimelineBlock
{
    int sceneIndex;        // Which scene's pattern to use
    int startBar;          // Where this block starts (0-based)
    int lengthBars;        // How many bars this block spans (can differ from scene's barCount)
};
```

No change to the struct — `lengthBars` already allows per-block sizing. The playback logic handles the loop/truncate math:

```cpp
// In processBlock, for each module:
int barWithinBlock = (currentBar - block.startBar) % sceneBarCount;
// Use barWithinBlock to index into the pattern data
```

### Visual Representation

- Blocks that loop show a subtle **repeat marker** (thin vertical dashed line) at each pattern boundary within the block
- Block label shows the loop count: "A ×2" for an 8-bar block of a 4-bar pattern
- Truncated blocks show a **fade-out gradient** at the right edge to indicate the pattern doesn't complete

### Per-Module Bar Count

Currently `GlobalTransport::barCount` is shared across all modules. For polymetric support:

- `barCount` remains the **default** pattern length for new recordings
- Each **scene slot** stores its own `barCount` per module (already implicitly true — SynthPattern length is independent)
- In SONG mode, block `lengthBars` overrides everything — the block is the authoritative duration
- In JAM mode, per-module patterns loop at their own natural length (the pattern's own bar count)

### JAM Mode Polymetric Behavior

When modules have different pattern lengths in JAM mode:

```
Jupiter: 8-bar pattern from Scene A → loops every 8 bars
LinnDrum: 2-bar pattern from Scene B → loops every 2 bars
```

The **master loop length** (for chain advance and scene queuing) is the **LCM** of active pattern lengths, capped at a reasonable maximum:

```cpp
int masterLoopBars = lcm(allActivePatternLengths);
if (masterLoopBars > 64) masterLoopBars = maxActivePatternLength; // fallback
```

Alternatively, simpler approach: **longest active pattern** determines the master loop. Scene queuing triggers when the longest pattern loops. Shorter patterns simply repeat within that period. This is simpler to understand and matches how hardware groove boxes work.

**Recommendation: Use "longest pattern = master loop" for simplicity.** The LCM approach is mathematically correct but confusing (a 3-bar and 4-bar pattern would create a 12-bar master loop, which may surprise the user).

---

## 9. Render to WAV (Phase D)

---

### 9.1 Entry Point — FILE Menu + Keyboard Shortcut

The existing `showSessionMenu()` in `PluginEditor.cpp` gets a new item:

```cpp
menu.addSeparator();
menu.addItem(5, "Render to File...   Ctrl+E");
```

The menu item is **always enabled**. If the user opens it in JAM mode or with an empty arrangement, the dialog itself shows a message ("Switch to SONG mode and add blocks to render"). This avoids the confusion of a grayed-out menu item with no explanation.

`Ctrl+E` keyboard shortcut wired in `keyPressed()` alongside the existing `Ctrl+S`, `Ctrl+O`, etc.

---

### 9.2 UI Component — RenderDialog

Follows the same overlay pattern as `AboutOverlay` and `PresetBrowser`: a `juce::Component` child of `PluginEditor`, full-bounds, hidden until shown.

```cpp
// In PluginEditor.h
#include "UI/RenderDialog.h"
ui::RenderDialog renderDialog;

// In constructor (after aboutOverlay setup):
renderDialog.setVisible(false);
addChildComponent(renderDialog);

// In resized():
renderDialog.setBounds(getLocalBounds());
```

---

### 9.3 Layout

Centered floating card over a semi-transparent backdrop. Follows the AboutOverlay visual language.

```
╔════════════════════════════════════════════════════╗
║  RENDER TO FILE                               [✕]  ║  ← gold border, 14px bold title
║  ──────────────────────────────────────────────── ║
║                                                    ║
║  OUTPUT                                            ║
║  [ Full Mix ]  [ Individual Tracks ]               ║  ← segmented toggle
║                                                    ║
║  FORMAT                   SAMPLE RATE              ║
║  [ WAV 24-bit        ▾ ] [ 48000 Hz          ▾ ]  ║
║                                                    ║
║  TAIL                     ☑ Normalize output       ║
║  [ +2 bars           ▾ ]                           ║
║                                                    ║
║  32 bars — 62.1 sec  →  7 files                    ║  ← read-only summary
║                                                    ║
║  OUTPUT PATH                                       ║
║  C:\Users\…\My Session.wav          [ Browse… ]    ║  ← path label + browse button
║                                                    ║
║  ──────────────────────────────────────────────── ║
║  [████████████████████████░░░░░░░░░] 64%     Abort ║  ← hidden until rendering
║                                                    ║
║                      [ Cancel ]  [ Render  ▶ ]     ║
╚════════════════════════════════════════════════════╝
```

**Card dimensions:** 520 × 420px, centered.

**Colors (matching AxelFColours.h):**
| Element | Colour |
|---|---|
| Backdrop | `0xCC000000` |
| Card background | `0xFF1a1a30` |
| Card border | `accentGold` (0xFFd4a843), 1.5px rounded rect r=8 |
| Title text | `accentGold`, 14px bold |
| Section labels ("OUTPUT", "FORMAT", etc.) | `textSecondary` (0xFF9898b0), 10px |
| Combo/toggle bg | `bgControl` (0xFF3a3a5c) |
| Active toggle fill | `accentBlue` (0xFF5b8bd4) |
| Summary text | `textPrimary` (0xFFe8e8f0), 12px |
| Path label | `textPrimary`, 11px, truncated left (shows `…\filename.wav`) |
| Progress bar fill | `accentGreen` (0xFF5bd47a) |
| Progress bar track | `bgSection` (0xFF2d2d4a) |
| Render button | `accentGreen` bg, textPrimary text, 13px bold |
| Cancel/Close button | `bgControl` bg, textSecondary text |
| Normalize checkbox | `textLabel` (0xFFb0b0cc), tick = `accentBlue` |
| Error text | `accentRed` (0xFFd45b5b), 11px |
| Plugin warning banner | `bgSection` bg, `accentOrange` left stripe, 10px text |

---

### 9.4 Controls Specification

#### Output Mode — "Full Mix" / "Individual Tracks"
Segmented toggle pair (two `TextButton`, radio group):
- **Full Mix** — single interleaved stereo WAV (full master bus output including FX)
- **Individual Tracks** — 7 files: 6 module tracks + a final mix

Label "Full Mix" / "Individual Tracks" chosen to be immediately clear to non-engineers. Default: Full Mix.

#### Format (ComboBox)
1. WAV 16-bit
2. **WAV 24-bit** ← default
3. WAV 32-bit float
4. AIFF 24-bit

#### Sample Rate (ComboBox)
1. 44100 Hz
2. **48000 Hz** ← default
3. 88200 Hz
4. 96000 Hz

Default auto-selects to match the current host sample rate at dialog open.

#### Tail (ComboBox)
Extra bars of silence after last arrangement block (for reverb/delay decay):
1. None (+0)
2. +1 bar
3. **+2 bars** ← default
4. +4 bars

#### Normalize checkbox
When checked, the rendered file(s) are peak-normalized to −0.1 dBFS after writing. Default: unchecked.

Implementation: two-pass isn't needed — measure the peak during the render loop, then apply a single gain pass on the completed file. Or simpler: scan peak after writing, then re-open and scale in a second pass via `AudioFormatReader` → gain → rewrite. For stems mode, each file is normalized independently.

#### Length summary (read-only label)
Shows: `"32 bars — 62.1 sec"` (or `"32 bars — 62.1 sec  →  7 files"` in Individual Tracks mode). Updates live when Tail dropdown changes.

#### Output path row
- **Path label** — shows the selected file path, truncated from the left (`…\folder\filename.wav`). Initially shows a suggested default: `<SessionsDir>/<SessionName>.wav`.
- **Browse…** button — opens a native `juce::FileChooser`. In "Full Mix" mode: file picker. In "Individual Tracks" mode: folder picker (files auto-named).

The path is visible and editable before the user clicks Render. This matches the standard DAW pattern where users verify the destination first.

#### Progress area (hidden until render starts)
- `juce::ProgressBar` with `accentGreen` fill
- Status label: "Rendering… 64%" or "Normalizing…" or "Done! → My Session.wav"
- "Abort" text button (replaces Cancel during render)

#### Buttons
- **Cancel** / **Close** (left): hides dialog in idle/done states; aborts in rendering state
- **Render ▶** (right, green): validates path → starts render thread. Disabled during render, becomes "Render Again" on completion.

---

### 9.5 State Machine

```
     IDLE ──── [Cancel / ✕ / click backdrop] ──── (hidden)
       │
       │ [Render ▶]
       │ ├─ Path empty? → show error "Choose an output path"
       │ ├─ No SONG content? → show error "Switch to SONG mode and add blocks"
       │ └─ Valid → RENDERING
       │
     RENDERING
       │  - Render button disabled ("Rendering…")
       │  - Progress bar visible, Cancel becomes "Abort"
       │  - OfflineRenderThread runs
       │
       ├─ [Abort] → thread.signalThreadShouldExit()
       │           → wait for exit → delete partial file(s)
       │           → back to IDLE
       │
       ├─ Normalize checked? → brief "Normalizing…" pass after render loop
       │
       ├─ thread error → show red error text, revert to IDLE
       │
       └─ thread success → DONE
                           label = "Done! → filename.wav"
                           Render becomes "Render Again", Cancel becomes "Close"
```

---

### 9.6 OfflineRenderThread

```cpp
class OfflineRenderThread : public juce::Thread
{
public:
    struct Options
    {
        bool         individualTracks;
        int          bitDepth;       // 16, 24, or 32
        bool         floatFormat;    // true for 32-bit float
        bool         useAiff;
        double       sampleRate;
        int          tailBars;
        bool         normalize;
        juce::File   outputFile;     // used for Full Mix
        juce::File   outputDir;      // used for Individual Tracks
        juce::String baseName;
    };

    OfflineRenderThread(AxelFProcessor& proc, const Options& opts);
    void run() override;

    std::atomic<double> progress { 0.0 };
    std::atomic<bool>   succeeded { false };
    juce::String        errorMessage;
};
```

**`run()` pseudocode:**

```
1. Snapshot state → MemoryBlock
2. suspendProcessing(true) to pause the audio thread
3. prepareToPlay(options.sampleRate, 512)
4. Reset transport to beat 0
5. Calculate totalSamples from arrangement length + tail
6. Open AudioFormatWriter(s) for output files
7. Loop in 512-sample blocks:
     a. Clear MIDI buffer
     b. processBlock(mixBuffer, midi)
     c. Write mixBuffer to mix writer
     d. If individualTracks: capture per-module buffers, write to stem writers
     e. Track peak sample (if normalize)
     f. progress = samplesWritten / totalSamples
     g. if threadShouldExit() → break
8. Close all writers
9. If normalize && peak > 0: apply gain (−0.1 dBFS / peak) to each file
10. Restore state from snapshot
11. suspendProcessing(false)
12. Set succeeded / errorMessage
13. MessageManager::callAsync → trigger UI refresh
```

`suspendProcessing(true)` tells JUCE to silence the audio callback, so the render thread has exclusive access to processBlock. This is safe in both standalone and plugin mode. In standalone the user hears silence during render; in plugin mode the DAW gets silence (equivalent to bypassing).

---

### 9.7 Track File Naming

When **Individual Tracks** is selected, the user picks an output folder. Files are auto-named:

| File | Content |
|---|---|
| `<BaseName>_Jupiter8.wav` | Jupiter-8 post-fader (level + pan applied, pre-send) |
| `<BaseName>_Moog.wav` | Moog post-fader |
| `<BaseName>_JX3P.wav` | JX-3P post-fader |
| `<BaseName>_DX7.wav` | DX7 post-fader |
| `<BaseName>_PPGWave.wav` | PPG Wave post-fader |
| `<BaseName>_LinnDrum.wav` | LinnDrum post-fader |
| `<BaseName>_Mix.wav` | Full master bus (with send FX + master bus processing) |

`BaseName` defaults to the session name (sanitized via `juce::File::createLegalFileName`).

Tracks are **post-fader, pre-send** (dry). The Mix file goes through the complete chain including reverb, delay, chorus, and master bus EQ/comp/limiter.

---

### 9.8 Plugin Mode Warning

When running as VST3/AU (not standalone), a one-line banner appears above the Format row:

```
⚠ Stop your DAW before rendering for best results.
```

`bgSection` background, `accentOrange` left stripe (3px), 10px text in `textSecondary`. No technical jargon. The Render button remains enabled.

---

### 9.9 Source Files

| File | Purpose |
|---|---|
| `Source/UI/RenderDialog.h` | Full dialog component (header-only, matches AboutOverlay pattern) |
| `Source/PluginEditor.h/.cpp` | `renderDialog` member, `showRenderDialog()`, FILE menu item 5, `Ctrl+E` shortcut |

No CMake changes needed — header-only UI components are not listed in `target_sources`.
