---
name: cmake
description: "CMake build configuration for the AxelF JUCE project. Use when: modifying CMakeLists.txt, adding new source files or modules, linking third-party DSP libraries, configuring platform-specific builds, or setting up VST3/AU/Standalone targets. Covers juce_add_plugin conventions, module dependencies, and resource embedding."
---

# CMake — AxelF Workstation

## Root CMakeLists.txt Structure

```cmake
cmake_minimum_required(VERSION 3.22)
project(AxelF VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE ──────────────────────────────────────────────────────
# JUCE added as a subdirectory (Git submodule at extern/JUCE)
add_subdirectory(extern/JUCE)

# ── Plugin Target ─────────────────────────────────────────────
juce_add_plugin(AxelF
    PRODUCT_NAME "AxelF"
    COMPANY_NAME "AxelF"
    PLUGIN_MANUFACTURER_CODE AXLF
    PLUGIN_CODE Axlf
    FORMATS VST3 AU Standalone        # build targets
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE     # set TRUE for local dev
)

# ── Source Files ──────────────────────────────────────────────
target_sources(AxelF
    PRIVATE
        # Top-level
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp

        # Common
        Source/Common/MidiRouter.cpp
        Source/Common/MasterMixer.cpp
        Source/Common/ModulationMatrix.cpp
        Source/Common/PresetManager.cpp

        # Common DSP
        Source/Common/DSP/LadderFilter.cpp
        Source/Common/DSP/IR3109Filter.cpp
        Source/Common/DSP/ADSREnvelope.cpp
        Source/Common/DSP/LFOGenerator.cpp
        Source/Common/DSP/Oversampler.cpp

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
        Source/Modules/LinnDrum/LinnDrumSequencer.cpp
        Source/Modules/LinnDrum/LinnDrumParams.cpp
)

# ── Include Paths ─────────────────────────────────────────────
target_include_directories(AxelF
    PRIVATE
        Source
        Source/Common
        Source/Common/DSP
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

# ── Compile Definitions ──────────────────────────────────────
target_compile_definitions(AxelF
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ── Binary Resources (LinnDrum samples, factory presets) ─────
juce_add_binary_data(AxelFData
    SOURCES
        Resources/Samples/kick.wav
        Resources/Samples/snare.wav
        Resources/Samples/hihat_closed.wav
        Resources/Samples/hihat_open.wav
        Resources/Samples/ride.wav
        Resources/Samples/crash.wav
        Resources/Samples/tom_hi.wav
        Resources/Samples/tom_mid.wav
        Resources/Samples/tom_lo.wav
        Resources/Samples/conga_hi.wav
        Resources/Samples/conga_lo.wav
        Resources/Samples/clap.wav
        Resources/Samples/cowbell.wav
        Resources/Samples/tambourine.wav
        Resources/Samples/cabasa.wav
)

target_link_libraries(AxelF PRIVATE AxelFData)
```

## Adding New Source Files

When creating a new `.h/.cpp` file, **always add the `.cpp` to `target_sources`**:

```cmake
# In the relevant section of target_sources
Source/Modules/Jupiter8/Jupiter8NewFeature.cpp
```

Headers don't need to be listed (they're found via include paths), but listing them is optional for IDE integration.

## Adding Third-Party DSP Libraries

```cmake
# Option 1: Header-only library (e.g., a filter reference implementation)
add_subdirectory(extern/SomeFilterLib)
target_link_libraries(AxelF PRIVATE SomeFilterLib)

# Option 2: Single-header library — just add to include path
target_include_directories(AxelF PRIVATE extern/single-header-libs)

# Option 3: FetchContent for Git-hosted dependencies
include(FetchContent)
FetchContent_Declare(
    SomeDSPLib
    GIT_REPOSITORY https://github.com/example/some-dsp-lib.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(SomeDSPLib)
target_link_libraries(AxelF PRIVATE SomeDSPLib)
```

## Platform-Specific Configuration

```cmake
# macOS: AU format + codesigning
if(APPLE)
    set_target_properties(AxelF PROPERTIES
        BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "AxelF"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )
endif()

# Windows: static runtime for portable builds
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(AxelF PRIVATE /W4)
endif()

# Linux: ALSA/JACK
if(UNIX AND NOT APPLE)
    find_package(ALSA REQUIRED)
    target_link_libraries(AxelF PRIVATE ${ALSA_LIBRARIES})
endif()
```

## Build Commands

```bash
# Configure (from project root)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Build debug
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug

# Clean rebuild
cmake --build build --clean-first
```

## Key CMake Rules

1. **Always use `target_*` commands** — never bare `include_directories()`, `link_libraries()`, or `add_definitions()`.
2. **JUCE modules are linked via `juce::juce_*`** — don't manually add JUCE headers to include paths.
3. **Binary resources** (samples, presets) go through `juce_add_binary_data` — accessible in code via `BinaryData::` namespace.
4. **C++17 minimum** — required for structured bindings, `std::optional`, `if constexpr`.
5. **LTO flags** via `juce::juce_recommended_lto_flags` — enables link-time optimization in Release.
6. **Keep `COPY_PLUGIN_AFTER_BUILD` off in CI** — set to `TRUE` only for local development convenience.
