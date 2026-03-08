#pragma once

#include <JuceHeader.h>

#include "../../../TapeEngine.h"

#include <array>

class UserSettingsSection : public juce::Component,
                            private juce::ChangeListener
{
public:
    explicit UserSettingsSection(TapeEngine& engineToUse);
    ~UserSettingsSection() override;

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

    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void configureLabelEditor(juce::Label& editor, int maxCharacters);
    void configureToggleButton(ToggleButton& button);
    void refreshFromSettings();
    void openColourSelector(int trackIndex);

    TapeEngine& engine;
    std::array<ColourButton, TapeEngine::numTracks> trackColourButtons;
    juce::Label bpmEditor;
    juce::Label beatsPerBarEditor;
    ToggleButton metronomeDefaultButton { "Metronome On Launch" };
    ToggleButton countInDefaultButton { "Count-In Enabled" };
    juce::Component::SafePointer<juce::ColourSelector> activeColourSelector;
    int activeColourTrackIndex = -1;
};
