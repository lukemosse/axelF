#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <mutex>

namespace axelf
{

class MidiLearn
{
public:
    struct Mapping
    {
        int ccNumber = -1;
        juce::String parameterID;
    };

    // Start learning — the next CC received will be mapped to this parameter
    void startLearning(const juce::String& parameterID)
    {
        std::lock_guard<std::mutex> lock(mutex);
        learningParamID = parameterID;
        learning = true;
    }

    void cancelLearning()
    {
        std::lock_guard<std::mutex> lock(mutex);
        learning = false;
        learningParamID.clear();
    }

    bool isLearning() const { return learning; }
    juce::String getLearningParam() const { return learningParamID; }

    // Remove mapping for a specific parameter
    void unmap(const juce::String& parameterID)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto it = ccToParam.begin(); it != ccToParam.end(); ++it)
        {
            if (it->second == parameterID)
            {
                ccToParam.erase(it);
                break;
            }
        }
    }

    // Remove all mappings
    void clearAll()
    {
        std::lock_guard<std::mutex> lock(mutex);
        ccToParam.clear();
    }

    // Called from audio thread — processes CC messages, applies mapped values
    // paramLookup: given a parameterID, returns the RangedAudioParameter* or nullptr
    void processMidi(const juce::MidiBuffer& midiBuffer,
                     std::function<juce::RangedAudioParameter*(const juce::String&)> paramLookup)
    {
        for (const auto metadata : midiBuffer)
        {
            const auto msg = metadata.getMessage();
            if (!msg.isController())
                continue;

            const int cc = msg.getControllerNumber();
            const float normValue = msg.getControllerValue() / 127.0f;

            // Learning mode: map this CC to the pending parameter
            if (learning)
            {
                std::lock_guard<std::mutex> lock(mutex);
                ccToParam.erase(cc);
                for (auto it = ccToParam.begin(); it != ccToParam.end(); ++it)
                {
                    if (it->second == learningParamID)
                    {
                        ccToParam.erase(it);
                        break;
                    }
                }
                ccToParam[cc] = learningParamID;
                learning = false;
                learningParamID.clear();
                continue;
            }

            // Apply mapped CCs
            std::lock_guard<std::mutex> lock(mutex);
            auto it = ccToParam.find(cc);
            if (it != ccToParam.end())
            {
                if (auto* param = paramLookup(it->second))
                    param->setValueNotifyingHost(normValue);
            }
        }
    }

    // Serialise to XML
    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("MidiLearn");
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& [cc, paramID] : ccToParam)
        {
            auto* mapping = xml->createNewChildElement("Map");
            mapping->setAttribute("cc", cc);
            mapping->setAttribute("param", paramID);
        }
        return xml;
    }

    // Deserialise from XML
    void fromXml(const juce::XmlElement& xml)
    {
        std::lock_guard<std::mutex> lock(mutex);
        ccToParam.clear();
        for (auto* child : xml.getChildIterator())
        {
            if (child->hasTagName("Map"))
            {
                int cc = child->getIntAttribute("cc", -1);
                auto paramID = child->getStringAttribute("param");
                if (cc >= 0 && cc <= 127 && paramID.isNotEmpty())
                    ccToParam[cc] = paramID;
            }
        }
    }

    // Get all current mappings (for UI display)
    std::map<int, juce::String> getMappings() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return ccToParam;
    }

    // Get CC number mapped to a parameter (-1 if none)
    int getCCForParam(const juce::String& parameterID) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& [cc, pid] : ccToParam)
        {
            if (pid == parameterID)
                return cc;
        }
        return -1;
    }

private:
    mutable std::mutex mutex;
    std::map<int, juce::String> ccToParam;
    std::atomic<bool> learning { false };
    juce::String learningParamID;
};

} // namespace axelf
