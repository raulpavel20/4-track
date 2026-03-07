#pragma once

#include <JuceHeader.h>

#include <array>

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
    class CloseButton : public juce::Button
    {
    public:
        CloseButton();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

    class ContentComponent : public juce::Component
    {
    public:
        explicit ContentComponent(TrackControlChain& ownerToUse);
        void paint(juce::Graphics& g) override;

    private:
        TrackControlChain& owner;
    };

    TapeEngine& engine;
    int selectedTrack = 0;
    juce::StringArray inputOptions;
    juce::Viewport modulesViewport;
    ContentComponent contentComponent;
    juce::ComboBox inputSourceBox;
    juce::Slider gainSlider;
    juce::TextButton addModuleButton { "+" };
    std::array<juce::Slider, Track::maxFilterModules> filterSliders;
    std::array<CloseButton, Track::maxFilterModules> removeButtons;

    void paintContent(juce::Graphics& g);
    void refreshFromEngine();
    void updateSliderColours();
    void updateModuleVisibility();
    void layoutContent();
    int getActiveFilterModuleCount() const noexcept;
    int getActiveFilterSlot(int visibleIndex) const noexcept;
    int getVisibleIndexForSlot(int slot) const noexcept;
    juce::Rectangle<int> getFrameBounds() const;
    juce::Rectangle<int> getViewportBounds() const;
    juce::Rectangle<int> getInputModuleBounds() const;
    juce::Rectangle<int> getFilterModuleBounds(int visibleIndex) const;
    juce::Rectangle<int> getAddButtonBounds() const;
};
