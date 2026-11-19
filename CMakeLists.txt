cmake_minimum_required(VERSION 3.22)

project(Acid303 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

option(ACID_BUILD_PLUGIN "Build the VST3/AU/Standalone plugin" ON)
option(ACID_BUILD_TESTS  "Build the headless unit tests"      ON)
option(ACID_BUILD_TOOLS  "Build the offline render tool"      ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Pin JUCE to an exact tag. Override with -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE for offline builds.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.17-security
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# acid_core: the GUI-free engine (DSP, sequencer, generator, pattern model).
# It is an INTERFACE library so every consumer compiles the sources together
# with its own copy of the JUCE modules (JUCE modules can't be linked into two
# static libraries that end up in the same binary).
# Core code must only depend on the non-GUI modules listed in ACID_CORE_MODULES;
# the test target enforces this because it links nothing else.
# ---------------------------------------------------------------------------
set(ACID_CORE_MODULES
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_dsp)

add_library(acid_core INTERFACE)
target_sources(acid_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/core/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/core/ParamSpec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/seq/Sequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/seq/Generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/seq/Pattern.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/seq/PatternController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/seq/MidiExport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/seq/World.cpp)
target_include_directories(acid_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_definitions(acid_core INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
if(ACID_BUILD_PLUGIN)
    if(APPLE)
        set(ACID_FORMATS AU VST3 Standalone)
    else()
        set(ACID_FORMATS VST3 Standalone)
    endif()

    juce_add_plugin(Acid303
        PRODUCT_NAME               "Rungs"
        COMPANY_NAME               "Rungs"
        BUNDLE_ID                  "com.acid303.acid303"
        PLUGIN_MANUFACTURER_CODE   Acdl
        PLUGIN_CODE                Ac3d
        FORMATS                    ${ACID_FORMATS}
        IS_SYNTH                   TRUE
        NEEDS_MIDI_INPUT           TRUE
        NEEDS_MIDI_OUTPUT          FALSE
        IS_MIDI_EFFECT             FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES            Instrument Synth
        AU_MAIN_TYPE               kAudioUnitType_MusicDevice
        COPY_PLUGIN_AFTER_BUILD    FALSE)

    target_sources(Acid303 PRIVATE
        Source/plugin/PluginProcessor.cpp
        Source/plugin/PluginEditor.cpp
        Source/ui/Theme.cpp
        Source/ui/RungsLookAndFeel.cpp
        Source/ui/PianoRoll.cpp
        Source/ui/GeneratorPanel.cpp
        Source/ui/HeaderBar.cpp
        Source/ui/ParamPanels.cpp
        Source/ui/WorldScenes.cpp
        Source/ui/WorldView.cpp
        Source/ui/MainView.cpp)

    # pixel fonts (SIL Open Font License, see Resources/Fonts/OFL-*.txt)
    juce_add_binary_data(RungsAssets SOURCES
        Resources/Fonts/PressStart2P-Regular.ttf
        Resources/Fonts/VT323-Regular.ttf)

    target_compile_definitions(Acid303 PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(Acid303
        PRIVATE
            acid_core
            RungsAssets
            ${ACID_CORE_MODULES}
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Offline render tool: renders a generated pattern to a WAV file.
# ---------------------------------------------------------------------------
if(ACID_BUILD_TOOLS)
    juce_add_console_app(acid_render PRODUCT_NAME "acid_render")
    target_sources(acid_render PRIVATE tools/RenderDemo.cpp)
    target_link_libraries(acid_render PRIVATE
        acid_core ${ACID_CORE_MODULES} juce::juce_audio_formats
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ACID_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    juce_add_console_app(acid_tests PRODUCT_NAME "acid_tests")
    target_sources(acid_tests PRIVATE
        Tests/TestDsp.cpp
        Tests/TestSequencer.cpp
        Tests/TestGenerator.cpp
        Tests/TestPatternController.cpp
        Tests/TestEngine.cpp
        Tests/TestWorlds.cpp)
    target_link_libraries(acid_tests PRIVATE
        acid_core ${ACID_CORE_MODULES} Catch2::Catch2WithMain
        juce::juce_recommended_config_flags)

    enable_testing()
    add_test(NAME acid_tests COMMAND acid_tests)
endif()
