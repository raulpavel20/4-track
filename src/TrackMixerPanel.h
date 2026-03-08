#pragma once

#include <JuceHeader.h>

#include <array>

#include "TapeEngine.h"

class TrackMixerPanel : public juce::Component,
                        private juce::Timer,
                        private juce::ChangeListener
{
public:
    explicit TrackMixerPanel(TapeEngine& engineToUse);
    ~TrackMixerPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ContentComponent : public juce::Component
    {
    public:
        explicit ContentComponent(TrackMixerPanel& ownerToUse);
        void paint(juce::Graphics& g) override;

    private:
        TrackMixerPanel& owner;
    };

    TapeEngine& engine;
    juce::Viewport modulesViewport;
    ContentComponent contentComponent;
    std::array<juce::Slider, TapeEngine::numTracks> gainSliders;
    std::array<juce::Slider, TapeEngine::numTracks> panSliders;
    std::array<juce::Slider, TapeEngine::numTracks> delaySendSliders;
    std::array<juce::Slider, 3> delayControlSliders;
    juce::TextButton delayTimeModeButton;
    juce::ComboBox exportFormatBox;
    juce::ComboBox exportSampleRateBox;
    juce::ComboBox exportBitDepthBox;
    juce::ComboBox exportTailBox;
    juce::TextButton exportButton { "Export" };
    std::unique_ptr<juce::FileChooser> exportChooser;
    std::array<juce::Rectangle<int>, TapeEngine::numTracks> trackModuleBounds {};
    juce::Rectangle<int> delayModuleBounds;
    juce::Rectangle<int> exportModuleBounds;

    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void paintContent(juce::Graphics& g);
    void refreshFromEngine();
    void updateColours();
    void layoutContent();
    void syncExportDefaultsFromSettings();
    void persistExportDefaults();
    juce::Rectangle<int> getFrameBounds() const;
    juce::Rectangle<int> getViewportBounds() const;
};
