cmake_minimum_required(VERSION 3.22)
project(VUMisterNAM VERSION 1.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# NeuralAudio provides the actual NAM inference engine. Keep its optional tools off
# so the plugin build only compiles the library and its required dependencies.
set(BUILD_UTILS OFF CACHE BOOL "Disable NeuralAudio utilities" FORCE)
# This NAM is version 0.7.0 architecture SlimmableContainer (A2).
# NeuralAudio documents that non-standard A2 models require NAM Core support.
set(BUILD_NAMCORE ON CACHE BOOL "Enable NAM Core for NAM A2/SlimmableContainer models" FORCE)
set(BUILD_STATIC_RTNEURAL OFF CACHE BOOL "Disable optional static RTNeural models" FORCE)
FetchContent_Declare(NeuralAudio
    GIT_REPOSITORY https://github.com/mikeoliphant/NeuralAudio.git
    GIT_TAG main
    GIT_SHALLOW FALSE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(NeuralAudio)

juce_add_plugin(VUMisterNAM
    COMPANY_NAME "VUMister"
    BUNDLE_ID "com.vumister.nam"
    PLUGIN_MANUFACTURER_CODE Vumi
    PLUGIN_CODE VuNm
    FORMATS VST3 Standalone
    PRODUCT_NAME "VU Mister NAM"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(VUMisterNAM)

# Embed the user-provided reference artwork in the plug-in so no external PNGs are required.
juce_add_binary_data(VUMisterAssets SOURCES
    Assets/reference_full_interface.png
    Assets/Separated/00_Full_Image.png
    Assets/Separated/01_Input_VU_Meter.png
    Assets/Separated/02_Center_Logo_and_Thai_Text.png
    Assets/Separated/03_Output_VU_Meter.png
    Assets/Separated/04_Input_Knob.png
    Assets/Separated/05_Mix_Knob.png
    Assets/Separated/06_Output_Knob.png
    Assets/Separated/07_Bypass_Switch.png
    Assets/Separated/08_Model_Selector.png
    Assets/Separated/09_Load_NAM_Button.png
    Assets/Separated/10_Top_Title_and_Preset.png
    Assets/Separated/11_Bottom_Status_and_Model_Row.png
)

target_sources(VUMisterNAM PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(VUMisterNAM PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VUMisterNAM PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    NeuralAudio
    VUMisterAssets
)
