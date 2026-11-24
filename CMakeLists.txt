cmake_minimum_required(VERSION 3.22)

# Project
project(LOERIC_SL_LINK VERSION 0.0.1)

# JUCE
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Modules/JUCE)

# FluidLite
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Modules/FluidLite)
include(${CMAKE_CURRENT_SOURCE_DIR}/Modules/FluidLite/aliases.cmake)

# fatar_sl_link (by FATAR/Studiologic)
juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/Modules/fatar_sl_link)

# LOERIC Smart Keyboard
juce_add_console_app(LoericSmartKeyboard
    PRODUCT_NAME "LOERIC Smart Keyboard"
    MANUFACTURER_NAME "gregogiudici"
)

# Generate JuceHeader.h
juce_generate_juce_header(LoericSmartKeyboard)

# Add sources
target_sources(LoericSmartKeyboard
    PRIVATE
    Source/Main.cpp
    Source/Soundfont/SoundfontAudioProcessor.cpp
    Source/SL/CustomPages.cpp
)

# Add compile definitions
target_compile_definitions(LoericSmartKeyboard
    PUBLIC
    # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
    JUCE_WEB_BROWSER=0 # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0 # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Add binary data
juce_add_binary_data(AudioPluginData SOURCES
    # Soundfonts
    "Assets/Soundfonts/Full Grand Piano.sf2"
)

# Link libraries
target_link_libraries(LoericSmartKeyboard
    PRIVATE
    AudioPluginData
    juce::juce_audio_utils
    juce::juce_dsp
    fluidlite::fluidlite-static
    fatar_sl_link

    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)