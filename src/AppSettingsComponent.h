#pragma once

#include <JuceHeader.h>

#include "TapeEngine.h"

#include <array>

class AppSettingsComponent : public juce::Component,
                             private juce::ChangeListener
{
public:
    AppSettingsComponent(juce::AudioDeviceManager& audioDeviceManagerToUse, TapeEngine& engineToUse);
    ~AppSettingsComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ColourButton : public juce::Button
    {
    public:
        ColourButton();

        void setButtonColour(juce::Colour newColour);
        juce::Colour getButtonColour() const noexcept;
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    private:
        juce::Colour colour { juce::Colours::white };
    };

    class ToggleButton : public juce::Button
    {
    public:
        explicit ToggleButton(const juce::String& textToUse);

        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    private:
        juce::String text;
    };

    juce::AudioDeviceManager& audioDeviceManager;
    TapeEngine& engine;
    std::array<ColourButton, TapeEngine::numTracks> trackColourButtons;
    juce::Label bpmEditor;
    juce::Label beatsPerBarEditor;
    ToggleButton metronomeDefaultButton { "Metronome On Launch" };
    ToggleButton countInDefaultButton { "Count-In Enabled" };
    juce::ComboBox exportFormatBox;
    juce::ComboBox exportSampleRateBox;
    juce::ComboBox exportBitDepthBox;
    juce::ComboBox exportTailBox;
    juce::ComboBox inputDeviceBox;
    juce::ComboBox outputDeviceBox;
    juce::ComboBox sampleRateBox;
    juce::ComboBox bufferSizeBox;
    juce::Component::SafePointer<juce::ColourSelector> activeColourSelector;
    int activeColourTrackIndex = -1;
    bool refreshingAudioControls = false;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void configureLabelEditor(juce::Label& editor, int maxCharacters);
    void configureComboBox(juce::ComboBox& box);
    void configureToggleButton(ToggleButton& button);
    void refreshFromSettings();
    void refreshAudioOptions();
    void applyAudioSetup();
    void openColourSelector(int trackIndex);
    juce::Rectangle<int> getContentBounds() const;
};
