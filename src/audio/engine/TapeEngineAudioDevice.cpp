#include "../../TapeEngine.h"

#include <cmath>

void TapeEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate = juce::jmax(1.0, device->getCurrentSampleRate());
    preparedBlockSize = juce::jmax(256, device->getCurrentBufferSizeSamples());
    inputScratch.setSize(Track::numChannels, preparedBlockSize, false, false, true);
    inputScratch.clear();
    maxDelaySamples = juce::jmax(1, (int) std::ceil(sampleRate * 2.0));
    delayBuffer.setSize(Track::numChannels, maxDelaySamples, false, false, true);
    delayBuffer.clear();
    delayWritePosition = 0;
    reverb.reset();
    reverb.setSampleRate(sampleRate);
    lastMasterInputBus.fill(0.0f);

    for (auto& trackBus : lastTrackInputBuses)
        trackBus.fill(0.0f);

    requiredChunkCount.store(initialChunkCount, std::memory_order_release);
    clickSamplesRemaining = 0;
    clickTotalSamples = juce::jmax(1, (int) std::round(sampleRate * 0.035));
    clickPhase = 0.0;
    clickPhaseDelta = 0.0;
    clickAmplitude = 0.0f;
    countInSamplePosition = 0.0;
    countInTotalBeats = 0;
    countInRequested.store(false, std::memory_order_release);
    countInActive.store(false, std::memory_order_release);
    countInAudible.store(false, std::memory_order_release);
    transportStartPulsePending.store(false, std::memory_order_release);
    metronomePulseRevision.store(0, std::memory_order_release);

    for (auto& track : tracks)
        resetTrackRuntimeState(track);

    servicePendingAllocations();
}

void TapeEngine::audioDeviceStopped()
{
    resetCountInState();
    playing.store(false, std::memory_order_release);
    rewinding.store(false, std::memory_order_release);
    reversePlaying.store(false, std::memory_order_release);
    clickSamplesRemaining = 0;
    countInSamplePosition = 0.0;
    countInTotalBeats = 0;
    countInAudible.store(false, std::memory_order_release);
    transportStartPulsePending.store(false, std::memory_order_release);
    delayWritePosition = 0;
    delayBuffer.clear();
    reverb.reset();
    lastMasterInputBus.fill(0.0f);

    for (auto& trackBus : lastTrackInputBuses)
        trackBus.fill(0.0f);
}
