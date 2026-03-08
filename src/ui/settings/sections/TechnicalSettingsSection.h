#pragma once

#include <JuceHeader.h>

class TechnicalSettingsSection : public juce::Component,
                                 private juce::ChangeListener
{
public:
    explicit TechnicalSettingsSection(juce::AudioDeviceManager& audioDeviceManagerToUse);
    ~TechnicalSettingsSection() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void configureComboBox(juce::ComboBox& box);
    void refreshAudioOptions();
    void applyAudioSetup();

    juce::AudioDeviceManager& audioDeviceManager;
    juce::ComboBox inputDeviceBox;
    juce::ComboBox outputDeviceBox;
    juce::ComboBox sampleRateBox;
    juce::ComboBox bufferSizeBox;
    bool refreshingAudioControls = false;
};
