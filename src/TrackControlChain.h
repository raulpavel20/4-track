#pragma once

#include <JuceHeader.h>

#include <array>

#include "TapeEngine.h"

class TrackControlChain : public juce::Component,
                          private juce::Timer
{
public:
    explicit TrackControlChain(TapeEngine& engineToUse);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelectedTrack(int trackIndex);
    int getSelectedTrack() const noexcept;
    void setInputOptions(const juce::StringArray& options);

private:
    static constexpr int compressorControlCount = 5;

    class CloseButton : public juce::Button
    {
    public:
        CloseButton();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

    class SaturationModeButton : public juce::Button
    {
    public:
        SaturationModeButton();
        void setAccentColour(juce::Colour newAccentColour);
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    private:
        juce::Colour accentColour { juce::Colours::white };
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
    juce::Slider inputGainSlider;
    juce::TextButton addModuleButton { "+" };
    std::array<CloseButton, Track::maxChainModules> removeButtons;
    std::array<juce::Slider, Track::maxChainModules> filterSliders;
    std::array<std::array<juce::Slider, Track::maxEqBands>, Track::maxChainModules> eqGainSliders;
    std::array<std::array<juce::Slider, Track::maxEqBands>, Track::maxChainModules> eqQSliders;
    std::array<std::array<juce::TextEditor, Track::maxEqBands>, Track::maxChainModules> eqFrequencyEditors;
    std::array<std::array<juce::Slider, compressorControlCount>, Track::maxChainModules> compressorSliders;
    std::array<SaturationModeButton, Track::maxChainModules> saturationModeButtons;
    std::array<juce::Slider, Track::maxChainModules> saturationAmountSliders;
    std::array<juce::Slider, Track::maxChainModules> gainModuleSliders;
    std::array<int, Track::maxChainModules> visibleSlots {};
    std::array<ChainModuleType, Track::maxChainModules> visibleTypes {};
    std::array<juce::Rectangle<int>, Track::maxChainModules> moduleBounds {};
    int visibleModuleCount = 0;

    void timerCallback() override;
    void paintContent(juce::Graphics& g);
    void refreshFromEngine();
    void updateAccentColours();
    void updateVisibleModules();
    void updateModuleVisibility();
    void layoutContent();
    void syncFrequencyEditorsFromEngine(bool force);
    juce::Rectangle<int> getFrameBounds() const;
    juce::Rectangle<int> getViewportBounds() const;
    juce::Rectangle<int> getInputModuleBounds() const;
    juce::Rectangle<int> getAddButtonBounds() const;
};
