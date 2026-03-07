#include "MainComponent.h"

MainComponent::MainComponent()
    : tapeView(tapeEngine),
      trackControlChain(tapeEngine)
{
    addAndMakeVisible(tapeView);
    addAndMakeVisible(trackControlChain);

    tapeView.onSelectedTrackChanged = [this](int trackIndex)
    {
        trackControlChain.setSelectedTrack(trackIndex);
    };
    trackControlChain.setSelectedTrack(0);

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

    setSize(1100, 900);
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
    auto chainBounds = bounds.removeFromBottom(230);
    trackControlChain.setBounds(chainBounds);
    bounds.removeFromBottom(10);
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
    juce::StringArray inputOptions;

    if (auto* device = audioDeviceManager.getCurrentAudioDevice())
    {
        const auto inputNames = device->getInputChannelNames();

        for (int pairIndex = 0; pairIndex + 1 < inputNames.size(); pairIndex += 2)
            inputOptions.add("Stereo " + juce::String(pairIndex + 1) + "/" + juce::String(pairIndex + 2));

        for (int channelIndex = 0; channelIndex < inputNames.size(); ++channelIndex)
        {
            auto label = inputNames[channelIndex].trim();

            if (label.isEmpty())
                label = "Input " + juce::String(channelIndex + 1);

            inputOptions.add("Mono " + juce::String(channelIndex + 1) + " - " + label);
        }
    }

    if (inputOptions.isEmpty())
        inputOptions.add("No input");

    trackControlChain.setInputOptions(inputOptions);
}
