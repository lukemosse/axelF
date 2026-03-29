#include "Moog15Voice.h"
#include <cmath>
#include <algorithm>

namespace axelf::moog15
{

// ── Soft saturation (tube-like) ─────────────────────────────────────────────
static inline float softClip(float x)
{
    // Cubic soft clip per synth-high-quality skill
    x = std::clamp(x, -1.5f, 1.5f);
    return x - (x * x * x) / 3.0f;
}

// ── Fast 2^x (bit-manipulation trick from synth-high-quality skill) ─────────
inline float Moog15Voice::fastPow2(float x)
{
    // Accurate to ~0.1% — good enough for per-sample pitch/filter
    x = std::clamp(x, -10.0f, 10.0f);
    union { float f; int32_t i; } u;
    u.i = static_cast<int32_t>(x * 8388608.0f) + 0x3f800000;
    return u.f;
}

// ── DC blocker (R ≈ 0.995 → ~44Hz HPF at 44.1kHz) ─────────────────────────
inline float Moog15Voice::dcBlock(float x)
{
    constexpr float R = 0.995f;
    float y = x - dcBlockX1 + R * dcBlockY1;
    dcBlockX1 = x;
    dcBlockY1 = y;
    return y;
}

// ── LFO (simple built-in, separate from Osc 3 as LFO) ──────────────────────
float Moog15Voice::getLfoSample()
{
    float out = 0.0f;
    switch (lfoWaveform)
    {
        case 0: out = std::sin(lfoPhase * 6.283185307f); break;          // Sine
        case 1: out = 2.0f * std::abs(2.0f * lfoPhase - 1.0f) - 1.0f; break; // Triangle
        case 2: out = 2.0f * lfoPhase - 1.0f; break;                    // Saw
        case 3: out = lfoPhase < 0.5f ? 1.0f : -1.0f; break;           // Square
        case 4: // S&H — re-evaluate every cycle
        {
            if (lfoPhase + lfoPhaseInc >= 1.0f || lfoPhase < lfoPhaseInc)
            {
                noiseState ^= noiseState << 13;
                noiseState ^= noiseState >> 17;
                noiseState ^= noiseState << 5;
                shValue = static_cast<float>(static_cast<int32_t>(noiseState))
                        / static_cast<float>(INT32_MAX);
            }
            out = shValue;
            break;
        }
    }
    lfoPhase += lfoPhaseInc;
    if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    return out;
}

// ── Noise generators ────────────────────────────────────────────────────────
float Moog15Voice::getWhiteNoise()
{
    noiseState ^= noiseState << 13;
    noiseState ^= noiseState >> 17;
    noiseState ^= noiseState << 5;
    return static_cast<float>(static_cast<int32_t>(noiseState))
         / static_cast<float>(INT32_MAX);
}

float Moog15Voice::getPinkNoise()
{
    // Paul Kellet pink filter (3-stage)
    float white = getWhiteNoise();
    pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
    pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
    pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
    return (pinkState[0] + pinkState[1] + pinkState[2] + white * 0.5362f) * 0.25f;
}

// ── Voice lifecycle ─────────────────────────────────────────────────────────

bool Moog15Voice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<Moog15Sound*>(sound) != nullptr;
}

void Moog15Voice::startNote(int midiNoteNumber, float vel,
                             juce::SynthesiserSound*, int pitchWheel)
{
    velocity = vel;
    midiNoteNum = midiNoteNumber;

    float freq = static_cast<float>(
        juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

    glideTarget = freq;
    pitchBendSemitones = ((static_cast<float>(pitchWheel) - 8192.0f) / 8192.0f) * 2.0f;

    // If no glide or first note, jump to target
    if (glideTime <= 0.001f || !loudnessContour.isActive())
        currentFreq = freq;

    // Set sample rates on all DSP components
    const double sr = getSampleRate();
    vco1.setSampleRate(sr);
    vco2.setSampleRate(sr);
    vco3.setSampleRate(sr);
    vcf.setSampleRate(sr);
    filterContour.setSampleRate(sr);
    loudnessContour.setSampleRate(sr);

    // Trigger contour generators — retrigger from current level, NEVER reset.
    // This is the core click-prevention design: no amplitude discontinuity.
    filterContour.noteOn();
    loudnessContour.noteOn();

    // DO NOT reset filter (MoogLadder has no reset — continuous analog behavior)
    // DO NOT reset oscillators (free-running — no phase reset ever)
    // DO NOT use anti-click counter (contour attack IS the anti-click)

    // Initialize smoothers
    cutoffSmoothed.reset(sr, 0.02);
    resoSmoothed.reset(sr, 0.02);
    driveSmoothed.reset(sr, 0.02);
    mixVco1Smoothed.reset(sr, 0.005);
    mixVco2Smoothed.reset(sr, 0.005);
    mixVco3Smoothed.reset(sr, 0.005);
    noiseLevelSmoothed.reset(sr, 0.005);
    feedbackSmoothed.reset(sr, 0.01);
    envDepthSmoothed.reset(sr, 0.02);
    masterVolSmoothed.reset(sr, 0.01);
}

void Moog15Voice::stopNote(float /*vel*/, bool allowTailOff)
{
    // Enter release phase — contour decays toward 0 using decayRate
    // (authentic Model D: decay rate IS the release rate)
    filterContour.noteOff();
    loudnessContour.noteOff();

    if (!allowTailOff)
    {
        // Voice steal: don't zero anything — next startNote() retriggers
        // from current contour level (click-free voice stealing)
        clearCurrentNote();
    }
}

void Moog15Voice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchBendSemitones = ((static_cast<float>(newPitchWheelValue) - 8192.0f) / 8192.0f) * 2.0f;
}

void Moog15Voice::controllerMoved(int controllerNumber, int newControllerValue)
{
    if (controllerNumber == 1)
        modWheelValue = static_cast<float>(newControllerValue) / 127.0f;
}

// ── Render ──────────────────────────────────────────────────────────────────

void Moog15Voice::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                   int startSample, int numSamples)
{
    const float sr = static_cast<float>(getSampleRate());

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // 1. Advance contour generators
        float env1Val = filterContour.getNextSample();
        float env2Val = loudnessContour.getNextSample();

        // 2. If loudness contour is idle → voice is done
        if (!loudnessContour.isActive())
        {
            clearCurrentNote();
            break;
        }

        // 3. Micro-glide: always-on 1ms minimum (per spec + click-prevention skill)
        //    Prevents pitch discontinuity under non-zero VCA amplitude
        float effectiveGlide = std::max(glideTime, 0.001f);
        float glideCoeff = 1.0f / (effectiveGlide * sr);
        currentFreq += (glideTarget - currentFreq) * glideCoeff;

        // 4. LFO
        float lfoVal = getLfoSample();
        float modPitchLfo = lfoPitchAmount * lfoVal * modWheelValue;
        float modFilterLfo = vcfLfoAmount * lfoVal * modWheelValue;

        // 5. Osc 3 as modulation source (when vco3Ctrl == 1)
        float osc3ModValue = 0.0f;
        if (vco3CtrlMode == 1)
        {
            osc3ModValue = vco3.getCurrentOutput();
        }

        // 6. Pitch calculation with exponential scaling (1V/oct)
        float bendMul = fastPow2(pitchBendSemitones / 12.0f);
        float pitchMod = fastPow2(modPitchLfo * 0.16667f); // ±2 semitones at full

        // Osc 3 pitch modulation when in LFO mode
        if (vco3CtrlMode == 1)
        {
            pitchMod *= fastPow2(osc3ModValue * lfoPitchAmount * 0.16667f);
        }

        float detune2 = fastPow2(vco2DetuneCents / 1200.0f);
        float detune3 = fastPow2(vco3DetuneCents / 1200.0f);

        float basePitch = currentFreq * bendMul * pitchMod;

        // VCO range multipliers
        float r1 = (vco1RangeIdx < 5) ? kRangeMultipliers[vco1RangeIdx] : 1.0f;
        float r2 = (vco2RangeIdx < 5) ? kRangeMultipliers[vco2RangeIdx] : 1.0f;
        float r3 = 1.0f;
        if (vco3CtrlMode == 0 && vco3RangeIdx < 5)
            r3 = kRangeMultipliers[vco3RangeIdx];

        vco1.setFrequency(basePitch * r1);
        vco2.setFrequency(basePitch * r2 * detune2);

        if (vco3CtrlMode == 0)
        {
            // Normal keyboard-tracking mode
            vco3.setFrequency(basePitch * r3 * detune3);
        }
        // else: Osc 3 in LFO mode — frequency set by setParameters (free-running at LFO rate)

        // 7. Generate oscillator samples
        float osc1 = vco1.getNextSample();
        float osc2 = vco2.getNextSample();
        float osc3 = vco3.getNextSample();

        // 8. Mixer: sum VCOs + noise + feedback
        float mix = osc1 * mixVco1Smoothed.getNextValue()
                  + osc2 * mixVco2Smoothed.getNextValue();

        // Osc 3 goes to mixer only when NOT in LFO mode
        if (vco3CtrlMode == 0)
            mix += osc3 * mixVco3Smoothed.getNextValue();
        else
            mixVco3Smoothed.getNextValue(); // skip to keep smoother advancing

        // Noise
        float noiseVal = (noiseColorParam == 0) ? getWhiteNoise() : getPinkNoise();
        mix += noiseVal * noiseLevelSmoothed.getNextValue();

        // Feedback (output → mixer, classic Minimoog growl)
        mix += feedbackSample * feedbackSmoothed.getNextValue();

        // 9. Filter — exponential cutoff modulation (1V/oct, per synth-high-quality skill)
        float smCutoff = cutoffSmoothed.getNextValue();
        float smReso = resoSmoothed.getNextValue();
        float smDrive = driveSmoothed.getNextValue();
        float smEnvDepth = envDepthSmoothed.getNextValue();

        // Key tracking: octave-based (exponential, not linear)
        static constexpr float kKeyTrackAmounts[] = { 0.0f, 0.33f, 0.67f, 1.0f };
        float keyTrackAmt = kKeyTrackAmounts[std::clamp(vcfKeyTrackMode, 0, 3)];

        // Exponential cutoff modulation per spec:
        //   fc = baseCutoff × 2^(envDepth × envVal × 4.0)
        //                    × 2^(modFilterLfo × filterModDepth)
        //                    × 2^(keyTrack × (note - 60) / 12)
        float fc = smCutoff
                 * fastPow2(smEnvDepth * env1Val * 4.0f)
                 * fastPow2(modFilterLfo * 2.0f)
                 * fastPow2(keyTrackAmt * (static_cast<float>(midiNoteNum) - 60.0f) / 12.0f);

        // Osc 3 filter modulation when in LFO mode
        if (vco3CtrlMode == 1)
        {
            fc *= fastPow2(osc3ModValue * vcfLfoAmount * 2.0f);
        }

        fc = std::clamp(fc, 20.0f, 20000.0f);
        vcf.setParameters(fc, smReso, smDrive);

        // 10. Filter
        float filtered = vcf.processSample(mix);

        // 11. VCA: filtered × loudness contour × master volume
        float smMasterVol = masterVolSmoothed.getNextValue();
        float output = filtered * env2Val * velocity * smMasterVol;

        // 12. Soft clip (tube-like output saturation per spec)
        output = softClip(output * 1.2f);

        // 13. DC blocker — prevents DC offset from pulse waves and feedback
        //     causing clicks when gate closes (per synth-click-prevention skill)
        output = dcBlock(output);

        // 14. Store feedback sample for next iteration
        feedbackSample = output;

        // 15. Write to buffer (mono → both channels)
        int pos = startSample + sample;
        buffer.addSample(0, pos, output);
        buffer.addSample(1, pos, output);
    }
}

// ── Parameter update ────────────────────────────────────────────────────────

void Moog15Voice::setParameters(int vco1Wave, int vco1Range, float vco1Level, float vco1PW,
                                 int vco2Wave, int vco2Range, float vco2Detune, float vco2Level, float vco2PW,
                                 int vco3Wave, int vco3Range, float vco3Detune, float vco3Level, float vco3PW,
                                 int vco3Ctrl,
                                 float noiseLvl, int noiseColor,
                                 float feedback,
                                 float cutoff, float reso, float drive, float envDepth, int keyTrack,
                                 float e1A, float e1D, float e1S,
                                 float e2A, float e2D, float e2S,
                                 float glideTimeParam,
                                 float lfoPitchAmt, float vcfLfoAmt,
                                 float lfoRate, int lfoWave,
                                 float masterVol)
{
    // Waveforms
    vco1.setWaveform(vco1Wave);
    vco2.setWaveform(vco2Wave);
    vco3.setWaveform(vco3Wave);

    // Pulse widths
    vco1.setPulseWidth(vco1PW);
    vco2.setPulseWidth(vco2PW);
    vco3.setPulseWidth(vco3PW);

    // Range indices (0–4 for VCO1/2, 0–5 for VCO3 with Lo)
    vco1RangeIdx = std::clamp(vco1Range, 0, 4);
    vco2RangeIdx = std::clamp(vco2Range, 0, 4);
    vco3RangeIdx = std::clamp(vco3Range, 0, 5);

    // Detune
    vco2DetuneCents = vco2Detune;
    vco3DetuneCents = vco3Detune;

    // Osc 3 control mode
    vco3CtrlMode = vco3Ctrl;
    if (vco3CtrlMode == 1 && vco3RangeIdx == 5)
    {
        // Lo range: Osc 3 runs as LFO (0.1–30 Hz, free-running)
        // Use lfoRate as the Osc 3 LFO frequency
        vco3.setFrequency(lfoRate);
    }

    // Noise
    noiseLevelParam = noiseLvl;
    noiseColorParam = noiseColor;

    // Feedback
    feedbackGain = std::clamp(feedback, 0.0f, 0.8f);

    // Filter
    vcfBaseCutoff = cutoff;
    vcfResonance = reso;
    vcfDrive = drive;
    vcfEnvDepth = envDepth;
    vcfKeyTrackMode = keyTrack;
    vcfLfoAmount = vcfLfoAmt;

    // Contour generators (Model D: decay IS the release, no separate release param)
    filterContour.setParameters(e1A, e1D, e1S);
    loudnessContour.setParameters(e2A, e2D, e2S);

    // Glide
    glideTime = glideTimeParam;

    // LFO
    lfoPitchAmount = lfoPitchAmt;
    lfoWaveform = lfoWave;
    lfoPhaseInc = lfoRate / static_cast<float>(getSampleRate());

    // Master volume
    masterVolume = masterVol;

    // Update smoother targets (per DSP conventions skill)
    cutoffSmoothed.setTargetValue(cutoff);
    resoSmoothed.setTargetValue(reso);
    driveSmoothed.setTargetValue(drive);
    mixVco1Smoothed.setTargetValue(vco1Level);
    mixVco2Smoothed.setTargetValue(vco2Level);
    mixVco3Smoothed.setTargetValue(vco3Level);
    noiseLevelSmoothed.setTargetValue(noiseLvl);
    feedbackSmoothed.setTargetValue(feedbackGain);
    envDepthSmoothed.setTargetValue(envDepth);
    masterVolSmoothed.setTargetValue(masterVol);
}

} // namespace axelf::moog15
