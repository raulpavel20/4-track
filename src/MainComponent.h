#pragma once

#include <JuceHeader.h>

#include "audio/AudioDeviceController.h"
#include "AppSettingsComponent.h"
#include "TapeEngine.h"
#include "TrackControlChain.h"
#include "TrackMixerPanel.h"
#include "TapeView.h"

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      private juce::KeyListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void shutdown();
    void beginInitialiseAudio();

private:
    class HelpButton : public juce::Button
    {
    public:
        HelpButton();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

    class SettingsButton : public juce::Button
    {
    public:
        SettingsButton();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

    class SettingsWindow : public juce::DialogWindow
    {
    public:
        SettingsWindow(MainComponent& ownerToUse, std::unique_ptr<juce::Component> content);
        void closeButtonPressed() override;

    private:
        MainComponent& owner;
    };

    enum class BottomPanelMode
    {
        preTape = 0,
        mixer,
        send1,
        send2,
        send3
    };

    TapeEngine tapeEngine;
    AudioDeviceController audioDeviceController;
    TapeView tapeView;
    TrackControlChain trackControlChain;
    TrackControlChain send1ControlChain;
    TrackControlChain send2ControlChain;
    TrackControlChain send3ControlChain;
    TrackMixerPanel trackMixerPanel;
    juce::TextButton preTapeTabButton { "PRE-TAPE" };
    juce::TextButton mixerTabButton { "MIXER" };
    juce::TextButton send1TabButton { "SEND 1" };
    juce::TextButton send2TabButton { "SEND 2" };
    juce::TextButton send3TabButton { "SEND 3" };
    SettingsButton settingsButton;
    HelpButton shortcutsHelpButton;
    BottomPanelMode bottomPanelMode = BottomPanelMode::preTape;
    std::unique_ptr<SettingsWindow> settingsWindow;
    bool shuttingDown = false;
    bool audioInitialisationStarted = false;

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
    void showSettingsWindow();
    void closeSettingsWindow();
    void showShortcutsHelp();
};
