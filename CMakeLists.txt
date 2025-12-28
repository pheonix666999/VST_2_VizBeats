cmake_minimum_required(VERSION 3.22)

project(VizBeats VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  find_program(GCC_AR gcc-ar)
  find_program(GCC_RANLIB gcc-ranlib)

  if (GCC_AR AND GCC_RANLIB)
    set(CMAKE_AR "${GCC_AR}" CACHE FILEPATH "GNU archiver wrapper" FORCE)
    set(CMAKE_RANLIB "${GCC_RANLIB}" CACHE FILEPATH "GNU ranlib wrapper" FORCE)
  endif()
endif()

set(JUCE_DIR "" CACHE PATH "Path to JUCE source directory (optional). If empty, JUCE will be fetched from GitHub.")

include(FetchContent)

if (JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(STATUS "Using local JUCE: ${JUCE_DIR}")
  add_subdirectory("${JUCE_DIR}" juce)
else()
  message(STATUS "Fetching JUCE from GitHub (set JUCE_DIR to use a local checkout)")
  FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.11
  )
  FetchContent_MakeAvailable(juce)
endif()

set(VIZBEATS_PLUGIN_FORMATS VST3)
if (APPLE)
  list(APPEND VIZBEATS_PLUGIN_FORMATS AU)
endif()

juce_add_plugin(VizBeats
  COMPANY_NAME "VizBeats"
  PRODUCT_NAME "VizBeats"
  BUNDLE_ID "com.vizbeats.vizbeats"
  PLUGIN_MANUFACTURER_CODE VzBt
  PLUGIN_CODE Vzbp
  FORMATS ${VIZBEATS_PLUGIN_FORMATS}

  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE

  # Plugin description
  DESCRIPTION "Visual metronome plugin with beat synchronization"
)

target_sources(VizBeats PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
)

juce_generate_juce_header(VizBeats)

target_compile_definitions(VizBeats PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VizBeats PRIVATE
  juce::juce_audio_processors
  juce::juce_audio_devices
  juce::juce_gui_basics
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
)

juce_add_gui_app(VizBeatsStandalone
  COMPANY_NAME "VizBeats"
  PRODUCT_NAME "VizBeatsStandalone"
)

target_sources(VizBeatsStandalone PRIVATE
  Source/StandaloneApp.cpp
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
)

juce_generate_juce_header(VizBeatsStandalone)

target_compile_definitions(VizBeatsStandalone PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

target_link_libraries(VizBeatsStandalone PRIVATE
  juce::juce_audio_processors
  juce::juce_audio_devices
  juce::juce_audio_utils
  juce::juce_gui_basics
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
)
