#pragma once

#include <JuceHeader.h>

#include "TapeEngine.h"

class TrackControlChain : public juce::Component
{
public:
    explicit TrackControlChain(TapeEngine& engineToUse);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelectedTrack(int trackIndex);
    int getSelectedTrack() const noexcept;
    void setInputOptions(const juce::StringArray& options);

private:
    TapeEngine& engine;
    int selectedTrack = 0;
    juce::ComboBox inputSourceBox;
    juce::Slider gainSlider;
    juce::Slider filterSlider;

    juce::Rectangle<int> getFrameBounds() const;
    juce::Rectangle<int> getContentBounds() const;
    juce::Rectangle<int> getInputModuleBounds() const;
    juce::Rectangle<int> getFilterModuleBounds() const;
};
