#pragma once

#include <JuceHeader.h>

#include <array>

class AppSettings : public juce::ChangeBroadcaster
{
public:
    struct ExportDefaults
    {
        int formatId = 1;
        double sampleRate = 44100.0;
        int bitDepth = 24;
        double tailSeconds = 2.0;
    };

    static AppSettings& getInstance();

    void initialise();
    void shutdown();

    juce::Colour getTrackColour(int trackIndex) const;
    void setTrackColour(int trackIndex, juce::Colour colour);

    int getDefaultBpm() const;
    void setDefaultBpm(int bpm);

    int getDefaultBeatsPerBar() const;
    void setDefaultBeatsPerBar(int beatsPerBar);

    bool getDefaultMetronomeEnabled() const;
    void setDefaultMetronomeEnabled(bool shouldBeEnabled);

    bool getDefaultCountInEnabled() const;
    void setDefaultCountInEnabled(bool shouldBeEnabled);

    ExportDefaults getExportDefaults() const;
    void setExportDefaults(const ExportDefaults& defaults);

    std::unique_ptr<juce::XmlElement> createAudioDeviceState() const;
    void setAudioDeviceState(const juce::XmlElement* state);

private:
    AppSettings() = default;

    juce::PropertiesFile* getProperties() const;
    void setValue(const juce::String& key, const juce::var& value);

    mutable juce::ApplicationProperties applicationProperties;
    bool initialised = false;
};
