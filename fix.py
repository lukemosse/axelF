import os
import re

files = [
    r'c:\axelF\Source\Modules\Jupiter8\Jupiter8Editor.h',
    r'c:\axelF\Source\Modules\Moog15\Moog15Editor.h',
    r'c:\axelF\Source\Modules\JX3P\JX3PEditor.h',
    r'c:\axelF\Source\Modules\DX7\DX7Editor.h',
    r'c:\axelF\Source\Modules\LinnDrum\LinnDrumEditor.h'
]

for filepath in files:
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Let's fix the slider lambda
    content = re.sub(
        r'auto\* p = apvts\.getParameter\(paramId\);.*?(?:auto\* att|auto\* attachment|\n).*?(?:auto\* lbl = new juce::Label|auto\s+\*lbl = new juce::Label|auto\* lbl\s*=\s*new juce::Label\(\{\},\s*label\);)',
        r'''auto* att = (apvts.getParameter(paramId) != nullptr) ? new juce::AudioProcessorValueTreeState::SliderAttachment(apvts, paramId, *slider) : nullptr;
            auto* lbl = new juce::Label({}, label);''', 
        content, flags=re.DOTALL
    )

    # Let's fix the combos:
    content = re.sub(
        r'auto\* p = apvts\.getParameter\(paramId\);.*?(?:auto\* att|auto\* attachment|\n).*?auto\* lbl = new juce::Label\(\{\},\s*label\);',
        r'''auto* att = (apvts.getParameter(paramId) != nullptr) ? new juce::AudioProcessorValueTreeState::ComboBoxAttachment(apvts, paramId, *combo) : nullptr;
            auto* lbl = new juce::Label({}, label);''',
        content, flags=re.DOTALL
    )

    # fix the add statements
    content = re.sub(r'if \(attachment\) attachments\.add\(attachment\);', r'if (att) attachments.add(att);', content)
    content = re.sub(r'comboif \(attachment\) attachments\.add\(attachment\);', r'if (att) comboAttachments.add(att);', content)
    
    # Also for DX7 attachments might be named sliderAttachments
    content = re.sub(r'if \(sliderAttachment\) sliderAttachments\.add\(sliderAttachment\);', r'if (att) sliderAttachments.add(att);', content)
    content = re.sub(r'if \(comboAttachment\) comboAttachments\.add\(comboAttachment\);', r'if (att) comboAttachments.add(att);', content)

    # Re-apply attachments add bug fix that my powershell did:
    content = re.sub(r'if \(att\) attachments\.add\(att\);', r'if (att) attachments.add(att);', content)
    content = re.sub(r'if \(att\) sliderAttachments\.add\(att\);', r'if (att) sliderAttachments.add(att);', content)
    content = re.sub(r'if \(att\) comboAttachments\.add\(att\);', r'if (att) comboAttachments.add(att);', content)

    with open(filepath, 'w') as f:
        f.write(content)
