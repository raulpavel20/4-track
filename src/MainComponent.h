#pragma once

#include <JuceHeader.h>

#include "TapeEngine.h"
#include "TrackControlChain.h"
#include "TrackMixerPanel.h"
#include "TapeView.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum class BottomPanelMode
    {
        preTape = 0,
        mixer
    };

    juce::AudioDeviceManager audioDeviceManager;
    TapeEngine tapeEngine;
    TapeView tapeView;
    TrackControlChain trackControlChain;
    TrackMixerPanel trackMixerPanel;
    juce::TextButton preTapeTabButton { "PRE-TAPE" };
    juce::TextButton mixerTabButton { "MIXER" };
    BottomPanelMode bottomPanelMode = BottomPanelMode::preTape;

    void initialiseAudio();
    void refreshInputOptions();
    void setBottomPanelMode(BottomPanelMode newMode);
    void updateTabButtonStyles();
};
