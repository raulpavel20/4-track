#include "AudioDeviceController.h"

#include "../AppSettings.h"
#include "../TapeEngine.h"

namespace
{
void applyBluetoothHeadsetFallback(juce::AudioDeviceManager& audioDeviceManager)
{
    auto setup = audioDeviceManager.getAudioDeviceSetup();

    if (setup.inputDeviceName.isEmpty() || setup.outputDeviceName.isEmpty())
        return;

    if (setup.inputDeviceName != setup.outputDeviceName)
        return;

    auto* device = audioDeviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return;

    if (device->getCurrentSampleRate() > 24000.0)
        return;

    setup.inputDeviceName = {};
    audioDeviceManager.setAudioDeviceSetup(setup, true);
}
}

AudioDeviceController::AudioDeviceController()
{
    audioDeviceManager.addChangeListener(this);
}

AudioDeviceController::~AudioDeviceController()
{
    audioDeviceManager.removeChangeListener(this);
    shutdown();
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
        applyBluetoothHeadsetFallback(audioDeviceManager);
        audioDeviceManager.addAudioCallback(tapeEngine);
        persistAudioDeviceState();

        if (onDeviceChange != nullptr)
            onDeviceChange();
    }
}

void AudioDeviceController::shutdown()
{
    if (tapeEngine != nullptr)
        audioDeviceManager.removeAudioCallback(tapeEngine);

    tapeEngine = nullptr;
    onDeviceChange = nullptr;
    audioDeviceManager.closeAudioDevice();
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
