cmake_minimum_required(VERSION 3.22)

project(FourTrackTape VERSION 0.1.0)

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
    MICROPHONE_PERMISSION_TEXT "4-Track Tape needs microphone access to record audio onto the virtual tape."
    PRODUCT_NAME "4-Track Tape"
)

target_sources(FourTrackTape
    PRIVATE
        src/main.cpp
        src/MainComponent.cpp
        src/TapeEngine.cpp
        src/TapeView.cpp
        src/TrackControlChain.cpp
        src/TrackMixerPanel.cpp
        src/TransportControls.cpp
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
