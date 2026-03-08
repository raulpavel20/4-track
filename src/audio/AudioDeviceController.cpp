#include "AudioDeviceController.h"

#include "../AppSettings.h"
#include "../TapeEngine.h"

AudioDeviceController::AudioDeviceController()
{
    audioDeviceManager.addChangeListener(this);
}

AudioDeviceController::~AudioDeviceController()
{
    audioDeviceManager.removeChangeListener(this);

    if (tapeEngine != nullptr)
        audioDeviceManager.removeAudioCallback(tapeEngine);

    audioDeviceManager.closeAudioDevice();
}

void AudioDeviceController::initialise(TapeEngine& tapeEngineToUse, std::function<void()> onDeviceChangeToUse)
{
    if (tapeEngine != nullptr)
        audioDeviceManager.removeAudioCallback(tapeEngine);

    tapeEngine = &tapeEngineToUse;
    onDeviceChange = std::move(onDeviceChangeToUse);

    const auto wantsInput = juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio) ? 16 : 0;
    const auto savedState = AppSettings::getInstance().createAudioDeviceState();
    auto error = audioDeviceManager.initialise(wantsInput, 2, savedState.get(), true);

    if (error.isNotEmpty() && savedState != nullptr)
        error = audioDeviceManager.initialise(wantsInput, 2, nullptr, true);

    if (error.isEmpty())
    {
        audioDeviceManager.addAudioCallback(tapeEngine);
        persistAudioDeviceState();

        if (onDeviceChange != nullptr)
            onDeviceChange();
    }
}

juce::AudioDeviceManager& AudioDeviceController::getDeviceManager() noexcept
{
    return audioDeviceManager;
}

const juce::AudioDeviceManager& AudioDeviceController::getDeviceManager() const noexcept
{
    return audioDeviceManager;
}

juce::StringArray AudioDeviceController::getHardwareInputNames() const
{
    juce::StringArray hardwareInputNames;

    if (auto* device = audioDeviceManager.getCurrentAudioDevice())
        hardwareInputNames = device->getInputChannelNames();

    return hardwareInputNames;
}

void AudioDeviceController::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &audioDeviceManager)
        return;

    persistAudioDeviceState();

    if (onDeviceChange != nullptr)
        onDeviceChange();
}

void AudioDeviceController::persistAudioDeviceState()
{
    const auto state = audioDeviceManager.createStateXml();
    AppSettings::getInstance().setAudioDeviceState(state.get());
}
