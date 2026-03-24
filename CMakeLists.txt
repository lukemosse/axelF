cmake_minimum_required(VERSION 3.22)
project(AxelF VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE ──────────────────────────────────────────────────────
add_subdirectory(extern/JUCE)

# ── Plugin Target ─────────────────────────────────────────────
juce_add_plugin(AxelF
    PRODUCT_NAME "AxelF"
    COMPANY_NAME "AxelF"
    PLUGIN_MANUFACTURER_CODE AXLF
    PLUGIN_CODE Axlf
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# ── Source Files ──────────────────────────────────────────────
target_sources(AxelF
    PRIVATE
        # Top-level
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp

        # UI
        Source/UI/AxelFLookAndFeel.cpp

        # Common
        Source/Common/MasterMixer.cpp
        Source/Common/ModulationMatrix.cpp
        Source/Common/PresetManager.cpp
        Source/Common/GlobalTransport.cpp
        Source/Common/Pattern.cpp
        Source/Common/PatternEngine.cpp
        Source/Common/SceneManager.cpp

        # Common DSP
        Source/Common/DSP/LadderFilter.cpp
        Source/Common/DSP/IR3109Filter.cpp
        Source/Common/DSP/ADSREnvelope.cpp
        Source/Common/DSP/LFOGenerator.cpp
        Source/Common/DSP/Oversampler.cpp
        Source/Common/DSP/PlateReverb.cpp
        Source/Common/DSP/StereoDelay.cpp
        Source/Common/DSP/MasterBus.cpp

        # Jupiter-8
        Source/Modules/Jupiter8/Jupiter8Processor.cpp
        Source/Modules/Jupiter8/Jupiter8Editor.cpp
        Source/Modules/Jupiter8/Jupiter8Voice.cpp
        Source/Modules/Jupiter8/Jupiter8DCO.cpp
        Source/Modules/Jupiter8/Jupiter8Params.cpp

        # Moog 15
        Source/Modules/Moog15/Moog15Processor.cpp
        Source/Modules/Moog15/Moog15Editor.cpp
        Source/Modules/Moog15/Moog15Voice.cpp
        Source/Modules/Moog15/Moog15VCO.cpp
        Source/Modules/Moog15/Moog15Params.cpp

        # JX-3P
        Source/Modules/JX3P/JX3PProcessor.cpp
        Source/Modules/JX3P/JX3PEditor.cpp
        Source/Modules/JX3P/JX3PVoice.cpp
        Source/Modules/JX3P/JX3PDCO.cpp
        Source/Modules/JX3P/JX3PChorus.cpp
        Source/Modules/JX3P/JX3PParams.cpp

        # DX7
        Source/Modules/DX7/DX7Processor.cpp
        Source/Modules/DX7/DX7Editor.cpp
        Source/Modules/DX7/DX7Voice.cpp
        Source/Modules/DX7/DX7Operator.cpp
        Source/Modules/DX7/DX7Algorithm.cpp
        Source/Modules/DX7/DX7Params.cpp

        # LinnDrum
        Source/Modules/LinnDrum/LinnDrumProcessor.cpp
        Source/Modules/LinnDrum/LinnDrumEditor.cpp
        Source/Modules/LinnDrum/LinnDrumVoice.cpp
        Source/Modules/LinnDrum/LinnDrumParams.cpp
)

# ── Include Paths ─────────────────────────────────────────────
target_include_directories(AxelF
    PRIVATE
        Source
        Source/Common
        Source/Common/DSP
        Source/UI
)

# ── JUCE Module Dependencies ─────────────────────────────────
target_link_libraries(AxelF
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_graphics
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
# ── Compiler Options ─────────────────────────────────────────────
if(MSVC)
    target_compile_options(AxelF PRIVATE /utf-8 /FS)
endif()
# ── Compile Definitions ──────────────────────────────────────
target_compile_definitions(AxelF
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)
