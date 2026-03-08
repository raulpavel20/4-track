#pragma once

#include <JuceHeader.h>

#include "TapeEngine.h"

#include <array>
#include <functional>

class TapeView : public juce::Component,
                 private juce::Timer
{
public:
    explicit TapeView(TapeEngine& engineToUse);
    ~TapeView() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    void setSelectedTrack(int trackIndex);
    int getSelectedTrack() const noexcept;

    std::function<void(int)> onSelectedTrackChanged;

private:
    TapeEngine& engine;
    int selectedTrack = 0;
    float reelPhase = 0.0f;
    double displayedPlayhead = 0.0;
    double lastDisplayedPlayhead = 0.0;
    double lastTimerSeconds = 0.0;
    double metronomeBlinkLevel = 0.0;
    int lastMetronomePulseRevision = 0;
    bool isScrubbing = false;
    bool reversePreviewPressed = false;
    float scrubStartX = 0.0f;
    double scrubStartPlayhead = 0.0;
    juce::Label bpmEditor;
    juce::Label beatsPerBarEditor;
    juce::Slider zoomSlider;

    juce::Rectangle<int> getLaneBounds(int trackIndex) const;
    juce::Rectangle<int> getTapeAreaBounds() const;
    juce::Rectangle<int> getTrackSectionBounds() const;
    juce::Rectangle<int> getReelSectionBounds() const;
    juce::Rectangle<int> getPlayStopButtonBounds() const;
    juce::Rectangle<int> getRewindButtonBounds() const;
    juce::Rectangle<int> getReversePreviewButtonBounds() const;
    juce::Rectangle<int> getMetronomeButtonBounds() const;
    juce::Rectangle<int> getLoopButtonBounds() const;
    juce::Rectangle<int> getBpmEditorBounds() const;
    juce::Rectangle<int> getBeatsPerBarEditorBounds() const;
    juce::Rectangle<int> getControlClusterBounds(int trackIndex) const;
    juce::Rectangle<int> getModeButtonBounds(int trackIndex, int buttonIndex) const;
    juce::Rectangle<int> getUtilityButtonBounds(int trackIndex, int buttonIndex) const;
    juce::Rectangle<int> getZoomSliderBounds() const;
    juce::Rectangle<int> getWaveformBounds(int trackIndex) const;
    juce::Rectangle<int> getMeterBounds(int trackIndex) const;
    double getVisibleSamples() const;
    juce::Range<float> getWaveformExtents(int trackIndex, int startSample, int endSample) const;
    void scrubTo(juce::Point<float> position);
    void timerCallback() override;
};
