#include "MasterMixer.h"
#include <cmath>

namespace axelf
{

// ── Parameter layout for mixer APVTS ─────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout createMixerParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    for (int i = 0; i < 6; ++i)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { MixerParamIDs::levelID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Level",
            juce::NormalisableRange<float> (0.0f, 1.5f, 0.01f), 0.75f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { MixerParamIDs::panID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Pan",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { MixerParamIDs::muteID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Mute",
            false));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { MixerParamIDs::soloID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Solo",
            false));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { MixerParamIDs::send1ID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Send 1",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { MixerParamIDs::send2ID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Send 2",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { MixerParamIDs::send3ID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Send 3",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { MixerParamIDs::send4ID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Send 4",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { MixerParamIDs::send5ID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Send 5",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { MixerParamIDs::tiltID (i), 1 },
            juce::String (MixerParamIDs::kPrefixes[i]) + " Tilt EQ",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
    }

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { MixerParamIDs::kMasterLevel, 1 },
        "Master Level",
        juce::NormalisableRange<float> (0.0f, 1.5f, 0.01f), 1.0f));

    return { params.begin(), params.end() };
}

// ── Bind cached pointers ─────────────────────────────────────
void MasterMixer::bindToAPVTS (juce::AudioProcessorValueTreeState& apvts)
{
    boundAPVTS = &apvts;

    for (int i = 0; i < 6; ++i)
    {
        channels[static_cast<size_t> (i)].level = apvts.getRawParameterValue (MixerParamIDs::levelID (i));
        channels[static_cast<size_t> (i)].pan   = apvts.getRawParameterValue (MixerParamIDs::panID (i));
        channels[static_cast<size_t> (i)].mute  = apvts.getRawParameterValue (MixerParamIDs::muteID (i));
        channels[static_cast<size_t> (i)].solo  = apvts.getRawParameterValue (MixerParamIDs::soloID (i));
        channels[static_cast<size_t> (i)].send1 = apvts.getRawParameterValue (MixerParamIDs::send1ID (i));
        channels[static_cast<size_t> (i)].send2 = apvts.getRawParameterValue (MixerParamIDs::send2ID (i));
        channels[static_cast<size_t> (i)].send3 = apvts.getRawParameterValue (MixerParamIDs::send3ID (i));
        channels[static_cast<size_t> (i)].send4 = apvts.getRawParameterValue (MixerParamIDs::send4ID (i));
        channels[static_cast<size_t> (i)].send5 = apvts.getRawParameterValue (MixerParamIDs::send5ID (i));
        channels[static_cast<size_t> (i)].tilt  = apvts.getRawParameterValue (MixerParamIDs::tiltID (i));
    }

    masterLevelPtr = apvts.getRawParameterValue (MixerParamIDs::kMasterLevel);
}

// ── Prepare smoothing ────────────────────────────────────────
void MasterMixer::prepare (double sampleRate)
{
    for (int i = 0; i < 6; ++i)
    {
        levelSmoothed[i].reset (sampleRate, 0.01); // 10ms ramp
        panSmoothed[i].reset (sampleRate, 0.01);
        levelSmoothed[i].setCurrentAndTargetValue (1.0f);
        panSmoothed[i].setCurrentAndTargetValue (0.0f);
    }
    masterSmoothed.reset (sampleRate, 0.01);
    masterSmoothed.setCurrentAndTargetValue (1.0f);

    // Tilt EQ crossover at ~1 kHz
    tiltCoeff = 1.0f - std::exp (static_cast<float> (-2.0 * juce::MathConstants<double>::pi * 1000.0 / sampleRate));
    for (int i = 0; i < 6; ++i)
    {
        tiltLpStateL[i] = 0.0f;
        tiltLpStateR[i] = 0.0f;
    }

    prepared = true;
}

// ── Audio-thread process ─────────────────────────────────────
void MasterMixer::process (const std::array<juce::AudioBuffer<float>*, 6>& moduleOutputs,
                           juce::AudioBuffer<float>& mainOut)
{
    mainOut.clear();

    // Check if any channel is solo'd
    bool anySolo = false;
    for (const auto& ch : channels)
    {
        if (ch.solo && ch.solo->load (std::memory_order_relaxed) >= 0.5f)
        {
            anySolo = true;
            break;
        }
    }

    const float master = masterLevelPtr ? masterLevelPtr->load (std::memory_order_relaxed) : 1.0f;
    if (prepared) masterSmoothed.setTargetValue (master);

    for (size_t i = 0; i < 6; ++i)
    {
        auto& ch  = channels[i];
        const auto& src = *moduleOutputs[i];

        const float level = ch.level ? ch.level->load (std::memory_order_relaxed) : 1.0f;
        const float pan   = ch.pan   ? ch.pan->load (std::memory_order_relaxed)   : 0.0f;
        const bool  muted = ch.mute  ? ch.mute->load (std::memory_order_relaxed) >= 0.5f : false;
        const bool  solo  = ch.solo  ? ch.solo->load (std::memory_order_relaxed) >= 0.5f : false;
        const float tilt  = ch.tilt  ? ch.tilt->load (std::memory_order_relaxed)  : 0.0f;

        if (prepared)
        {
            levelSmoothed[i].setTargetValue (level);
            panSmoothed[i].setTargetValue (pan);
        }

        // Compute peak level for metering
        float peak = 0.0f;
        for (int c = 0; c < src.getNumChannels(); ++c)
            peak = std::max (peak, src.getMagnitude (c, 0, mainOut.getNumSamples()));
        ch.peakLevel.store (peak * level, std::memory_order_relaxed);

        // Skip if muted, or if solo mode is active and this channel isn't solo'd
        if (muted)
            continue;
        if (anySolo && !solo)
            continue;

        if (src.getNumChannels() < 2 || mainOut.getNumChannels() < 2)
            continue;

        const int numSamples = mainOut.getNumSamples();
        const auto* srcL = src.getReadPointer (0);
        const auto* srcR = src.getReadPointer (1);
        auto* outL = mainOut.getWritePointer (0);
        auto* outR = mainOut.getWritePointer (1);

        const bool smoothing = prepared
            && (levelSmoothed[i].isSmoothing() || panSmoothed[i].isSmoothing());
        const bool hasTilt = std::abs (tilt) > 0.001f;
        const float tiltLow  = 1.0f - tilt * 0.5f;
        const float tiltHigh = 1.0f + tilt * 0.5f;

        if (smoothing || hasTilt)
        {
            for (int s = 0; s < numSamples; ++s)
            {
                float inL = srcL[s];
                float inR = srcR[s];

                if (hasTilt)
                {
                    tiltLpStateL[i] += tiltCoeff * (inL - tiltLpStateL[i]);
                    tiltLpStateR[i] += tiltCoeff * (inR - tiltLpStateR[i]);
                    inL = tiltLpStateL[i] * tiltLow + (inL - tiltLpStateL[i]) * tiltHigh;
                    inR = tiltLpStateR[i] * tiltLow + (inR - tiltLpStateR[i]) * tiltHigh;
                }

                const float lv    = smoothing ? levelSmoothed[i].getNextValue() : level;
                const float pn    = smoothing ? panSmoothed[i].getNextValue()   : pan;
                const float theta = (pn + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                const float gL    = lv * master * std::cos (theta);
                const float gR    = lv * master * std::sin (theta);
                outL[s] += inL * gL;
                outR[s] += inR * gR;
            }
        }
        else
        {
            const float theta = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gainL = level * master * std::cos (theta);
            const float gainR = level * master * std::sin (theta);
            mainOut.addFrom (0, 0, src, 0, 0, numSamples, gainL);
            mainOut.addFrom (1, 0, src, 1, 0, numSamples, gainR);
        }
    }

    // Master output peak
    {
        float mPeak = 0.0f;
        for (int c = 0; c < mainOut.getNumChannels(); ++c)
            mPeak = std::max (mPeak, mainOut.getMagnitude (c, 0, mainOut.getNumSamples()));
        masterPeakLevel.store (mPeak, std::memory_order_relaxed);
    }
}

// ── Extended process with aux send taps ──────────────────────
void MasterMixer::process (const std::array<juce::AudioBuffer<float>*, 6>& moduleOutputs,
                           juce::AudioBuffer<float>& mainOut,
                           juce::AudioBuffer<float>& aux1Out,
                           juce::AudioBuffer<float>& aux2Out)
{
    mainOut.clear();
    aux1Out.clear();
    aux2Out.clear();

    // Check if any channel is solo'd
    bool anySolo = false;
    for (const auto& ch : channels)
    {
        if (ch.solo && ch.solo->load (std::memory_order_relaxed) >= 0.5f)
        {
            anySolo = true;
            break;
        }
    }

    const float master = masterLevelPtr ? masterLevelPtr->load (std::memory_order_relaxed) : 1.0f;
    if (prepared) masterSmoothed.setTargetValue (master);

    for (size_t i = 0; i < 6; ++i)
    {
        auto& ch  = channels[i];
        const auto& src = *moduleOutputs[i];

        const float level = ch.level ? ch.level->load (std::memory_order_relaxed) : 1.0f;
        const float pan   = ch.pan   ? ch.pan->load (std::memory_order_relaxed)   : 0.0f;
        const bool  muted = ch.mute  ? ch.mute->load (std::memory_order_relaxed) >= 0.5f : false;
        const bool  solo  = ch.solo  ? ch.solo->load (std::memory_order_relaxed) >= 0.5f : false;
        const float s1    = ch.send1 ? ch.send1->load (std::memory_order_relaxed) : 0.0f;
        const float s2    = ch.send2 ? ch.send2->load (std::memory_order_relaxed) : 0.0f;
        const float tilt  = ch.tilt  ? ch.tilt->load (std::memory_order_relaxed)  : 0.0f;

        if (prepared)
        {
            levelSmoothed[i].setTargetValue (level);
            panSmoothed[i].setTargetValue (pan);
        }

        // Compute peak level for metering
        float peak = 0.0f;
        for (int c = 0; c < src.getNumChannels(); ++c)
            peak = std::max (peak, src.getMagnitude (c, 0, mainOut.getNumSamples()));
        ch.peakLevel.store (peak * level, std::memory_order_relaxed);

        if (muted)
            continue;
        if (anySolo && !solo)
            continue;

        if (src.getNumChannels() < 2 || mainOut.getNumChannels() < 2)
            continue;

        const int numSamples = mainOut.getNumSamples();
        const auto* srcL = src.getReadPointer (0);
        const auto* srcR = src.getReadPointer (1);
        auto* outL = mainOut.getWritePointer (0);
        auto* outR = mainOut.getWritePointer (1);

        const bool smoothing = prepared
            && (levelSmoothed[i].isSmoothing() || panSmoothed[i].isSmoothing());
        const bool hasTilt2 = std::abs (tilt) > 0.001f;
        const float tiltLow2  = 1.0f - tilt * 0.5f;
        const float tiltHigh2 = 1.0f + tilt * 0.5f;

        if (smoothing || hasTilt2)
        {
            auto* a1L = aux1Out.getWritePointer (0);
            auto* a1R = aux1Out.getWritePointer (1);
            auto* a2L = aux2Out.getWritePointer (0);
            auto* a2R = aux2Out.getWritePointer (1);

            for (int s = 0; s < numSamples; ++s)
            {
                float inL = srcL[s];
                float inR = srcR[s];

                if (hasTilt2)
                {
                    tiltLpStateL[i] += tiltCoeff * (inL - tiltLpStateL[i]);
                    tiltLpStateR[i] += tiltCoeff * (inR - tiltLpStateR[i]);
                    inL = tiltLpStateL[i] * tiltLow2 + (inL - tiltLpStateL[i]) * tiltHigh2;
                    inR = tiltLpStateR[i] * tiltLow2 + (inR - tiltLpStateR[i]) * tiltHigh2;
                }

                const float lv    = smoothing ? levelSmoothed[i].getNextValue() : level;
                const float pn    = smoothing ? panSmoothed[i].getNextValue()   : pan;
                const float theta = (pn + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                const float gL    = lv * master * std::cos (theta);
                const float gR    = lv * master * std::sin (theta);
                outL[s] += inL * gL;
                outR[s] += inR * gR;

                if (s1 > 0.001f)
                {
                    a1L[s] += inL * gL * s1;
                    a1R[s] += inR * gR * s1;
                }
                if (s2 > 0.001f)
                {
                    a2L[s] += inL * gL * s2;
                    a2R[s] += inR * gR * s2;
                }
            }
        }
        else
        {
            const float theta = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gainL = level * master * std::cos (theta);
            const float gainR = level * master * std::sin (theta);

            mainOut.addFrom (0, 0, src, 0, 0, numSamples, gainL);
            mainOut.addFrom (1, 0, src, 1, 0, numSamples, gainR);

            if (s1 > 0.001f)
            {
                aux1Out.addFrom (0, 0, src, 0, 0, numSamples, gainL * s1);
                aux1Out.addFrom (1, 0, src, 1, 0, numSamples, gainR * s1);
            }
            if (s2 > 0.001f)
            {
                aux2Out.addFrom (0, 0, src, 0, 0, numSamples, gainL * s2);
                aux2Out.addFrom (1, 0, src, 1, 0, numSamples, gainR * s2);
            }
        }
    }

    // Master output peak
    {
        float mPeak = 0.0f;
        for (int c = 0; c < mainOut.getNumChannels(); ++c)
            mPeak = std::max (mPeak, mainOut.getMagnitude (c, 0, mainOut.getNumSamples()));
        masterPeakLevel.store (mPeak, std::memory_order_relaxed);
    }
}

// ── Extended process with 5 aux send taps ────────────────────
void MasterMixer::process (const std::array<juce::AudioBuffer<float>*, 6>& moduleOutputs,
                           juce::AudioBuffer<float>& mainOut,
                           juce::AudioBuffer<float>& aux1Out,
                           juce::AudioBuffer<float>& aux2Out,
                           juce::AudioBuffer<float>& aux3Out,
                           juce::AudioBuffer<float>& aux4Out,
                           juce::AudioBuffer<float>& aux5Out)
{
    mainOut.clear();
    aux1Out.clear();
    aux2Out.clear();
    aux3Out.clear();
    aux4Out.clear();
    aux5Out.clear();

    bool anySolo = false;
    for (const auto& ch : channels)
        if (ch.solo && ch.solo->load (std::memory_order_relaxed) >= 0.5f)
        { anySolo = true; break; }

    const float master = masterLevelPtr ? masterLevelPtr->load (std::memory_order_relaxed) : 1.0f;
    if (prepared) masterSmoothed.setTargetValue (master);

    for (size_t i = 0; i < 6; ++i)
    {
        auto& ch  = channels[i];
        const auto& src = *moduleOutputs[i];

        const float level = ch.level ? ch.level->load (std::memory_order_relaxed) : 1.0f;
        const float pan   = ch.pan   ? ch.pan->load (std::memory_order_relaxed)   : 0.0f;
        const bool  muted = ch.mute  ? ch.mute->load (std::memory_order_relaxed) >= 0.5f : false;
        const bool  solo  = ch.solo  ? ch.solo->load (std::memory_order_relaxed) >= 0.5f : false;
        const float s1    = ch.send1 ? ch.send1->load (std::memory_order_relaxed) : 0.0f;
        const float s2    = ch.send2 ? ch.send2->load (std::memory_order_relaxed) : 0.0f;
        const float s3    = ch.send3 ? ch.send3->load (std::memory_order_relaxed) : 0.0f;
        const float s4    = ch.send4 ? ch.send4->load (std::memory_order_relaxed) : 0.0f;
        const float s5    = ch.send5 ? ch.send5->load (std::memory_order_relaxed) : 0.0f;
        const float tilt  = ch.tilt  ? ch.tilt->load (std::memory_order_relaxed)  : 0.0f;

        if (prepared)
        {
            levelSmoothed[i].setTargetValue (level);
            panSmoothed[i].setTargetValue (pan);
        }

        float peak = 0.0f;
        for (int c = 0; c < src.getNumChannels(); ++c)
            peak = std::max (peak, src.getMagnitude (c, 0, mainOut.getNumSamples()));
        ch.peakLevel.store (peak * level, std::memory_order_relaxed);

        if (muted) continue;
        if (anySolo && !solo) continue;
        if (src.getNumChannels() < 2 || mainOut.getNumChannels() < 2) continue;

        const int numSamples = mainOut.getNumSamples();
        const bool hasTilt5 = std::abs (tilt) > 0.001f;

        if (hasTilt5)
        {
            // Per-sample path with tilt
            const auto* srcL = src.getReadPointer (0);
            const auto* srcR = src.getReadPointer (1);
            auto* outL = mainOut.getWritePointer (0);
            auto* outR = mainOut.getWritePointer (1);
            const float tL5 = 1.0f - tilt * 0.5f;
            const float tH5 = 1.0f + tilt * 0.5f;

            const float theta = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gainL = level * master * std::cos (theta);
            const float gainR = level * master * std::sin (theta);

            for (int s = 0; s < numSamples; ++s)
            {
                float inL = srcL[s];
                float inR = srcR[s];
                tiltLpStateL[i] += tiltCoeff * (inL - tiltLpStateL[i]);
                tiltLpStateR[i] += tiltCoeff * (inR - tiltLpStateR[i]);
                inL = tiltLpStateL[i] * tL5 + (inL - tiltLpStateL[i]) * tH5;
                inR = tiltLpStateR[i] * tL5 + (inR - tiltLpStateR[i]) * tH5;

                outL[s] += inL * gainL;
                outR[s] += inR * gainR;

                if (s1 > 0.001f) { aux1Out.getWritePointer(0)[s] += inL * gainL * s1; aux1Out.getWritePointer(1)[s] += inR * gainR * s1; }
                if (s2 > 0.001f) { aux2Out.getWritePointer(0)[s] += inL * gainL * s2; aux2Out.getWritePointer(1)[s] += inR * gainR * s2; }
                if (s3 > 0.001f) { aux3Out.getWritePointer(0)[s] += inL * gainL * s3; aux3Out.getWritePointer(1)[s] += inR * gainR * s3; }
                if (s4 > 0.001f) { aux4Out.getWritePointer(0)[s] += inL * gainL * s4; aux4Out.getWritePointer(1)[s] += inR * gainR * s4; }
                if (s5 > 0.001f) { aux5Out.getWritePointer(0)[s] += inL * gainL * s5; aux5Out.getWritePointer(1)[s] += inR * gainR * s5; }
            }
        }
        else
        {
            const float theta = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gainL = level * master * std::cos (theta);
            const float gainR = level * master * std::sin (theta);

            mainOut.addFrom (0, 0, src, 0, 0, numSamples, gainL);
            mainOut.addFrom (1, 0, src, 1, 0, numSamples, gainR);

            if (s1 > 0.001f) { aux1Out.addFrom (0, 0, src, 0, 0, numSamples, gainL * s1); aux1Out.addFrom (1, 0, src, 1, 0, numSamples, gainR * s1); }
            if (s2 > 0.001f) { aux2Out.addFrom (0, 0, src, 0, 0, numSamples, gainL * s2); aux2Out.addFrom (1, 0, src, 1, 0, numSamples, gainR * s2); }
            if (s3 > 0.001f) { aux3Out.addFrom (0, 0, src, 0, 0, numSamples, gainL * s3); aux3Out.addFrom (1, 0, src, 1, 0, numSamples, gainR * s3); }
            if (s4 > 0.001f) { aux4Out.addFrom (0, 0, src, 0, 0, numSamples, gainL * s4); aux4Out.addFrom (1, 0, src, 1, 0, numSamples, gainR * s4); }
            if (s5 > 0.001f) { aux5Out.addFrom (0, 0, src, 0, 0, numSamples, gainL * s5); aux5Out.addFrom (1, 0, src, 1, 0, numSamples, gainR * s5); }
        }
    }

    // Master output peak
    {
        float mPeak = 0.0f;
        for (int c = 0; c < mainOut.getNumChannels(); ++c)
            mPeak = std::max (mPeak, mainOut.getMagnitude (c, 0, mainOut.getNumSamples()));
        masterPeakLevel.store (mPeak, std::memory_order_relaxed);
    }
}

// ── Thread-safe read accessors ───────────────────────────────
float MasterMixer::getLevel (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].level;
    return p ? p->load (std::memory_order_relaxed) : 1.0f;
}

float MasterMixer::getPan (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].pan;
    return p ? p->load (std::memory_order_relaxed) : 0.0f;
}

bool MasterMixer::getMute (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].mute;
    return p ? p->load (std::memory_order_relaxed) >= 0.5f : false;
}

bool MasterMixer::getSolo (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].solo;
    return p ? p->load (std::memory_order_relaxed) >= 0.5f : false;
}

float MasterMixer::getSend1 (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].send1;
    return p ? p->load (std::memory_order_relaxed) : 0.0f;
}

float MasterMixer::getSend2 (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].send2;
    return p ? p->load (std::memory_order_relaxed) : 0.0f;
}

float MasterMixer::getSend3 (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].send3;
    return p ? p->load (std::memory_order_relaxed) : 0.0f;
}

float MasterMixer::getSend4 (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].send4;
    return p ? p->load (std::memory_order_relaxed) : 0.0f;
}

float MasterMixer::getSend5 (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].send5;
    return p ? p->load (std::memory_order_relaxed) : 0.0f;
}

float MasterMixer::getTilt (int ch) const
{
    auto* p = channels[static_cast<size_t> (ch)].tilt;
    return p ? p->load (std::memory_order_relaxed) : 0.0f;
}

float MasterMixer::getMasterLevel() const
{
    return masterLevelPtr ? masterLevelPtr->load (std::memory_order_relaxed) : 1.0f;
}

// ── Thread-safe write accessors ──────────────────────────────
void MasterMixer::setLevel (int ch, float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::levelID (ch)))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void MasterMixer::setPan (int ch, float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::panID (ch)))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void MasterMixer::setMute (int ch, bool v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::muteID (ch)))
            param->setValueNotifyingHost (v ? 1.0f : 0.0f);
}

void MasterMixer::setSolo (int ch, bool v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::soloID (ch)))
            param->setValueNotifyingHost (v ? 1.0f : 0.0f);
}

void MasterMixer::setSend1 (int ch, float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::send1ID (ch)))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void MasterMixer::setSend2 (int ch, float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::send2ID (ch)))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void MasterMixer::setSend3 (int ch, float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::send3ID (ch)))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void MasterMixer::setSend4 (int ch, float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::send4ID (ch)))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void MasterMixer::setSend5 (int ch, float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::send5ID (ch)))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void MasterMixer::setTilt (int ch, float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::tiltID (ch)))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

void MasterMixer::setMasterLevel (float v)
{
    if (boundAPVTS)
        if (auto* param = boundAPVTS->getParameter (MixerParamIDs::kMasterLevel))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
}

} // namespace axelf
