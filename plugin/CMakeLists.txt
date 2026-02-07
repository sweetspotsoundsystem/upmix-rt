cmake_minimum_required(VERSION 3.22)

project(AudioPlugin VERSION 0.1.0)

set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/UpmixRT")

juce_add_plugin(
  ${PROJECT_NAME}
  COMPANY_NAME
  "Sweet Spot"
  IS_SYNTH
  FALSE
  NEEDS_MIDI_INPUT
  FALSE
  NEEDS_MIDI_OUTPUT
  FALSE
  PLUGIN_MANUFACTURER_CODE
  Swee
  PLUGIN_CODE
  Upmx
  FORMATS
  Standalone
  VST3
  AU
  PRODUCT_NAME
  "UpmixRT"
  BUNDLE_ID
  "com.sweetspot.UpmixRT"
)

# Binary data for logo asset
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
if(EXISTS "${ASSETS_DIR}/logo.png")
  juce_add_binary_data(PluginAssets SOURCES "${ASSETS_DIR}/logo.png")
  set(HAS_LOGO_ASSET TRUE)
else()
  message(STATUS "Logo not found at ${ASSETS_DIR}/logo.png - building without logo")
  set(HAS_LOGO_ASSET FALSE)
endif()

set(SOURCE_FILES
  source/PluginEditor.cpp
  source/PluginProcessor.cpp
  source/FilterBank.cpp
  source/AnalysisBand.cpp
  source/SpatialAnalyzer.cpp
  source/AmbisonicEncoder.cpp
  source/Decorrelator.cpp
  source/HeightEstimator.cpp
  source/AmbisonicDecoder.cpp
  source/OutputWriter.cpp
)

set(HEADER_FILES
  ${INCLUDE_DIR}/Constants.h
  ${INCLUDE_DIR}/FilterBank.h
  ${INCLUDE_DIR}/AnalysisBand.h
  ${INCLUDE_DIR}/SpatialAnalyzer.h
  ${INCLUDE_DIR}/AmbisonicEncoder.h
  ${INCLUDE_DIR}/Decorrelator.h
  ${INCLUDE_DIR}/HeightEstimator.h
  ${INCLUDE_DIR}/AmbisonicDecoder.h
  ${INCLUDE_DIR}/SpeakerLayout.h
  ${INCLUDE_DIR}/OutputWriter.h
  ${INCLUDE_DIR}/PluginProcessor.h
  ${INCLUDE_DIR}/PluginEditor.h
)

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries_system(${PROJECT_NAME} PUBLIC juce::juce_audio_utils juce::juce_dsp)
target_link_libraries(
  ${PROJECT_NAME} PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
                         juce::juce_recommended_warning_flags
)

target_compile_definitions(${PROJECT_NAME} PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

# Link logo binary data if available
if(HAS_LOGO_ASSET)
  target_link_libraries(${PROJECT_NAME} PRIVATE PluginAssets)
  target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_LOGO_ASSET=1)
endif()

# Enables strict C++ warnings and treats warnings as errors.
set_source_files_properties(${SOURCE_FILES} PROPERTIES COMPILE_OPTIONS "${PROJECT_WARNINGS_CXX}")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/..)
