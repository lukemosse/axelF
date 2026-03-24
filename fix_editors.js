const fs = require('fs');
const glob = require('glob');

const files = [
    'c:/axelF/Source/Modules/Jupiter8/Jupiter8Editor.h',
    'c:/axelF/Source/Modules/Moog15/Moog15Editor.h',
    'c:/axelF/Source/Modules/JX3P/JX3PEditor.h',
    'c:/axelF/Source/Modules/DX7/DX7Editor.h',
    'c:/axelF/Source/Modules/LinnDrum/LinnDrumEditor.h'
];

for (const file of files) {
    let code = fs.readFileSync(file, 'utf8');

    // Restore slider lambda completely
    code = code.replace(/auto makeSlider = \[\&\][^]*?sliders\.add\(slider\);\s+if \(attachment\) attachments\.add\(attachment\);\s+labels\.add\(lbl\);\s+\};/gm, 
`auto makeSlider = [&](const juce::String& paramId, const juce::String& label, juce::Slider::SliderStyle style = juce::Slider::RotaryHorizontalVerticalDrag)
        {
            auto* slider = new juce::Slider(style, juce::Slider::TextBoxBelow);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
            if (apvts.getParameter(paramId) == nullptr) {
                juce::Logger::writeToLog("MISSING SLIDER PARAM: " + paramId);
            }
            auto* attachment = apvts.getParameter(paramId) ? new juce::AudioProcessorValueTreeState::SliderAttachment(apvts, paramId, *slider) : nullptr;
            auto* lbl = new juce::Label({}, label);
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(juce::FontOptions(11.0f));
            lbl->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(slider);
            addAndMakeVisible(lbl);
            sliders.add(slider);
            if (attachment) attachments.add(attachment);
            labels.add(lbl);
        };`);
        
    // Also DX7 and LinnDrum might have different makeSlider params or sizes, but let's try a regex that catches their inner content

    fs.writeFileSync(file, code);
}
