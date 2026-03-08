#include "../../TapeEngine.h"

#include <cmath>

void TapeEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                  int numInputChannels,
                                                  float* const* outputChannelData,
                                                  int numOutputChannels,
                                                  int numSamples,
                                                  const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);

    clearAudioOutputs(outputChannelData, numOutputChannels, numSamples);

    if (numSamples <= 0)
        return;

    if (numSamples > inputScratch.getNumSamples())
        return;

    copyInputsToScratch(inputChannelData, numInputChannels, numSamples);

    auto localPlayhead = applyRequestedPlayheadForBlock();
    const auto currentBpm = juce::jmax(30.0, (double) bpm.load(std::memory_order_acquire));
    const auto samplesPerBeat = (sampleRate * 60.0) / currentBpm;
    const auto currentBeatsPerBar = juce::jmax(1, beatsPerBar.load(std::memory_order_acquire));
    const auto metronomeOn = metronomeEnabled.load(std::memory_order_acquire);

    beginCountInIfRequestedForBlock(currentBeatsPerBar);
    prepareTransportStartForBlock(localPlayhead);
    applyPendingModuleChanges();

    displayPlayhead.store((double) localPlayhead, std::memory_order_release);

    auto transportRunning = playing.load(std::memory_order_acquire);
    auto countInRunning = countInActive.load(std::memory_order_acquire);
    const auto countInHasClicks = countInAudible.load(std::memory_order_acquire);
    const auto shouldRewind = rewinding.load(std::memory_order_acquire);
    const auto shouldReversePlay = reversePlaying.load(std::memory_order_acquire);
    const auto soloedTrack = soloTrack.load(std::memory_order_acquire);
    std::array<int64_t, maxLoopMarkers> loopMarkers {};
    const auto loopMarkerCountSnapshot = readLoopMarkers(loopMarkers);

    if (transportRunning || countInRunning)
    {
        const auto endSample = localPlayhead + numSamples;
        const auto neededChunks = juce::jlimit(initialChunkCount,
                                               Track::maxChunks,
                                               (endSample / Track::chunkSize) + allocationLeadChunks);
        requiredChunkCount.store(neededChunks, std::memory_order_release);
    }

    std::array<float, numTracks> blockPeaks {};
    std::array<bool, numTracks> blockClips {};
    prepareTrackRuntimePeaksForBlock();

    auto* outputLeft = numOutputChannels > 0 ? outputChannelData[0] : nullptr;
    auto* outputRight = numOutputChannels > 1 ? outputChannelData[1] : nullptr;

    auto transportSample = localPlayhead;
    auto reachedTransportStart = false;

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        auto tapeSample = transportSample;
        const auto rawPreviousBeatIndex = (int64_t) std::floor(((double) tapeSample - 1.0) / samplesPerBeat);
        const auto rawCurrentBeatIndex = (int64_t) std::floor((double) tapeSample / samplesPerBeat);
        int64_t loopStartBeat = 0;

        if (transportRunning && rawCurrentBeatIndex != rawPreviousBeatIndex
            && getLoopStartBeat(rawCurrentBeatIndex, loopMarkers, loopMarkerCountSnapshot, loopStartBeat))
        {
            for (auto& track : tracks)
            {
                const auto recordMode = (TrackRecordMode) track.recordMode.load(std::memory_order_acquire);

                if (recordMode == TrackRecordMode::replace || recordMode == TrackRecordMode::erase)
                {
                    track.recordMode.store((int) TrackRecordMode::off, std::memory_order_release);
                    track.pendingRecordMode.store((int) TrackRecordMode::off, std::memory_order_release);
                }
            }

            tapeSample = juce::jmax(0, (int) std::llround((double) loopStartBeat * samplesPerBeat));
        }

        const auto previousBeatIndex = (int64_t) std::floor(((double) tapeSample - 1.0) / samplesPerBeat);
        const auto currentBeatIndex = (int64_t) std::floor((double) tapeSample / samplesPerBeat);

        if (transportRunning && currentBeatIndex != previousBeatIndex && currentBeatIndex % currentBeatsPerBar == 0)
            applyPendingRecordModes();

        auto mixedLeft = 0.0f;
        auto mixedRight = 0.0f;
        auto delayBusLeft = 0.0f;
        auto delayBusRight = 0.0f;
        std::array<std::array<float, Track::numChannels>, numTracks> trackDryContributions {};
        std::array<std::array<float, Track::numChannels>, numTracks> trackDelayContributions {};

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
        {
            auto& track = tracks[(size_t) trackIndex];
            const auto recordMode = (TrackRecordMode) track.recordMode.load(std::memory_order_acquire);
            const auto isMuted = track.muted.load(std::memory_order_acquire);
            const auto isAudible = (soloedTrack < 0 || soloedTrack == trackIndex) && ! isMuted;

            const auto transportReadsTape = transportRunning || shouldReversePlay;
            auto trackLeft = transportReadsTape ? readTrackSample(track, 0, tapeSample) : 0.0f;
            auto trackRight = transportReadsTape ? readTrackSample(track, 1, tapeSample) : 0.0f;
            auto monitorLeft = 0.0f;
            auto monitorRight = 0.0f;
            auto hasProcessedInput = false;
            auto processedLeft = 0.0f;
            auto processedRight = 0.0f;

            if (recordMode != TrackRecordMode::off && recordMode != TrackRecordMode::erase)
            {
                processedLeft = processInputSample(track,
                                                   0,
                                                   getInputSampleForTrack(trackIndex,
                                                                          track,
                                                                          inputChannelData,
                                                                          numInputChannels,
                                                                          0,
                                                                          sampleIndex));
                processedRight = processInputSample(track,
                                                    1,
                                                    getInputSampleForTrack(trackIndex,
                                                                           track,
                                                                           inputChannelData,
                                                                           numInputChannels,
                                                                           1,
                                                                           sampleIndex));
                hasProcessedInput = true;
            }

            if (transportRunning && recordMode != TrackRecordMode::off)
            {
                if (recordMode == TrackRecordMode::erase)
                {
                    if (writeTrackSample(track, 0, tapeSample, 0.0f))
                        trackLeft = 0.0f;

                    if (writeTrackSample(track, 1, tapeSample, 0.0f))
                        trackRight = 0.0f;
                }
                else
                {
                    const auto writtenLeft = recordMode == TrackRecordMode::replace ? processedLeft
                                                                                     : trackLeft + processedLeft;
                    const auto writtenRight = recordMode == TrackRecordMode::replace ? processedRight
                                                                                      : trackRight + processedRight;

                    if (writeTrackSample(track, 0, tapeSample, writtenLeft))
                        trackLeft = writtenLeft;

                    if (writeTrackSample(track, 1, tapeSample, writtenRight))
                        trackRight = writtenRight;
                }
            }
            else if (hasProcessedInput)
            {
                monitorLeft = processedLeft;
                monitorRight = processedRight;
            }

            const auto audibleLeft = trackLeft + monitorLeft;
            const auto audibleRight = trackRight + monitorRight;
            auto postMixerLeft = audibleLeft * juce::Decibels::decibelsToGain(track.mixerGainDb.load(std::memory_order_relaxed));
            auto postMixerRight = audibleRight * juce::Decibels::decibelsToGain(track.mixerGainDb.load(std::memory_order_relaxed));
            applyTrackMixer(track.mixerPan.load(std::memory_order_relaxed), postMixerLeft, postMixerRight);

            if (isAudible)
            {
                const auto delaySendAmount = track.delaySend.load(std::memory_order_relaxed);
                mixedLeft += postMixerLeft;
                mixedRight += postMixerRight;
                delayBusLeft += postMixerLeft * delaySendAmount;
                delayBusRight += postMixerRight * delaySendAmount;
                trackDryContributions[(size_t) trackIndex][0] = postMixerLeft;
                trackDryContributions[(size_t) trackIndex][1] = postMixerRight;
                trackDelayContributions[(size_t) trackIndex][0] = postMixerLeft * delaySendAmount;
                trackDelayContributions[(size_t) trackIndex][1] = postMixerRight * delaySendAmount;
            }

            const auto peak = juce::jmax(std::abs(audibleLeft), std::abs(audibleRight));
            blockPeaks[(size_t) trackIndex] = juce::jmax(blockPeaks[(size_t) trackIndex], peak);
            blockClips[(size_t) trackIndex] = blockClips[(size_t) trackIndex] || peak > 1.0f;
            track.mixerBlockPeak = juce::jmax(track.mixerBlockPeak,
                                              juce::jmax(std::abs(postMixerLeft), std::abs(postMixerRight)));
        }

        auto delayReturnLeft = 0.0f;
        auto delayReturnRight = 0.0f;
        processDelayReturn(delayBusLeft, delayBusRight, delayReturnLeft, delayReturnRight);
        mixedLeft += delayReturnLeft;
        mixedRight += delayReturnRight;

        auto totalDelayWeight = 0.0f;
        std::array<std::array<float, Track::numChannels>, numTracks> currentTrackInputBuses {};

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
            totalDelayWeight += std::abs(trackDelayContributions[(size_t) trackIndex][0]) + std::abs(trackDelayContributions[(size_t) trackIndex][1]);

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
        {
            const auto delayWeight = std::abs(trackDelayContributions[(size_t) trackIndex][0]) + std::abs(trackDelayContributions[(size_t) trackIndex][1]);
            const auto delayShare = totalDelayWeight > 0.0f ? delayWeight / totalDelayWeight : 0.0f;
            currentTrackInputBuses[(size_t) trackIndex][0] = trackDryContributions[(size_t) trackIndex][0] + (delayReturnLeft * delayShare);
            currentTrackInputBuses[(size_t) trackIndex][1] = trackDryContributions[(size_t) trackIndex][1] + (delayReturnRight * delayShare);
        }

        lastMasterInputBus[0] = mixedLeft;
        lastMasterInputBus[1] = mixedRight;

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
            lastTrackInputBuses[(size_t) trackIndex] = currentTrackInputBuses[(size_t) trackIndex];

        if (transportStartPulsePending.exchange(false, std::memory_order_acq_rel) && metronomeOn)
            triggerMetronomePulse(true);

        if (metronomeOn || (countInRunning && countInHasClicks))
        {
            if (transportRunning && currentBeatIndex != previousBeatIndex)
                triggerMetronomePulse(currentBeatIndex % currentBeatsPerBar == 0);
            else if (countInRunning && countInHasClicks)
            {
                const auto previousCountInBeat = (int64_t) std::floor((countInSamplePosition - 1.0) / samplesPerBeat);
                const auto currentCountInBeat = (int64_t) std::floor(countInSamplePosition / samplesPerBeat);

                if (currentCountInBeat != previousCountInBeat && currentCountInBeat < countInTotalBeats)
                    triggerMetronomePulse(currentCountInBeat == 0);
            }

            if (clickSamplesRemaining > 0)
            {
                const auto envelope = (float) clickSamplesRemaining / (float) clickTotalSamples;
                const auto clickSample = (float) (std::sin(clickPhase) * clickAmplitude * envelope);
                clickPhase += clickPhaseDelta;
                --clickSamplesRemaining;
                mixedLeft += clickSample;
                mixedRight += clickSample;
            }
        }

        if (countInRunning)
        {
            countInSamplePosition += 1.0;

            if (countInSamplePosition >= (double) countInTotalBeats * samplesPerBeat)
            {
                countInRunning = false;
                countInSamplePosition = 0.0;
                countInTotalBeats = 0;
                countInActive.store(false, std::memory_order_release);
                countInAudible.store(false, std::memory_order_release);
                transportStartPulsePending.store(true, std::memory_order_release);
                syncWritePositionsToPlayhead(localPlayhead);
                playing.store(true, std::memory_order_release);
                transportRunning = true;
                transportSample = localPlayhead;
            }
        }

        if (outputLeft != nullptr)
            outputLeft[sampleIndex] = mixedLeft;

        if (outputRight != nullptr)
            outputRight[sampleIndex] = mixedRight;
        else if (outputLeft != nullptr && numOutputChannels == 1)
            outputLeft[sampleIndex] = 0.5f * (mixedLeft + mixedRight);

        if (shouldReversePlay)
        {
            if (tapeSample <= 0)
            {
                transportSample = 0;
                reachedTransportStart = true;
            }
            else
            {
                transportSample = tapeSample - 1;
            }
        }
        else if (transportRunning)
        {
            transportSample = tapeSample + 1;
        }
    }

    updateTrackMetersFromBlockPeaks(blockPeaks, blockClips);

    if (! transportRunning)
    {
        finalizeStoppedTransportForBlock(shouldReversePlay,
                                         shouldRewind,
                                         transportSample,
                                         reachedTransportStart,
                                         localPlayhead,
                                         numSamples);
        return;
    }

    finalizeRunningTransportForBlock(transportSample);
}

void TapeEngine::clearAudioOutputs(float* const* outputChannelData, int numOutputChannels, int numSamples) const noexcept
{
    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
    }
}

void TapeEngine::copyInputsToScratch(const float* const* inputChannelData, int numInputChannels, int numSamples) noexcept
{
    for (int channel = 0; channel < Track::numChannels; ++channel)
    {
        auto* destination = inputScratch.getWritePointer(channel);

        if (numInputChannels <= 0)
        {
            juce::FloatVectorOperations::clear(destination, numSamples);
            continue;
        }

        const auto sourceIndex = juce::jlimit(0, numInputChannels - 1, channel);
        auto* source = inputChannelData[sourceIndex];

        if (source != nullptr)
            juce::FloatVectorOperations::copy(destination, source, numSamples);
        else
            juce::FloatVectorOperations::clear(destination, numSamples);
    }
}

int TapeEngine::applyRequestedPlayheadForBlock() noexcept
{
    auto requestedPosition = requestedPlayhead.exchange(-1.0, std::memory_order_acq_rel);
    auto localPlayhead = (int) juce::jmax(0.0, playhead.load(std::memory_order_acquire));

    if (requestedPosition >= 0.0)
    {
        localPlayhead = (int) juce::jmax(0.0, requestedPosition);
        playhead.store((double) localPlayhead, std::memory_order_release);
        displayPlayhead.store((double) localPlayhead, std::memory_order_release);
        syncWritePositionsToPlayhead(localPlayhead);
    }

    return localPlayhead;
}

void TapeEngine::beginCountInIfRequestedForBlock(int currentBeatsPerBar) noexcept
{
    if (countInRequested.exchange(false, std::memory_order_acq_rel))
    {
        countInSamplePosition = 0.0;
        countInTotalBeats = currentBeatsPerBar;
        clickSamplesRemaining = 0;
        countInActive.store(true, std::memory_order_release);
        countInAudible.store(true, std::memory_order_release);
    }
}

void TapeEngine::prepareTransportStartForBlock(int localPlayhead) noexcept
{
    if (startRequested.exchange(false, std::memory_order_acq_rel))
        syncWritePositionsToPlayhead(localPlayhead);
}

void TapeEngine::prepareTrackRuntimePeaksForBlock() noexcept
{
    for (auto& track : tracks)
    {
        track.moduleBlockInputPeaks.fill(0.0f);
        track.moduleBlockOutputPeaks.fill(0.0f);
        track.mixerBlockPeak = 0.0f;
    }
}

void TapeEngine::updateTrackMetersFromBlockPeaks(const std::array<float, numTracks>& blockPeaks,
                                                 const std::array<bool, numTracks>& blockClips) noexcept
{
    for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
    {
        auto& track = tracks[(size_t) trackIndex];
        const auto previousPeak = track.peakMeter.load(std::memory_order_relaxed) * 0.82f;
        const auto previousMixerPeak = track.mixerMeter.load(std::memory_order_relaxed) * 0.82f;
        track.peakMeter.store(juce::jmax(blockPeaks[(size_t) trackIndex], previousPeak), std::memory_order_relaxed);
        track.mixerMeter.store(juce::jmax(track.mixerBlockPeak, previousMixerPeak), std::memory_order_relaxed);
        track.clipping.store(blockClips[(size_t) trackIndex], std::memory_order_relaxed);

        for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
        {
            const auto previousInput = track.moduleInputMeters[(size_t) moduleIndex].load(std::memory_order_relaxed) * 0.82f;
            const auto previousOutput = track.moduleOutputMeters[(size_t) moduleIndex].load(std::memory_order_relaxed) * 0.82f;
            track.moduleInputMeters[(size_t) moduleIndex].store(juce::jmax(track.moduleBlockInputPeaks[(size_t) moduleIndex], previousInput),
                                                                std::memory_order_relaxed);
            track.moduleOutputMeters[(size_t) moduleIndex].store(juce::jmax(track.moduleBlockOutputPeaks[(size_t) moduleIndex], previousOutput),
                                                                 std::memory_order_relaxed);
        }
    }
}

void TapeEngine::finalizeStoppedTransportForBlock(bool shouldReversePlay,
                                                  bool shouldRewind,
                                                  int transportSample,
                                                  bool reachedTransportStart,
                                                  int localPlayhead,
                                                  int numSamples) noexcept
{
    if (shouldReversePlay)
    {
        const auto reversePlayhead = juce::jmax(0, transportSample);
        playhead.store((double) reversePlayhead, std::memory_order_release);
        displayPlayhead.store((double) reversePlayhead, std::memory_order_release);

        if (reachedTransportStart || reversePlayhead == 0)
            reversePlaying.store(false, std::memory_order_release);

        return;
    }

    if (shouldRewind)
    {
        const auto rewindAmount = juce::jmax((int) std::round(numSamples * rewindSpeedMultiplier), 512);
        const auto rewoundPlayhead = juce::jmax(0, localPlayhead - rewindAmount);
        playhead.store((double) rewoundPlayhead, std::memory_order_release);
        displayPlayhead.store((double) rewoundPlayhead, std::memory_order_release);

        if (rewoundPlayhead == 0)
            rewinding.store(false, std::memory_order_release);
    }
}

void TapeEngine::finalizeRunningTransportForBlock(int newPlayhead) noexcept
{
    playhead.store((double) newPlayhead, std::memory_order_release);

    for (auto& track : tracks)
    {
        const auto recordMode = (TrackRecordMode) track.recordMode.load(std::memory_order_acquire);

        if (recordMode == TrackRecordMode::off)
            continue;

        track.writePosition = newPlayhead;

        if (recordMode != TrackRecordMode::erase)
        {
            const auto recordedLength = track.recordedLength.load(std::memory_order_relaxed);

            if (newPlayhead > recordedLength)
                track.recordedLength.store(newPlayhead, std::memory_order_release);
        }
    }
}
