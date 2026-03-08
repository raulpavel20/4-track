#pragma once

#include <JuceHeader.h>

#include <array>

#include "TapeEngine.h"

class TrackControlChain : public juce::Component,
                          private juce::Timer,
                          private juce::ChangeListener
{
public:
    explicit TrackControlChain(TapeEngine& engineToUse);
    ~TrackControlChain() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelectedTrack(int trackIndex);
    int getSelectedTrack() const noexcept;
    void setInputOptions(const juce::Array<TapeEngine::InputSourceOption>& options);

private:
    static constexpr int compressorControlCount = 5;
    static constexpr int delayControlCount = 3;
    static constexpr int reverbControlCount = 3;

    class CloseButton : public juce::Button
    {
    public:
        CloseButton();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

    class BypassButton : public juce::Button
    {
    public:
        BypassButton();
        void setAccentColour(juce::Colour newAccentColour);
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    private:
        juce::Colour accentColour { juce::Colours::white };
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
    juce::Array<TapeEngine::InputSourceOption> inputOptions;
    juce::Viewport modulesViewport;
    ContentComponent contentComponent;
    juce::ComboBox inputSourceBox;
    juce::Slider inputGainSlider;
    juce::TextButton addModuleButton { "+" };
    std::array<BypassButton, Track::maxChainModules> bypassButtons;
    std::array<CloseButton, Track::maxChainModules> removeButtons;
    std::array<juce::Slider, Track::maxChainModules> filterSliders;
    std::array<std::array<juce::Slider, Track::maxEqBands>, Track::maxChainModules> eqGainSliders;
    std::array<std::array<juce::Slider, Track::maxEqBands>, Track::maxChainModules> eqQSliders;
    std::array<std::array<juce::TextEditor, Track::maxEqBands>, Track::maxChainModules> eqFrequencyEditors;
    std::array<std::array<juce::Slider, compressorControlCount>, Track::maxChainModules> compressorSliders;
    std::array<SaturationModeButton, Track::maxChainModules> saturationModeButtons;
    std::array<juce::Slider, Track::maxChainModules> saturationAmountSliders;
    std::array<std::array<juce::Slider, delayControlCount>, Track::maxChainModules> delaySliders;
    std::array<juce::TextButton, Track::maxChainModules> delayTimeModeButtons;
    std::array<std::array<juce::Slider, reverbControlCount>, Track::maxChainModules> reverbSliders;
    std::array<juce::Slider, Track::maxChainModules> gainModuleSliders;
    std::array<int, Track::maxChainModules> visibleSlots {};
    std::array<ChainModuleType, Track::maxChainModules> visibleTypes {};
    std::array<juce::Rectangle<int>, Track::maxChainModules> moduleBounds {};
    int visibleModuleCount = 0;

    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void paintContent(juce::Graphics& g);
    void refreshFromEngine();
    void updateAccentColours();
    void updateVisibleModules();
    void updateModuleVisibility();
    void layoutContent();
    void layoutInputModule(juce::Rectangle<int> inputBounds);
    void layoutFilterModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutEqModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutCompressorModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutSaturationModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutDelayModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutReverbModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutGainModule(int slot, juce::Rectangle<int> moduleArea);
    void paintInputModule(juce::Graphics& g, juce::Colour accent, juce::Rectangle<int> inputBounds);
    void paintFilterModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintEqModule(juce::Graphics& g, int slot);
    void paintCompressorModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintSaturationModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintDelayModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintReverbModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintGainModule(juce::Graphics& g, int slot, juce::Rectangle<int> bounds);
    void syncFrequencyEditorsFromEngine(bool force);
    juce::Rectangle<int> getFrameBounds() const;
    juce::Rectangle<int> getViewportBounds() const;
    juce::Rectangle<int> getInputModuleBounds() const;
    juce::Rectangle<int> getAddButtonBounds() const;
};
