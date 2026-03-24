import re

with open(r'c:\axelF\Source\PluginProcessor.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Remove songModeActiveScenes from constructor init list if exists
text = re.sub(r',\s*songModeActiveScenes\{.*?\}', '', text)

# 2. Inside processBlock, remove arrangement references (override)
text = re.sub(r'    // Set pattern length override for Song mode \(total arrangement bars\)\s+if \(arrangement\.getMode\(\) == ArrangementMode::Song\)\s+\{\s+int totalBars = std::max\(1, arrangement\.getTotalBars\(\)\);\s+transport\.setPatternLengthOverride\(\s+static_cast<double>\(totalBars\) \* static_cast<double>\(transport\.getTimeSignatureNumerator\(\)\)\);\s+\}\s+else\s+\{\s+transport\.setPatternLengthOverride\(0\.0\);\s+\}', '', text)

# 3. Inside processBlock, remove Scene changes on loop boundary
text = re.sub(r'    // -- 2b\. Scene changes on loop boundary \(BEFORE pattern engine\) --\s+if \(transport\.didLoopReset\(\)\)\s+\{.*?    // -- 2c\. SONG mode: load patterns per-module based on timeline bar --', '    // -- 2b. / 2c. Removed --\n\n    // -- 2c. SONG mode: load patterns per-module based on timeline bar --', text, flags=re.DOTALL)

# 4. Remove 2c
text = re.sub(r'    // -- 2c\. SONG mode: load patterns per-module based on timeline bar --\s+if \(arrangement\.getMode\(\) == ArrangementMode::Song.*?\}\s+\}\s+\}', '// -- 2c. Removed --\n', text, flags=re.DOTALL)

# 5. Replace pattern engine call
pattern_start = text.find('    // -- 3. Pattern engine: record + playback ? output MIDI --')
pattern_end = text.find('    // -- 4. Process pattern engine')
if pattern_end == -1:
    pattern_end = text.find('    // -- 4. Process each module --')
if pattern_end == -1:
    pattern_end = text.find('    // -- 5. Route module MIDI')

if pattern_start != -1 and pattern_end != -1:
    new_pattern = '''    // -- 3. Pattern engine (LinnDrum step sequencer only) --
    patternEngine.processLinnDrumOnly(transport, numSamples, linnMidi);

'''
    text = text[:pattern_start] + new_pattern + text[pattern_end:]
else:
    print("Pattern 3-4 not found!", pattern_start, pattern_end)

# 6. Change outMidi to Midi
text = text.replace('jup8OutMidi', 'jup8Midi')
text = text.replace('moogOutMidi', 'moogMidi')
text = text.replace('jx3pOutMidi', 'jx3pMidi')
text = text.replace('dx7OutMidi', 'dx7Midi')
text = text.replace('linnOutMidi', 'linnMidi')

# 7. Remove XML arrangement
text = re.sub(r'\s*if \(auto arrXml = arrangement\.toXml\(\)\)\s*session->addChildElement\(arrXml\.release\(\)\);', '', text)
text = re.sub(r'\s*if \(auto\* arrXml = session->getChildByName\(\"Arrangement\"\)\)\s*arrangement\.fromXml\(arrXml\);', '', text)

# 8. Remove clear
text = re.sub(r'\s*arrangement\.clear\(\);\s*arrangement\.setMode\(ArrangementMode::Jam\);', '', text)

with open(r'c:\axelF\Source\PluginProcessor.cpp', 'w', encoding='utf-8') as f:
    f.write(text)

print("Done")
