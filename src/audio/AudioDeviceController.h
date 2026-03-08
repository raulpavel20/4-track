#pragma once

#include <JuceHeader.h>

#include <functional>

class TapeEngine;

class AudioDeviceController : private juce::ChangeListener
{
public:
    AudioDeviceController();
    ~AudioDeviceController() override;

    void initialise(TapeEngine& tapeEngineToUse, std::function<void()> onDeviceChangeToUse);
    juce::AudioDeviceManager& getDeviceManager() noexcept;
    const juce::AudioDeviceManager& getDeviceManager() const noexcept;
    juce::StringArray getHardwareInputNames() const;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void persistAudioDeviceState();

    juce::AudioDeviceManager audioDeviceManager;
    TapeEngine* tapeEngine = nullptr;
    std::function<void()> onDeviceChange;
};
