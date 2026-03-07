#include "MainComponent.h"

MainComponent::MainComponent()
    : tapeView(tapeEngine),
      trackControlChain(tapeEngine),
      trackMixerPanel(tapeEngine)
{
    addAndMakeVisible(tapeView);
    addAndMakeVisible(trackControlChain);
    addChildComponent(trackMixerPanel);
    addAndMakeVisible(preTapeTabButton);
    addAndMakeVisible(mixerTabButton);

    preTapeTabButton.onClick = [this] { setBottomPanelMode(BottomPanelMode::preTape); };
    mixerTabButton.onClick = [this] { setBottomPanelMode(BottomPanelMode::mixer); };

    tapeView.onSelectedTrackChanged = [this](int trackIndex)
    {
        trackControlChain.setSelectedTrack(trackIndex);
        refreshInputOptions();
    };
    trackControlChain.setSelectedTrack(0);
    setBottomPanelMode(BottomPanelMode::preTape);

    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
    {
        auto safeThis = juce::Component::SafePointer<MainComponent>(this);
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
                                          [safeThis] (bool)
                                          {
                                              if (safeThis != nullptr)
                                                  safeThis->initialiseAudio();
                                          });
    }
    else
    {
        initialiseAudio();
    }

    setSize(1100, 920);
}

MainComponent::~MainComponent()
{
    audioDeviceManager.removeAudioCallback(&tapeEngine);
    audioDeviceManager.closeAudioDevice();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(14);
    auto panelBounds = bounds.removeFromBottom(236);
    auto tabArea = bounds.removeFromBottom(26);
    const auto buttonHeight = 24;
    const auto preTapeWidth = 98;
    const auto mixerWidth = 82;
    const auto tabGap = 8;
    const auto totalWidth = preTapeWidth + mixerWidth + tabGap;
    auto selectorBounds = juce::Rectangle<int>(tabArea.getCentreX() - (totalWidth / 2),
                                               tabArea.getY() + ((tabArea.getHeight() - buttonHeight) / 2),
                                               totalWidth,
                                               buttonHeight);
    preTapeTabButton.setBounds(selectorBounds.removeFromLeft(preTapeWidth));
    selectorBounds.removeFromLeft(tabGap);
    mixerTabButton.setBounds(selectorBounds.removeFromLeft(mixerWidth));
    trackControlChain.setBounds(panelBounds);
    trackMixerPanel.setBounds(panelBounds);
    tapeView.setBounds(bounds);
}

void MainComponent::initialiseAudio()
{
    const auto wantsInput = juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio) ? 16 : 0;
    const auto error = audioDeviceManager.initialise(wantsInput, 2, nullptr, true);

    if (error.isEmpty())
    {
        audioDeviceManager.addAudioCallback(&tapeEngine);
        refreshInputOptions();
    }
}

void MainComponent::refreshInputOptions()
{
    juce::StringArray hardwareInputNames;

    if (auto* device = audioDeviceManager.getCurrentAudioDevice())
        hardwareInputNames = device->getInputChannelNames();

    trackControlChain.setInputOptions(tapeEngine.getAvailableInputSources(hardwareInputNames,
                                                                         trackControlChain.getSelectedTrack()));
}

void MainComponent::setBottomPanelMode(BottomPanelMode newMode)
{
    bottomPanelMode = newMode;
    trackControlChain.setVisible(bottomPanelMode == BottomPanelMode::preTape);
    trackMixerPanel.setVisible(bottomPanelMode == BottomPanelMode::mixer);
    updateTabButtonStyles();
}

void MainComponent::updateTabButtonStyles()
{
    auto applyStyle = [] (juce::TextButton& button, bool isActive, juce::Button::ConnectedEdgeFlags edges)
    {
        button.setColour(juce::TextButton::buttonColourId, isActive ? juce::Colours::white : juce::Colours::black);
        button.setColour(juce::TextButton::buttonOnColourId, isActive ? juce::Colours::white : juce::Colours::black);
        button.setColour(juce::TextButton::textColourOffId, isActive ? juce::Colours::black : juce::Colours::white);
        button.setColour(juce::TextButton::textColourOnId, isActive ? juce::Colours::black : juce::Colours::white);
        button.setConnectedEdges(edges);
    };

    applyStyle(preTapeTabButton, bottomPanelMode == BottomPanelMode::preTape, juce::Button::ConnectedOnRight);
    applyStyle(mixerTabButton, bottomPanelMode == BottomPanelMode::mixer, juce::Button::ConnectedOnLeft);
    repaint();
}
