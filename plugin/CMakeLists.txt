include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

file(GLOB VONOX_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.jpg)
juce_add_binary_data(VonoxAssets SOURCES ${VONOX_ASSETS})
set_target_properties(VonoxAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(VONOX_FORMATS VST3)
if(VONOX_BUILD_STANDALONE)
  list(APPEND VONOX_FORMATS Standalone)
endif()

juce_add_plugin(VONOX
  COMPANY_NAME "VONOX"
  PLUGIN_MANUFACTURER_CODE Vnox
  PLUGIN_CODE Vnx1
  FORMATS ${VONOX_FORMATS}
  PRODUCT_NAME "VONOX"
  IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
  VST3_CATEGORIES Fx
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(VONOX PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(VONOX PRIVATE Source)
target_link_libraries(VONOX PRIVATE vonox_dsp VonoxAssets
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
target_compile_definitions(VONOX PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

if(VONOX_BUILD_TESTS)
  juce_add_console_app(vonox_plugin_smoke PRODUCT_NAME "vonox_plugin_smoke")
  target_sources(vonox_plugin_smoke PRIVATE tests/plugin_smoke.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_include_directories(vonox_plugin_smoke PRIVATE Source)
  target_link_libraries(vonox_plugin_smoke PRIVATE vonox_dsp VonoxAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
  target_compile_definitions(vonox_plugin_smoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
  add_test(NAME plugin_smoke COMMAND vonox_plugin_smoke)
endif()
