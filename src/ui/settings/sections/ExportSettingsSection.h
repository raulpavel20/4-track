#pragma once

#include <JuceHeader.h>

class ExportSettingsSection : public juce::Component,
                              private juce::ChangeListener
{
public:
    ExportSettingsSection();
    ~ExportSettingsSection() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void configureComboBox(juce::ComboBox& box);
    void refreshFromSettings();
    void persistDefaults();

    juce::ComboBox exportFormatBox;
    juce::ComboBox exportSampleRateBox;
    juce::ComboBox exportBitDepthBox;
    juce::ComboBox exportTailBox;
};
