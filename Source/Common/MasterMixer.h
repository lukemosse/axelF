#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace axelf
{

// ── Mixer parameter ID helpers ───────────────────────────────
namespace MixerParamIDs
{
    static constexpr const char* kPrefixes[] = { "jup8", "moog", "jx3p", "dx7", "linn" };

    inline juce::String levelID (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_level"; }
    inline juce::String panID   (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_pan"; }
    inline juce::String muteID  (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_mute"; }
    inline juce::String soloID  (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_solo"; }
    inline juce::String send1ID (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_send1"; }
    inline juce::String send2ID (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_send2"; }
    inline juce::String send3ID (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_send3"; }
    inline juce::String send4ID (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_send4"; }
    inline juce::String send5ID (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_send5"; }
    inline juce::String tiltID  (int ch) { return juce::String ("mix_") + kPrefixes[ch] + "_tilt"; }

    static constexpr const char* kMasterLevel = "mix_master_level";
}

juce::AudioProcessorValueTreeState::ParameterLayout createMixerParameterLayout();

// ── Thread-safe APVTS-backed mixer ──────────────────────────
class MasterMixer
{
public:
    /** Call once after the mixer APVTS is constructed. */
    void bindToAPVTS (juce::AudioProcessorValueTreeState& apvts);

    /** Call from prepareToPlay to set smoothing rates. */
    void prepare (double sampleRate);

    void process (const std::array<juce::AudioBuffer<float>*, 5>& moduleOutputs,
                  juce::AudioBuffer<float>& mainOut);

    /** Extended process with aux send taps (post-fader). */
    void process (const std::array<juce::AudioBuffer<float>*, 5>& moduleOutputs,
                  juce::AudioBuffer<float>& mainOut,
                  juce::AudioBuffer<float>& aux1Out,
                  juce::AudioBuffer<float>& aux2Out);

    /** Extended process with 5 aux send taps (post-fader). */
    void process (const std::array<juce::AudioBuffer<float>*, 5>& moduleOutputs,
                  juce::AudioBuffer<float>& mainOut,
                  juce::AudioBuffer<float>& aux1Out,
                  juce::AudioBuffer<float>& aux2Out,
                  juce::AudioBuffer<float>& aux3Out,
                  juce::AudioBuffer<float>& aux4Out,
                  juce::AudioBuffer<float>& aux5Out);

    // Peak metering (written by audio thread, read by UI timer)
    float getPeakLevel (int index) const
    {
        return channels[static_cast<size_t> (index)].peakLevel.load (std::memory_order_relaxed);
    }

    // Thread-safe read accessors (read from APVTS cached atomics)
    float getLevel (int ch) const;
    float getPan   (int ch) const;
    bool  getMute  (int ch) const;
    bool  getSolo  (int ch) const;
    float getSend1 (int ch) const;
    float getSend2 (int ch) const;
    float getSend3 (int ch) const;
    float getSend4 (int ch) const;
    float getSend5 (int ch) const;
    float getTilt  (int ch) const;
    float getMasterLevel() const;

    // Thread-safe write accessors (via setValueNotifyingHost)
    void setLevel (int ch, float v);
    void setPan   (int ch, float v);
    void setMute  (int ch, bool  v);
    void setSolo  (int ch, bool  v);
    void setSend1 (int ch, float v);
    void setSend2 (int ch, float v);
    void setSend3 (int ch, float v);
    void setSend4 (int ch, float v);
    void setSend5 (int ch, float v);
    void setTilt  (int ch, float v);
    void setMasterLevel (float v);

private:
    juce::AudioProcessorValueTreeState* boundAPVTS = nullptr;

    struct CachedChannel
    {
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan   = nullptr;
        std::atomic<float>* mute  = nullptr;   // 0.0 or 1.0
        std::atomic<float>* solo  = nullptr;   // 0.0 or 1.0
        std::atomic<float>* send1 = nullptr;
        std::atomic<float>* send2 = nullptr;
        std::atomic<float>* send3 = nullptr;
        std::atomic<float>* send4 = nullptr;
        std::atomic<float>* send5 = nullptr;
        std::atomic<float>* tilt  = nullptr;
        std::atomic<float>  peakLevel { 0.0f };
    };

    std::array<CachedChannel, 5> channels;
    std::atomic<float>* masterLevelPtr = nullptr;

    // Smoothed gain values (Phase 8)
    juce::SmoothedValue<float> levelSmoothed[5];
    juce::SmoothedValue<float> panSmoothed[5];
    juce::SmoothedValue<float> masterSmoothed;
    bool prepared = false;

    // Tilt EQ filter state (one-pole crossover at ~1 kHz)
    float tiltLpStateL[5] = {};
    float tiltLpStateR[5] = {};
    float tiltCoeff = 0.134f;  // updated in prepare()
};

} // namespace axelf
