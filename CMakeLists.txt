cmake_minimum_required(VERSION 3.22)
project(AxelF VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime — no vcruntime140.dll dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ── JUCE ──────────────────────────────────────────────────────
add_subdirectory(extern/JUCE)

# ── Plugin Target ─────────────────────────────────────────────
juce_add_plugin(AxelF
    PRODUCT_NAME "AxelF"
    COMPANY_NAME "AxelF"
    PLUGIN_MANUFACTURER_CODE AXLF
    PLUGIN_CODE Axlf
    FORMATS Standalone
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
        Source/UI/RenderDialog.cpp

        # Common
        Source/Common/MasterMixer.cpp
        Source/Common/ExternalPluginSlot.cpp
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
        Source/Common/DSP/DiffuseDelay.cpp
        Source/Common/DSP/MasterBus.cpp

        # Jupiter-8
        Source/Modules/Jupiter8/Jupiter8Processor.cpp
        Source/Modules/Jupiter8/Jupiter8Editor.cpp
        Source/Modules/Jupiter8/Jupiter8Voice.cpp
        Source/Modules/Jupiter8/Jupiter8DCO.cpp
        Source/Modules/Jupiter8/Jupiter8Params.cpp

        # Moog 15 (Minimoog Model D)
        Source/Modules/Moog15/Moog15Processor.cpp
        Source/Modules/Moog15/Moog15Editor.cpp
        Source/Modules/Moog15/Moog15Voice.cpp
        Source/Modules/Moog15/Moog15VCO.cpp
        Source/Modules/Moog15/Moog15Params.cpp
        Source/Modules/Moog15/MoogContour.cpp
        Source/Modules/Moog15/MoogLadder.cpp

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

        # PPG Wave
        Source/Modules/PPGWave/PPGWaveParams.cpp
        Source/Modules/PPGWave/PPGWaveOsc.cpp
        Source/Modules/PPGWave/SSM2044Filter.cpp
        Source/Modules/PPGWave/PPGWaveVoice.cpp
        Source/Modules/PPGWave/PPGWaveProcessor.cpp

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
# ── Embedded LinnDrum Samples ─────────────────────────────────
juce_add_binary_data(AxelFData
    SOURCES
        Resources/Samples/b01_BD_Lindrum_Orig.wav
        Resources/Samples/b01_Cabasa_Lindrum_Orig.wav
        Resources/Samples/b01_CH_Lindrum_Orig_Decay_06.wav
        Resources/Samples/b01_Clap_Lindrum_Orig.wav
        Resources/Samples/b01_Conga_Lindrum_Orig_24_Fs3.wav
        Resources/Samples/b01_Conga_Lindrum_Orig_36_Fs4.wav
        Resources/Samples/b01_Cowbell_Lindrum_Orig.wav
        Resources/Samples/b01_Crash_Lindrum_Orig.wav
        Resources/Samples/b01_OH_Lindrum_Orig.wav
        Resources/Samples/b01_Ride_Lindrum_Orig.wav
        Resources/Samples/b01_SD_Lindrum_Orig_17.wav
        Resources/Samples/b01_SS_Lindrum_Orig_11.wav
        Resources/Samples/b01_Tamb_Lindrum_Orig.wav
        Resources/Samples/b01_Tom_Lindrum_Orig_09.wav
        Resources/Samples/b01_Tom_Lindrum_Orig_18.wav
        Resources/Samples/b01_Tom_Lindrum_Orig_22.wav
        Resources/Samples/b02_BD_Lindrum_Color_A_Orig.wav
        Resources/Samples/b02_Cabasa_Lindrum_Color_Orig.wav
        Resources/Samples/b02_CH_Lindrum_Color_Orig_Decay_04.wav
        Resources/Samples/b02_Clap_Lindrum_Color_A_Orig.wav
        Resources/Samples/b02_Conga_Lindrum_Color_A_Orig_15_A2.wav
        Resources/Samples/b02_Conga_Lindrum_Color_A_Orig_27_A3.wav
        Resources/Samples/b02_Cowbell_Lindrum_Color_Orig.wav
        Resources/Samples/b02_Crash_Lindrum_Color_Orig.wav
        Resources/Samples/b02_OH_Lindrum_Color_Orig.wav
        Resources/Samples/b02_Ride_Lindrum_Color_Orig.wav
        Resources/Samples/b02_SD_Lindrum_Color_B_Orig_17.wav
        Resources/Samples/b02_SS_Lindrum_Color_A_Orig_19.wav
        Resources/Samples/b02_Tamb_Lindrum_Color_Orig.wav
        Resources/Samples/b02_Tom_Lindrum_Color_A_Orig_13.wav
        Resources/Samples/b02_Tom_Lindrum_Color_A_Orig_19.wav
        Resources/Samples/b02_Tom_Lindrum_Color_A_Orig_27.wav
        Resources/Samples/b03_BD_Lindrum_Pitched_20.wav
        Resources/Samples/b03_Cabasa_Lindrum_Pitched_22.wav
        Resources/Samples/b03_CH_Lindrum_Orig_Decay_01.wav
        Resources/Samples/b03_CH_Lindrum_Orig_Decay_04.wav
        Resources/Samples/b03_Clap_Lindrum_Pitched_30.wav
        Resources/Samples/b03_Conga_Lindrum_Orig_22_E3.wav
        Resources/Samples/b03_Conga_Lindrum_Orig_29_B3.wav
        Resources/Samples/b03_Conga_Lindrum_Orig_34_E4.wav
        Resources/Samples/b03_Cowbell_Lindrum_Pitched_28.wav
        Resources/Samples/b03_OH_Lindrum_Orig.wav
        Resources/Samples/b03_SD_Lindrum_Orig_28.wav
        Resources/Samples/b03_SS_Lindrum_Orig_23.wav
        Resources/Samples/b03_Tamb_Lindrum_Pitched_22.wav
        Resources/Samples/b03_Tom_Lindrum_Orig_01.wav
        Resources/Samples/b03_Tom_Lindrum_Orig_21.wav
        Resources/Samples/b03_Tom_Lindrum_Orig_34.wav
        Resources/Samples/b04_BD_Lindrum_Color_D_Pitched_12.wav
        Resources/Samples/b04_BD_Lindrum_Color_D_Pitched_22.wav
        Resources/Samples/b04_Cabasa_Lindrum_Color_Pitched_31.wav
        Resources/Samples/b04_Clap_Lindrum_Color_A_Pitched_23.wav
        Resources/Samples/b04_Clap_Lindrum_Color_A_Pitched_37.wav
        Resources/Samples/b04_Conga_Lindrum_Color_A_Orig_19_Cs3.wav
        Resources/Samples/b04_Conga_Lindrum_Color_B_Orig_07_Cs2.wav
        Resources/Samples/b04_Conga_Lindrum_Color_B_Orig_19_Cs3.wav
        Resources/Samples/b04_Conga_Lindrum_Color_B_Orig_31_Cs4.wav
        Resources/Samples/b04_Conga_Lindrum_Color_B_Orig_43_Cs5.wav
        Resources/Samples/b04_Conga_Lindrum_Orig_43_Cs5.wav
        Resources/Samples/b04_Cowbell_Lindrum_Color_Pitched_36.wav
        Resources/Samples/b04_SD_Lindrum_Color_B_Decay_06.wav
        Resources/Samples/b04_SD_Lindrum_Color_B_Decay_12.wav
        Resources/Samples/b04_SS_Lindrum_Color_A_Orig_37.wav
        Resources/Samples/b04_Tamb_Lindrum_Color_Pitched_27.wav
        Resources/Samples/b05_BD_Lindrum_Color_A_Pitched_33.wav
        Resources/Samples/b05_BD_Lindrum_Color_D_Pitched_27.wav
        Resources/Samples/b05_Cabasa_Lindrum_Color_Pitched_29.wav
        Resources/Samples/b05_CH_Lindrum_Orig_Decay_02.wav
        Resources/Samples/b05_CH_Lindrum_Pitched_30.wav
        Resources/Samples/b05_Clap_Lindrum_Color_A_Pitched_29.wav
        Resources/Samples/b05_Conga_Lindrum_Orig_35_F4.wav
        Resources/Samples/b05_Conga_Lindrum_Orig_42_C5.wav
        Resources/Samples/b05_Cowbell_Lindrum_Pitched_33.wav
        Resources/Samples/b05_OH_Lindrum_Color_Pitched_30.wav
        Resources/Samples/b05_SD_Lindrum_Color_A_Orig_31.wav
        Resources/Samples/b05_SD_Lindrum_Color_B_Orig_36.wav
        Resources/Samples/b05_SS_Lindrum_Color_B_Orig_29.wav
        Resources/Samples/b05_Tamb_Lindrum_Pitched_34.wav
        Resources/Samples/b05_Tom_Lindrum_Color_B_Orig_22.wav
        Resources/Samples/b05_Tom_Lindrum_Color_B_Orig_34.wav
        Resources/Samples/b06_BD_Lindrum_Color_B_Pitched_12.wav
        Resources/Samples/b06_BD_Lindrum_Color_D_Pitched_23.wav
        Resources/Samples/b06_Cabasa_Lindrum_Pitched_08.wav
        Resources/Samples/b06_Cabasa_Lindrum_Pitched_14.wav
        Resources/Samples/b06_Clap_Lindrum_Color_A_Pitched_01.wav
        Resources/Samples/b06_Conga_Lindrum_Color_B_Orig_08_D2.wav
        Resources/Samples/b06_Conga_Lindrum_Color_B_Orig_09_Ds2.wav
        Resources/Samples/b06_Cowbell_Lindrum_Color_Pitched_03.wav
        Resources/Samples/b06_Crash_Lindrum_Color_Pitched_11.wav
        Resources/Samples/b06_OH_Lindrum_Color_Pitched_18.wav
        Resources/Samples/b06_Ride_Lindrum_Color_Pitched_08.wav
        Resources/Samples/b06_SD_Lindrum_Color_C_Orig_05.wav
        Resources/Samples/b06_SS_Lindrum_Orig_08.wav
        Resources/Samples/b06_Tamb_Lindrum_Pitched_04.wav
        Resources/Samples/b06_Tom_Lindrum_Color_A_Orig_01.wav
        Resources/Samples/b06_Tom_Lindrum_Color_A_Orig_06.wav
        Resources/Samples/b07_CH_Lindrum_Color_Orig_Decay_01.wav
        Resources/Samples/b07_CH_Lindrum_Color_Orig_Decay_03.wav
        Resources/Samples/b07_CH_Lindrum_Color_Orig_Decay_04.wav
        Resources/Samples/b07_CH_Lindrum_Color_Orig_Decay_06.wav
        Resources/Samples/b07_CH_Lindrum_Color_Orig_Decay_07.wav
        Resources/Samples/b07_CH_Lindrum_Color_Orig_Decay_10.wav
        Resources/Samples/b07_CH_Lindrum_Color_Orig_Decay_12.wav
        Resources/Samples/b07_CH_Lindrum_Orig_Decay_01.wav
        Resources/Samples/b07_CH_Lindrum_Orig_Decay_03.wav
        Resources/Samples/b07_CH_Lindrum_Orig_Decay_04.wav
        Resources/Samples/b07_CH_Lindrum_Orig_Decay_06.wav
        Resources/Samples/b07_CH_Lindrum_Orig_Decay_07.wav
        Resources/Samples/b07_CH_Lindrum_Orig_Decay_09.wav
        Resources/Samples/b07_CH_Lindrum_Orig_Decay_12.wav
        Resources/Samples/b07_OH_Lindrum_Color_Orig.wav
        Resources/Samples/b07_OH_Lindrum_Orig.wav
        Resources/Samples/b08_BD_Lindrum_Color_C_Orig.wav
        Resources/Samples/b08_BD_Lindrum_Color_C_Pitched_21.wav
        Resources/Samples/b08_Cabasa_Lindrum_Color_Pitched_14.wav
        Resources/Samples/b08_CH_Lindrum_Color_Orig_Decay_03.wav
        Resources/Samples/b08_CH_Lindrum_Color_Orig_Decay_07.wav
        Resources/Samples/b08_Clap_Lindrum_Color_A_Pitched_20.wav
        Resources/Samples/b08_Conga_Lindrum_Color_B_Orig_27_A3.wav
        Resources/Samples/b08_Conga_Lindrum_Color_B_Orig_39_A4.wav
        Resources/Samples/b08_Cowbell_Lindrum_Color_Pitched_24.wav
        Resources/Samples/b08_OH_Lindrum_Color_Orig.wav
        Resources/Samples/b08_SD_Lindrum_Color_C_Orig_01.wav
        Resources/Samples/b08_SD_Lindrum_Color_C_Orig_24.wav
        Resources/Samples/b08_SS_Lindrum_Color_B_Orig_01.wav
        Resources/Samples/b08_Tamb_Lindrum_Color_Pitched_13.wav
        Resources/Samples/b08_Tom_Lindrum_Color_A_Orig_24.wav
        Resources/Samples/b08_Tom_Lindrum_Color_B_Orig_01.wav
        Resources/Samples/b09_BD_Lindrum_Color_B_Pitched_24.wav
        Resources/Samples/b09_Cabasa_Lindrum_Color_Pitched_35.wav
        Resources/Samples/b09_CH_Lindrum_Color_Orig_Decay_01.wav
        Resources/Samples/b09_CH_Lindrum_Color_Orig_Decay_04.wav
        Resources/Samples/b09_CH_Lindrum_Color_Pitched_29.wav
        Resources/Samples/b09_Clap_Lindrum_Color_A_Decay_05.wav
        Resources/Samples/b09_Clap_Lindrum_Color_B_Decay_03.wav
        Resources/Samples/b09_Conga_Lindrum_Color_A_Orig_40_As4.wav
        Resources/Samples/b09_Conga_Lindrum_Color_A_Orig_42_C5.wav
        Resources/Samples/b09_Conga_Lindrum_Color_A_Orig_44_D5.wav
        Resources/Samples/b09_Ride_Lindrum_Color_Pitched_25.wav
        Resources/Samples/b09_SD_Lindrum_Color_B_Decay_03.wav
        Resources/Samples/b09_SD_Lindrum_Color_B_Decay_09.wav
        Resources/Samples/b09_SS_Lindrum_Color_A_Decay_02.wav
        Resources/Samples/b09_Tom_Lindrum_Color_A_Orig_27.wav
        Resources/Samples/b09_Tom_Lindrum_Color_A_Orig_33.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_18_C3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_19_Cs3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_20_D3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_21_Ds3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_22_E3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_23_F3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_24_Fs3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_25_G3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_26_Gs3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_27_A3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_28_As3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_29_B3.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_30_C4.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_31_Cs4.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_32_D4.wav
        Resources/Samples/b10_Conga_Lindrum_Orig_33_Ds4.wav
)

target_link_libraries(AxelF PRIVATE AxelFData)

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
        JUCE_PLUGINHOST_VST3=1
)
