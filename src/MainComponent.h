#pragma once

#include <JuceHeader.h>

#include "TapeEngine.h"
#include "TrackControlChain.h"
#include "TapeView.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioDeviceManager audioDeviceManager;
    TapeEngine tapeEngine;
    TapeView tapeView;
    TrackControlChain trackControlChain;

    void initialiseAudio();
    void refreshInputOptions();
};
