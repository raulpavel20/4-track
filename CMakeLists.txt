cmake_minimum_required(VERSION 3.22)

project(FourTrackTape VERSION 26.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(external/JUCE)

juce_add_binary_data(FourTrackAssets
    HEADER_NAME FourTrackBinaryData.h
    SOURCES
        assets/Manrope-VariableFont_wght.ttf
)

juce_add_gui_app(FourTrackTape
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "4-Track needs microphone access to record audio onto the virtual tape."
    PRODUCT_NAME "4-Track"
)

target_sources(FourTrackTape
    PRIVATE
        src/main.cpp
        src/AppSettings.cpp
        src/AppSettingsComponent.cpp
        src/MainComponent.cpp
        src/TapeView.cpp
        src/TrackControlChain.cpp
        src/TrackMixerPanel.cpp
        src/audio/AudioDeviceController.cpp
        src/audio/engine/TapeEngineAudioCallback.cpp
        src/audio/engine/TapeEngineAudioDevice.cpp
        src/audio/engine/TapeEngineMixerExport.cpp
        src/audio/engine/TapeEngineModules.cpp
        src/audio/engine/TapeEngineRouting.cpp
        src/audio/engine/TapeEngineTransport.cpp
        src/ui/settings/sections/ExportSettingsSection.cpp
        src/ui/settings/sections/TechnicalSettingsSection.cpp
        src/ui/settings/sections/UserSettingsSection.cpp
        src/ui/mixer/TrackMixerPanelLayout.cpp
        src/ui/mixer/TrackMixerPanelPaint.cpp
        src/ui/pre_tape/TrackControlChainLayout.cpp
        src/ui/pre_tape/TrackControlChainInputModule.cpp
        src/ui/pre_tape/TrackControlChainFilterModule.cpp
        src/ui/pre_tape/TrackControlChainEqModule.cpp
        src/ui/pre_tape/TrackControlChainCompressorModule.cpp
        src/ui/pre_tape/TrackControlChainSaturationModule.cpp
        src/ui/pre_tape/TrackControlChainGainModule.cpp
        src/ui/pre_tape/TrackControlChainPaint.cpp
        src/ui/pre_tape/TrackControlChainWidgets.cpp
        src/ui/tape/TapeViewGeometry.cpp
        src/ui/tape/TapeViewInteraction.cpp
        src/ui/tape/TapeViewPaint.cpp
)

target_include_directories(FourTrackTape
    PRIVATE
        src
)

target_compile_definitions(FourTrackTape
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(FourTrackTape
    PRIVATE
        FourTrackAssets
        juce::juce_audio_utils
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_generate_juce_header(FourTrackTape)
