---
name: synth-click-prevention
description: >
  Use this skill whenever the user is building, debugging, or auditing a software
  synthesizer (or any real-time audio DSP system) and the topic involves clicks,
  pops, glitches, audio artifacts, transients, discontinuities, zipper noise,
  voice stealing, buffer boundaries, or parameter smoothing. Trigger even if the
  user just says "my synth clicks", "there's a pop on note-off", "zipper noise on
  the filter", or asks how to make audio transitions smooth. Apply this skill
  proactively whenever writing or reviewing synth DSP code — don't wait to be asked.
---

# Synth Click & Pop Prevention

A complete reference for eliminating clicks, pops, and discontinuity artifacts
in software synthesizer DSP. Apply these patterns whenever writing or reviewing
real-time audio code.

---

## Root Cause Taxonomy

Every click or pop is a **discontinuity** — a sudden jump in sample value or its
derivative. Identify the source first, then apply the matching fix.

| Source | Symptom | Fix |
|---|---|---|
| Note-on with phase reset | Click at note start | Phase-continuous oscillator or reset-to-zero |
| Note-off / voice cut | Click at note end | Release envelope, fade-out |
| Voice stealing | Random clicks mid-note | Fade-out before reassign |
| Abrupt parameter change | Zipper noise on knob/mod | One-pole smoother |
| Envelope segment junction | Subtle click at A→D or S→R | Continuous slope transitions |
| Waveform / wavetable switch | Click on timbre change | Crossfade |
| DC offset | Click when gate closes | DC blocker highpass |
| Buffer boundary event | Click on exact sample | Sub-block scheduling or interpolation |
| Granular grain edge | Click at grain start/end | Windowing function |
| Hard sync | Click on slave reset | Soft sync crossfade |

---

## Technique Reference

### 1. Parameter Smoothing (de-zipper filter)

Apply to **every** continuously-variable parameter: volume, cutoff, resonance,
pitch, pan, send levels, LFO depth, etc.

```cpp
// One-pole lowpass smoother
// tau_ms: smoothing time in milliseconds (try 5–20ms)
float coeff = expf(-1.0f / (sample_rate * tau_ms * 0.001f));

// Per-sample update (in the audio loop):
smoothed = coeff * smoothed + (1.0f - coeff) * target;
```

**Rule of thumb for coeff:**
- ~0.9995 ≈ 10ms at 44.1kHz (gentle, good for filter cutoff)
- ~0.999  ≈ 5ms  at 44.1kHz (snappy, good for volume)
- ~0.995  ≈ 1ms  at 44.1kHz (very fast, good for pitch)

Never apply a raw modulation value directly to a DSP parameter. Route all
mod sources through a smoother.

---

### 2. Amplitude Envelope Shaping

Even a **1ms linear attack** eliminates the majority of note-on clicks.

```cpp
// Minimum safe values (can be user-overridden, but clamp to these floors):
const float MIN_ATTACK_MS  = 1.0f;
const float MIN_RELEASE_MS = 5.0f;

// Envelope segment smoothing — avoid discontinuous slope changes.
// At segment boundaries, ramp the rate, not the value.
// Example: at attack→decay transition, the slope change should be gradual.
```

**Envelope curve shapes to prefer:**
- **Exponential** curves feel natural and have no sudden slope changes
- **Linear** curves are fine but can sound mechanical at junctions
- **Avoid** instantaneous level jumps anywhere in the envelope

---

### 3. Voice Stealing with Fade-Out

Never reassign a voice whose amplitude is non-zero without a fade.

```cpp
// When stealing a voice:
void stealVoice(Voice& v) {
    v.state = STEALING;
    v.steal_fade_samples = (int)(sample_rate * 0.010f); // 10ms
    v.steal_gain = v.current_gain; // capture current level
}

// In voice render loop, when state == STEALING:
float stealGain = (float)steal_samples_remaining / steal_fade_total;
output *= stealGain;
if (--steal_samples_remaining == 0) {
    reassignVoice(v, newNote);
}
```

**Zero-crossing detection** (optional enhancement): if the steal fade would be
very short (<2ms), wait up to one oscillator period for the waveform to cross
zero before stealing. Only practical at low frequencies.

---

### 4. DC Offset Removal

DC offset means the signal's mean ≠ 0. When the gate closes, the signal jumps
from `DC_offset` to `0` — a click.

```cpp
// One-pole DC blocker (apply per voice, before output)
// R ≈ 0.995 for ~44Hz highpass at 44.1kHz
float dcBlocker(float x, float& x_prev, float& y_prev, float R = 0.995f) {
    float y = x - x_prev + R * y_prev;
    x_prev = x;
    y_prev = y;
    return y;
}
```

Apply a DC blocker on the output of any oscillator that can produce DC
(e.g., pulse waves with fixed duty cycle, feedback FM, certain waveshapers).

---

### 5. Oscillator Phase Management

**Option A — Phase-continuous (no reset):**
The oscillator phase accumulates freely. No click on retrigger, but:
- Polyphonic voices may phase-cancel slightly
- Retriggered notes can feel "soft"

```cpp
// Just don't reset phase on note-on.
```

**Option B — Reset to zero:**
On note-on, snap phase to 0. Safe only if the oscillator output at phase=0 is
also 0 (i.e., a sine or saw starting at zero). For waveforms that are non-zero
at phase=0, apply a short (1–2ms) amplitude fade-in after reset.

**Option C — Phase snap to nearest zero-crossing:**
Find the next zero-crossing in the waveform at the current phase, advance to it,
then start. Complex to implement but avoids both reset artifacts and free-running
looseness.

---

### 6. Wavetable / Waveform Morphing

When switching between wavetables or waveform shapes, crossfade.

```cpp
// Simple linear crossfade over N samples
for (int i = 0; i < XFADE_SAMPLES; ++i) {
    float t = (float)i / XFADE_SAMPLES;
    output[i] = (1.0f - t) * oldWave[i] + t * newWave[i];
}
```

`XFADE_SAMPLES` of 128–512 (3–12ms at 44.1kHz) is usually inaudible and safe.

---

### 7. Buffer Boundary & Sample-Accurate Scheduling

When a note-on/off or parameter change lands **mid-buffer**, you have two choices:

**A — Sub-block splitting (sample-accurate, higher CPU):**
```cpp
// Split the buffer at the event point
renderBlock(buf, 0, eventOffset);
applyEvent(event);
renderBlock(buf, eventOffset, blockSize - eventOffset);
```

**B — Parameter interpolation (practical, lower CPU):**
Ramp the parameter from its old to new value across the entire buffer.
Effectively the same as the one-pole smoother but applied block-by-block.

Most commercial synths use approach B for everything except note timing, where
they use A.

---

### 8. Granular Synthesis — Windowing

Every grain **must** be windowed to zero at both endpoints.

```cpp
// Hann window
float hannWindow(int n, int N) {
    return 0.5f * (1.0f - cosf(2.0f * M_PI * n / (N - 1)));
}

// Apply during grain render:
for (int i = 0; i < grainSize; ++i) {
    output[i] = grainBuffer[i] * hannWindow(i, grainSize);
}
```

Other good choices: **Blackman** (lower sidelobes), **triangular/Bartlett**
(simple, cheaper). Never output a grain without a window.

---

### 9. Hard Sync — Soft Sync Option

Classic hard sync resets the slave oscillator's phase abruptly → click.

**Soft sync:** when the master triggers a slave reset, instead of jumping:
1. Fade out the slave over 1–4 samples
2. Reset phase
3. Fade back in

```cpp
// On sync trigger:
slave.syncFade = 4; // 4-sample crossfade
slave.phase = 0.0f;

// In slave render:
if (slave.syncFade > 0) {
    float t = 1.0f - (float)slave.syncFade / SYNC_FADE_LEN;
    sample *= t;
    --slave.syncFade;
}
```

---

## Code Review Checklist

Use this when auditing synth DSP code for click/pop sources:

- [ ] Every note-on has a minimum attack time (≥1ms) or fade-in
- [ ] Every note-off has a minimum release time (≥5ms) or fade-out
- [ ] Voice stealing applies a fade before reassignment
- [ ] All modulatable parameters go through a one-pole smoother
- [ ] Envelope segment transitions are slope-continuous
- [ ] Oscillator phase reset strategy is explicitly chosen and safe
- [ ] Wavetable/waveform changes are crossfaded
- [ ] DC blocker applied on oscillators that can produce DC
- [ ] Buffer-boundary events are split or interpolated
- [ ] Granular grains are windowed at both ends
- [ ] Hard sync uses soft-sync crossfade if clicks are audible

---

## Debugging Approach

When a user reports a click/pop without knowing the cause:

1. **Characterize the trigger**: Does it happen on note-on, note-off, voice
   steal, specific parameter move, or randomly?
2. **Isolate to a voice**: Reduce to one voice / one oscillator to rule out
   inter-voice interaction.
3. **Visualize**: Capture the audio, zoom in to the sample level in a DAW or
   scope. A discontinuity will be visible as a vertical spike or sudden slope
   change.
4. **Bisect the signal chain**: Mute/bypass modules (oscillator → filter →
   amp → FX) until the click disappears to identify the stage.
5. **Apply the matching fix** from the taxonomy table above.

---

## Language-Specific Notes

### C++
- Use `expf` / `cosf` not `exp` / `cos` to stay in float precision
- Avoid branching inside the audio loop for state transitions — use gain ramps instead
- JUCE's `SmoothedValue<float>` implements a one-pole smoother out of the box

### Rust (cpal / fundsp)
- `SmoothedValue` pattern: maintain `current` and `target`, lerp each sample
- Prefer atomic parameter passing with a per-block snapshot + ramp

### JavaScript / Web Audio API
- Use `AudioParam.linearRampToValueAtTime()` or `setTargetAtTime()` — never set `.value` directly on a changing param
- For custom processors in `AudioWorkletProcessor`, implement the one-pole smoother manually in `process()`

### Python (numpy / scipy — offline/prototype only)
- Clicks in offline rendering: apply `scipy.signal.lfilter` with a highpass for DC, and use `np.linspace` ramps
- Not suitable for real-time; use for prototyping only
