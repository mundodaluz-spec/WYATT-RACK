cmake_minimum_required (VERSION 3.22)
project (WYATT_RACK VERSION 1.0.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
    set (CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

# JUCE: pasta ao lado (../JUCE) ou baixada pelo CMake
set (JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Pasta do JUCE 8.0.4")
if (EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory ("${JUCE_DIR}" JUCE)
else()
    include (FetchContent)
    FetchContent_Declare (JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
    FetchContent_MakeAvailable (JUCE)
endif()

set (FORMATS VST3 Standalone)
if (APPLE)
    list (APPEND FORMATS AU)
endif()

juce_add_plugin (WyattRack
    PRODUCT_NAME "WYATT RACK"
    COMPANY_NAME "WYATT Sounds"
    BUNDLE_ID "com.wyattsounds.wyattrack"
    PLUGIN_MANUFACTURER_CODE Wytt
    PLUGIN_CODE WyRk
    FORMATS ${FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    DESCRIPTION "Descreva o som em portugues e o rack monta a cadeia de efeitos")

juce_add_binary_data (WyattRackData SOURCES
    Resources/Inter-Regular.ttf
    Resources/Inter-Bold.ttf
    Resources/Inter-Black.ttf)

set (CORE_SOURCES
    Source/Engine/Catalog.cpp
    Source/Engine/Blocks.cpp
    Source/Engine/Rack.cpp
    Source/Brain/Treatment.cpp
    Source/Brain/LocalBrain.cpp
    Source/Brain/Analysis.cpp)

target_sources (WyattRack PRIVATE
    ${CORE_SOURCES}
    Source/Brain/CloudBrain.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions (WyattRack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries (WyattRack
    PRIVATE WyattRackData
            juce::juce_audio_utils
            juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- testes
option (WYATT_RACK_TESTS "Compilar os testes" ON)
if (WYATT_RACK_TESTS)
    add_executable (CoreTests Tests/CoreTests.cpp ${CORE_SOURCES})

    juce_add_console_app (PluginTests PRODUCT_NAME "WYATT RACK Testes")
    target_sources (PluginTests PRIVATE Tests/PluginTests.cpp)
    target_compile_definitions (PluginTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=1 JUCE_LOAD_CURL_SYMBOLS_LAZILY=1 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries (PluginTests PRIVATE WyattRack WyattRackData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
