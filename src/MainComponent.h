#pragma once

#include <JuceHeader.h>

#include "TapeEngine.h"
#include "TrackControlChain.h"
#include "TrackMixerPanel.h"
#include "TapeView.h"

class MainComponent : public juce::Component,
                      private juce::KeyListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class HelpButton : public juce::Button
    {
    public:
        HelpButton();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

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
    HelpButton shortcutsHelpButton;
    BottomPanelMode bottomPanelMode = BottomPanelMode::preTape;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void initialiseAudio();
    void refreshInputOptions();
    void setBottomPanelMode(BottomPanelMode newMode);
    void updateTabButtonStyles();
    void addKeyListenerRecursive(juce::Component& component);
    void removeKeyListenerRecursive(juce::Component& component);
    void togglePlayStop();
    void jumpToAdjacentBeat(bool forward);
    void jumpToAdjacentLoopMarker(bool forward);
    void selectAdjacentTrack(int delta);
    void toggleCurrentTrackSolo();
    void toggleCurrentTrackMute();
    void toggleCurrentTrackRecordMode(TrackRecordMode mode);
    void showShortcutsHelp();
};
