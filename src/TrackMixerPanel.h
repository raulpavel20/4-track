#pragma once

#include <JuceHeader.h>

#include <array>

#include "TapeEngine.h"

class TrackMixerPanel : public juce::Component,
                        private juce::Timer
{
public:
    explicit TrackMixerPanel(TapeEngine& engineToUse);

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
    std::array<juce::Slider, TapeEngine::numTracks> reverbSendSliders;
    std::array<juce::Slider, 3> delayControlSliders;
    std::array<juce::Slider, 3> reverbControlSliders;
    std::array<juce::Rectangle<int>, TapeEngine::numTracks> trackModuleBounds {};
    juce::Rectangle<int> delayModuleBounds;
    juce::Rectangle<int> reverbModuleBounds;

    void timerCallback() override;
    void paintContent(juce::Graphics& g);
    void refreshFromEngine();
    void updateColours();
    void layoutContent();
    juce::Rectangle<int> getFrameBounds() const;
    juce::Rectangle<int> getViewportBounds() const;
};
