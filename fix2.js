const fs = require('fs');
const files = [
    'c:/axelF/Source/Modules/Jupiter8/Jupiter8Editor.h',
    'c:/axelF/Source/Modules/Moog15/Moog15Editor.h',
    'c:/axelF/Source/Modules/JX3P/JX3PEditor.h',
    'c:/axelF/Source/Modules/DX7/DX7Editor.h',
    'c:/axelF/Source/Modules/LinnDrum/LinnDrumEditor.h'
];

for (const file of files) {
    let content = fs.readFileSync(file, 'utf8');

    // Wipe out the entire makeSlider lambda and replace cleanly
    content = content.replace(/auto makeSlider\s*=\s*\[&\][\s\S]*?labels\.add\(lbl\);\s*\};/m, 
`auto makeSlider = [&](const juce::String& paramId, const juce::String& label, juce::Slider::SliderStyle style = juce::Slider::RotaryHorizontalVerticalDrag)
        {
            auto* slider = new juce::Slider(style, juce::Slider::TextBoxBelow);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
            auto* att = (apvts.getParameter(paramId) != nullptr) ? new juce::AudioProcessorValueTreeState::SliderAttachment(apvts, paramId, *slider) : nullptr;
            auto* lbl = new juce::Label({}, label);
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(juce::Font(11.0f));
            lbl->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(slider);
            addAndMakeVisible(lbl);
            sliders.add(slider);
            ` + (file.includes('DX7') ? `if(att) sliderAttachments.add(att);` : `if(att) attachments.add(att);`) + `
            ` + (file.includes('DX7') ? `sliderLabels.add(lbl);` : `labels.add(lbl);`) + `
        };`);

    // Only apply makeCombo to those that have it
    if (!file.includes('LinnDrum')) {
        content = content.replace(/auto makeCombo\s*=\s*\[&\][\s\S]*?comboLabels\.add\(lbl\);\s*\};/m, 
`auto makeCombo = [&](const juce::String& paramId, const juce::String& label)
        {
            auto* combo = new juce::ComboBox();
            if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId)))
                combo->addItemList(choiceParam->choices, 1);
            auto* att = (apvts.getParameter(paramId) != nullptr) ? new juce::AudioProcessorValueTreeState::ComboBoxAttachment(apvts, paramId, *combo) : nullptr;
            auto* lbl = new juce::Label({}, label);
            lbl->setJustificationType(juce::Justification::centred);
            lbl->setFont(juce::Font(11.0f));
            lbl->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
            addAndMakeVisible(combo);
            addAndMakeVisible(lbl);
            combos.add(combo);
            if (att) comboAttachments.add(att);
            comboLabels.add(lbl);
        };`);
    }

    // specific fix for DX7 50, 14
    if (file.includes('DX7')) {
        content = content.replace(/false, 60, 16/g, 'false, 50, 14');
        content = content.replace(/juce::Font\(11\.0f\)/g, 'juce::Font(9.0f)');
    }
    // specific fix for LinnDrum 48, 14
    if (file.includes('LinnDrum')) {
        content = content.replace(/false, 60, 16/g, 'false, 48, 14');
        content = content.replace(/juce::Font\(11\.0f\)/g, 'juce::Font(9.0f)');
    }

    fs.writeFileSync(file, content);
}
