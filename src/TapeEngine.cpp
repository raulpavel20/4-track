#include "TapeEngine.h"

#include <cmath>

TapeEngine::TapeEngine()
{
    for (auto& loopMarkerBeat : loopMarkerBeats)
        loopMarkerBeat.store(-1, std::memory_order_relaxed);
}

void TapeEngine::play()
{
    rewinding.store(false, std::memory_order_release);
    startRequested.store(true, std::memory_order_release);
    playing.store(true, std::memory_order_release);
}

void TapeEngine::stop()
{
    playing.store(false, std::memory_order_release);
    rewinding.store(false, std::memory_order_release);
    clickSamplesRemaining = 0;
    applyPendingRecordModes();

    for (auto& track : tracks)
        track.peakMeter.store(0.0f, std::memory_order_relaxed);
}

void TapeEngine::rewind()
{
    playing.store(false, std::memory_order_release);
    rewinding.store(true, std::memory_order_release);
}

bool TapeEngine::isPlaying() const noexcept
{
    return playing.load(std::memory_order_acquire);
}

bool TapeEngine::isRewinding() const noexcept
{
    return rewinding.load(std::memory_order_acquire);
}

double TapeEngine::getSampleRate() const noexcept
{
    return sampleRate;
}

void TapeEngine::setBpm(float newBpm)
{
    bpm.store(juce::jlimit(30.0f, 300.0f, newBpm), std::memory_order_release);
}

float TapeEngine::getBpm() const noexcept
{
    return bpm.load(std::memory_order_acquire);
}

void TapeEngine::setBeatsPerBar(int newBeatsPerBar)
{
    beatsPerBar.store(juce::jlimit(1, 16, newBeatsPerBar), std::memory_order_release);
}

int TapeEngine::getBeatsPerBar() const noexcept
{
    return beatsPerBar.load(std::memory_order_acquire);
}

void TapeEngine::toggleLoopMarkerAtNearestBeat()
{
    if (! canEditLoopMarkers())
        return;

    const auto samplesPerBeat = (sampleRate * 60.0) / juce::jmax(30.0, (double) bpm.load(std::memory_order_acquire));
    const auto beatIndex = juce::jmax<int64_t>(0, (int64_t) std::llround(playhead.load(std::memory_order_acquire) / samplesPerBeat));
    std::array<int64_t, maxLoopMarkers> markers {};
    const auto markerCount = readLoopMarkers(markers);
    std::array<int64_t, maxLoopMarkers> updatedMarkers {};
    auto updatedCount = 0;
    auto markerRemoved = false;

    for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex)
    {
        const auto markerBeat = markers[(size_t) markerIndex];

        if (markerBeat == beatIndex)
        {
            markerRemoved = true;
            continue;
        }

        updatedMarkers[(size_t) updatedCount++] = markerBeat;
    }

    if (! markerRemoved && updatedCount < maxLoopMarkers)
    {
        auto insertIndex = updatedCount;

        for (int markerIndex = 0; markerIndex < updatedCount; ++markerIndex)
        {
            if (beatIndex < updatedMarkers[(size_t) markerIndex])
            {
                insertIndex = markerIndex;
                break;
            }
        }

        for (int markerIndex = updatedCount; markerIndex > insertIndex; --markerIndex)
            updatedMarkers[(size_t) markerIndex] = updatedMarkers[(size_t) (markerIndex - 1)];

        updatedMarkers[(size_t) insertIndex] = beatIndex;
        ++updatedCount;
    }

    loopMarkerRevision.fetch_add(1, std::memory_order_acq_rel);

    for (int markerIndex = 0; markerIndex < updatedCount; ++markerIndex)
        loopMarkerBeats[(size_t) markerIndex].store(updatedMarkers[(size_t) markerIndex], std::memory_order_release);

    loopMarkerCount.store(updatedCount, std::memory_order_release);
    loopMarkerRevision.fetch_add(1, std::memory_order_acq_rel);
}

bool TapeEngine::hasLoopMarkerAtBeat(int64_t beatIndex) const noexcept
{
    std::array<int64_t, maxLoopMarkers> markers {};
    const auto markerCount = readLoopMarkers(markers);

    for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex)
    {
        if (markers[(size_t) markerIndex] == beatIndex)
            return true;
    }

    return false;
}

bool TapeEngine::hasLoopMarkerNearPlayhead() const noexcept
{
    const auto samplesPerBeat = (sampleRate * 60.0) / juce::jmax(30.0, (double) bpm.load(std::memory_order_acquire));
    const auto beatIndex = juce::jmax<int64_t>(0, (int64_t) std::llround(playhead.load(std::memory_order_acquire) / samplesPerBeat));
    return hasLoopMarkerAtBeat(beatIndex);
}

bool TapeEngine::canEditLoopMarkers() const noexcept
{
    return ! isPlaying() && ! isRewinding();
}

void TapeEngine::setMetronomeEnabled(bool shouldBeEnabled)
{
    metronomeEnabled.store(shouldBeEnabled, std::memory_order_release);
}

bool TapeEngine::isMetronomeEnabled() const noexcept
{
    return metronomeEnabled.load(std::memory_order_acquire);
}

void TapeEngine::setPlayheadSample(double samplePosition)
{
    rewinding.store(false, std::memory_order_release);
    const auto clampedPosition = juce::jmax(0.0, samplePosition);
    displayPlayhead.store(clampedPosition, std::memory_order_release);
    requestedPlayhead.store(clampedPosition, std::memory_order_release);
}

double TapeEngine::getPlayheadSample() const noexcept
{
    return playhead.load(std::memory_order_acquire);
}

double TapeEngine::getDisplayPlayheadSample() const noexcept
{
    return displayPlayhead.load(std::memory_order_acquire);
}

double TapeEngine::getDisplayLengthSamples() const noexcept
{
    const auto minimumLength = sampleRate * 10.0;
    const auto recordedLength = (double) getMaxRecordedLength();
    const auto visiblePlayhead = playhead.load(std::memory_order_acquire) + (sampleRate * 2.0);
    return juce::jmax(minimumLength, juce::jmax(recordedLength, visiblePlayhead));
}

void TapeEngine::setTrackRecordArmed(int trackIndex, bool shouldBeArmed)
{
    setTrackRecordMode(trackIndex, shouldBeArmed ? TrackRecordMode::overdub
                                                 : TrackRecordMode::off);
}

bool TapeEngine::isTrackRecordArmed(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return false;

    return getTrackRecordMode(trackIndex) != TrackRecordMode::off;
}

void TapeEngine::setTrackRecordMode(int trackIndex, TrackRecordMode mode)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].pendingRecordMode.store((int) TrackRecordMode::off, std::memory_order_release);
    tracks[(size_t) trackIndex].recordMode.store((int) mode, std::memory_order_release);
}

void TapeEngine::requestTrackRecordMode(int trackIndex, TrackRecordMode mode)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    if (! isPlaying() || mode == TrackRecordMode::off)
    {
        setTrackRecordMode(trackIndex, mode);
        return;
    }

    tracks[(size_t) trackIndex].pendingRecordMode.store((int) mode, std::memory_order_release);
}

TrackRecordMode TapeEngine::getTrackRecordMode(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return TrackRecordMode::off;

    return (TrackRecordMode) tracks[(size_t) trackIndex].recordMode.load(std::memory_order_acquire);
}

TrackRecordMode TapeEngine::getPendingTrackRecordMode(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return TrackRecordMode::off;

    return (TrackRecordMode) tracks[(size_t) trackIndex].pendingRecordMode.load(std::memory_order_acquire);
}

void TapeEngine::setTrackInputGain(int trackIndex, float gain)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].inputGain.store(juce::jlimit(0.0f, 2.0f, gain), std::memory_order_release);
}

float TapeEngine::getTrackInputGain(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 1.0f;

    return tracks[(size_t) trackIndex].inputGain.load(std::memory_order_acquire);
}

void TapeEngine::setTrackInputSource(int trackIndex, int inputSource)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].inputSource.store(juce::jmax(0, inputSource), std::memory_order_release);
}

int TapeEngine::getTrackInputSource(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0;

    return tracks[(size_t) trackIndex].inputSource.load(std::memory_order_acquire);
}

void TapeEngine::setTrackFilterMorph(int trackIndex, float morph)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].filterMorph.store(juce::jlimit(-1.0f, 1.0f, morph), std::memory_order_release);
}

float TapeEngine::getTrackFilterMorph(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].filterMorph.load(std::memory_order_acquire);
}

void TapeEngine::setTrackMuted(int trackIndex, bool shouldBeMuted)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].muted.store(shouldBeMuted, std::memory_order_release);
}

bool TapeEngine::isTrackMuted(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return false;

    return tracks[(size_t) trackIndex].muted.load(std::memory_order_acquire);
}

void TapeEngine::setTrackSolo(int trackIndex, bool shouldBeSoloed)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    soloTrack.store(shouldBeSoloed ? trackIndex : -1, std::memory_order_release);
}

bool TapeEngine::isTrackSolo(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return false;

    return soloTrack.load(std::memory_order_acquire) == trackIndex;
}

int TapeEngine::getSoloTrack() const noexcept
{
    return soloTrack.load(std::memory_order_acquire);
}

float TapeEngine::getTrackPeakMeter(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].peakMeter.load(std::memory_order_acquire);
}

bool TapeEngine::getTrackClipping(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return false;

    return tracks[(size_t) trackIndex].clipping.load(std::memory_order_acquire);
}

void TapeEngine::clearTrackClipping(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].clipping.store(false, std::memory_order_release);
}

int TapeEngine::getTrackRecordedLength(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0;

    return tracks[(size_t) trackIndex].recordedLength.load(std::memory_order_acquire);
}

float TapeEngine::getTrackSample(int trackIndex, int channel, int samplePosition) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return readTrackSample(tracks[(size_t) trackIndex],
                           juce::jlimit(0, Track::numChannels - 1, channel),
                           samplePosition);
}

void TapeEngine::servicePendingAllocations()
{
    const auto chunkCount = juce::jlimit(initialChunkCount,
                                         Track::maxChunks,
                                         requiredChunkCount.load(std::memory_order_acquire));

    for (auto& track : tracks)
    {
        for (int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
            track.ensureChunkAllocated(chunkIndex);
    }
}

void TapeEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    sampleRate = juce::jmax(1.0, device->getCurrentSampleRate());
    preparedBlockSize = juce::jmax(256, device->getCurrentBufferSizeSamples());
    inputScratch.setSize(Track::numChannels, preparedBlockSize, false, false, true);
    inputScratch.clear();
    requiredChunkCount.store(initialChunkCount, std::memory_order_release);
    clickSamplesRemaining = 0;
    clickTotalSamples = juce::jmax(1, (int) std::round(sampleRate * 0.035));
    clickPhase = 0.0;
    clickPhaseDelta = 0.0;
    clickAmplitude = 0.0f;

    for (auto& track : tracks)
    {
        track.reset();
    }

    servicePendingAllocations();
}

void TapeEngine::audioDeviceStopped()
{
    playing.store(false, std::memory_order_release);
    rewinding.store(false, std::memory_order_release);
    clickSamplesRemaining = 0;
}

void TapeEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                  int numInputChannels,
                                                  float* const* outputChannelData,
                                                  int numOutputChannels,
                                                  int numSamples,
                                                  const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
    }

    if (numSamples <= 0)
        return;

    if (numSamples > inputScratch.getNumSamples())
        return;

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

    auto requestedPosition = requestedPlayhead.exchange(-1.0, std::memory_order_acq_rel);
    auto localPlayhead = (int) juce::jmax(0.0, playhead.load(std::memory_order_acquire));

    if (requestedPosition >= 0.0)
    {
        localPlayhead = (int) juce::jmax(0.0, requestedPosition);
        playhead.store((double) localPlayhead, std::memory_order_release);
        displayPlayhead.store((double) localPlayhead, std::memory_order_release);
        syncWritePositionsToPlayhead(localPlayhead);
    }

    if (startRequested.exchange(false, std::memory_order_acq_rel))
        syncWritePositionsToPlayhead(localPlayhead);

    displayPlayhead.store((double) localPlayhead, std::memory_order_release);

    const auto shouldPlay = playing.load(std::memory_order_acquire);
    const auto shouldRewind = rewinding.load(std::memory_order_acquire);
    const auto currentBpm = juce::jmax(30.0, (double) bpm.load(std::memory_order_acquire));
    const auto samplesPerBeat = (sampleRate * 60.0) / currentBpm;
    const auto currentBeatsPerBar = juce::jmax(1, beatsPerBar.load(std::memory_order_acquire));
    const auto metronomeOn = metronomeEnabled.load(std::memory_order_acquire);
    const auto soloedTrack = soloTrack.load(std::memory_order_acquire);
    std::array<int64_t, maxLoopMarkers> loopMarkers {};
    const auto loopMarkerCountSnapshot = readLoopMarkers(loopMarkers);

    if (shouldPlay)
    {
        const auto endSample = localPlayhead + numSamples;
        const auto neededChunks = juce::jlimit(initialChunkCount,
                                               Track::maxChunks,
                                               (endSample / Track::chunkSize) + allocationLeadChunks);
        requiredChunkCount.store(neededChunks, std::memory_order_release);
    }

    std::array<float, numTracks> blockPeaks {};
    std::array<bool, numTracks> blockClips {};

    auto* outputLeft = numOutputChannels > 0 ? outputChannelData[0] : nullptr;
    auto* outputRight = numOutputChannels > 1 ? outputChannelData[1] : nullptr;

    auto transportSample = localPlayhead;

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        auto tapeSample = transportSample;
        const auto rawPreviousBeatIndex = (int64_t) std::floor(((double) tapeSample - 1.0) / samplesPerBeat);
        const auto rawCurrentBeatIndex = (int64_t) std::floor((double) tapeSample / samplesPerBeat);
        int64_t loopStartBeat = 0;

        if (shouldPlay && rawCurrentBeatIndex != rawPreviousBeatIndex
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

        if (currentBeatIndex != previousBeatIndex && currentBeatIndex % currentBeatsPerBar == 0)
            applyPendingRecordModes();

        auto mixedLeft = 0.0f;
        auto mixedRight = 0.0f;

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
        {
            auto& track = tracks[(size_t) trackIndex];
            const auto recordMode = (TrackRecordMode) track.recordMode.load(std::memory_order_acquire);
            const auto isMuted = track.muted.load(std::memory_order_acquire);
            const auto isAudible = (soloedTrack < 0 || soloedTrack == trackIndex) && ! isMuted;

            auto trackLeft = shouldPlay ? readTrackSample(track, 0, tapeSample) : 0.0f;
            auto trackRight = shouldPlay ? readTrackSample(track, 1, tapeSample) : 0.0f;

            if (shouldPlay && recordMode != TrackRecordMode::off)
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
                    const auto processedLeft = processInputSample(track,
                                                                 0,
                                                                 getInputSampleForTrack(track,
                                                                                        inputChannelData,
                                                                                        numInputChannels,
                                                                                        0,
                                                                                        sampleIndex));
                    const auto processedRight = processInputSample(track,
                                                                  1,
                                                                  getInputSampleForTrack(track,
                                                                                         inputChannelData,
                                                                                         numInputChannels,
                                                                                         1,
                                                                                         sampleIndex));
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

            if (isAudible)
            {
                mixedLeft += trackLeft;
                mixedRight += trackRight;
            }

            const auto peak = juce::jmax(std::abs(trackLeft), std::abs(trackRight));
            blockPeaks[(size_t) trackIndex] = juce::jmax(blockPeaks[(size_t) trackIndex], peak);
            blockClips[(size_t) trackIndex] = blockClips[(size_t) trackIndex] || peak > 1.0f;
        }

        if (shouldPlay && metronomeOn)
        {
            if (currentBeatIndex != previousBeatIndex)
            {
                const auto isBarStart = currentBeatIndex % currentBeatsPerBar == 0;
                clickTotalSamples = juce::jmax(1, (int) std::round(sampleRate * (isBarStart ? 0.045 : 0.03)));
                clickSamplesRemaining = clickTotalSamples;
                clickPhase = 0.0;
                clickPhaseDelta = juce::MathConstants<double>::twoPi * (isBarStart ? 1760.0 : 1320.0) / sampleRate;
                clickAmplitude = isBarStart ? 0.24f : 0.16f;
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

        if (outputLeft != nullptr)
            outputLeft[sampleIndex] = mixedLeft;

        if (outputRight != nullptr)
            outputRight[sampleIndex] = mixedRight;
        else if (outputLeft != nullptr && numOutputChannels == 1)
            outputLeft[sampleIndex] = 0.5f * (mixedLeft + mixedRight);

        transportSample = tapeSample + 1;
    }

    for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
    {
        auto& track = tracks[(size_t) trackIndex];
        const auto previousPeak = track.peakMeter.load(std::memory_order_relaxed) * 0.82f;
        track.peakMeter.store(juce::jmax(blockPeaks[(size_t) trackIndex], previousPeak), std::memory_order_relaxed);
        track.clipping.store(blockClips[(size_t) trackIndex], std::memory_order_relaxed);
    }

    if (! shouldPlay)
    {
        if (shouldRewind)
        {
            const auto rewindAmount = juce::jmax((int) std::round(numSamples * rewindSpeedMultiplier), 512);
            const auto rewoundPlayhead = juce::jmax(0, localPlayhead - rewindAmount);
            playhead.store((double) rewoundPlayhead, std::memory_order_release);
            displayPlayhead.store((double) rewoundPlayhead, std::memory_order_release);

            if (rewoundPlayhead == 0)
                rewinding.store(false, std::memory_order_release);
        }

        return;
    }

    const auto newPlayhead = transportSample;
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

float TapeEngine::getInputSampleForTrack(const Track& track,
                                         const float* const* inputChannelData,
                                         int numInputChannels,
                                         int outputChannel,
                                         int sampleIndex) const noexcept
{
    if (numInputChannels <= 0 || inputChannelData == nullptr)
        return 0.0f;

    const auto stereoPairCount = numInputChannels / 2;
    const auto selectedSource = juce::jmax(0, track.inputSource.load(std::memory_order_relaxed));

    if (stereoPairCount > 0 && selectedSource < stereoPairCount)
    {
        const auto leftChannel = selectedSource * 2;
        const auto rightChannel = juce::jmin(leftChannel + 1, numInputChannels - 1);
        const auto channelIndex = outputChannel == 0 ? leftChannel : rightChannel;
        const auto* source = inputChannelData[channelIndex];
        return source != nullptr ? source[sampleIndex] : 0.0f;
    }

    const auto monoSourceIndex = selectedSource - stereoPairCount;
    const auto monoChannel = juce::jlimit(0, numInputChannels - 1, monoSourceIndex);
    const auto* source = inputChannelData[monoChannel];
    return source != nullptr ? source[sampleIndex] : 0.0f;
}

float TapeEngine::processInputSample(Track& track, int channel, float sample) noexcept
{
    auto processed = sample * track.inputGain.load(std::memory_order_relaxed);
    const auto morph = juce::jlimit(-1.0f, 1.0f, track.filterMorph.load(std::memory_order_relaxed));
    const auto lowpassCutoff = juce::jmap(-juce::jmin(morph, 0.0f), 16000.0f, 260.0f);
    const auto highpassCutoff = juce::jmap(juce::jmax(morph, 0.0f), 40.0f, 5200.0f);
    const auto bandHighpassCutoff = 450.0f;
    const auto bandLowpassCutoff = 2800.0f;

    const auto lowpassAlpha = std::exp(-juce::MathConstants<float>::twoPi * (lowpassCutoff / (float) sampleRate));
    auto& lowpassState = track.lowpassState[channel];
    const auto lowpassOutput = ((1.0f - lowpassAlpha) * processed) + (lowpassAlpha * lowpassState);
    lowpassState = lowpassOutput;

    const auto highpassAlpha = std::exp(-juce::MathConstants<float>::twoPi * (highpassCutoff / (float) sampleRate));
    auto& highpassOutputState = track.highpassState[channel];
    auto& highpassInputState = track.highpassInputState[channel];
    const auto highpassOutput = highpassAlpha * (highpassOutputState + processed - highpassInputState);
    highpassOutputState = highpassOutput;
    highpassInputState = processed;

    const auto bandHighpassAlpha = std::exp(-juce::MathConstants<float>::twoPi * (bandHighpassCutoff / (float) sampleRate));
    auto& bandHighpassOutputState = track.bandHighpassState[channel];
    auto& bandHighpassInputState = track.bandHighpassInputState[channel];
    const auto bandHighpassOutput = bandHighpassAlpha * (bandHighpassOutputState + processed - bandHighpassInputState);
    bandHighpassOutputState = bandHighpassOutput;
    bandHighpassInputState = processed;

    const auto bandLowpassAlpha = std::exp(-juce::MathConstants<float>::twoPi * (bandLowpassCutoff / (float) sampleRate));
    auto& bandLowpassState = track.bandLowpassState[channel];
    const auto bandpassOutput = ((1.0f - bandLowpassAlpha) * bandHighpassOutput) + (bandLowpassAlpha * bandLowpassState);
    bandLowpassState = bandpassOutput;

    if (morph < 0.0f)
        return juce::jmap(morph + 1.0f, lowpassOutput, bandpassOutput);

    return juce::jmap(morph, bandpassOutput, highpassOutput);
}

float TapeEngine::readTrackSample(const Track& track, int channel, int samplePosition) const noexcept
{
    if (samplePosition < 0 || samplePosition >= track.recordedLength.load(std::memory_order_acquire))
        return 0.0f;

    const auto chunkIndex = samplePosition / Track::chunkSize;
    const auto offset = samplePosition % Track::chunkSize;
    auto* chunk = track.getChunk(chunkIndex);

    if (chunk == nullptr)
        return 0.0f;

    return chunk->getSample(channel, offset);
}

bool TapeEngine::writeTrackSample(Track& track, int channel, int samplePosition, float value) noexcept
{
    if (samplePosition < 0)
        return false;

    const auto chunkIndex = samplePosition / Track::chunkSize;
    const auto offset = samplePosition % Track::chunkSize;
    auto* chunk = track.getChunk(chunkIndex);

    if (chunk == nullptr)
        return false;

    chunk->setSample(channel, offset, value);
    return true;
}

int TapeEngine::getMaxRecordedLength() const noexcept
{
    auto maxRecordedLength = 0;

    for (const auto& track : tracks)
        maxRecordedLength = juce::jmax(maxRecordedLength, track.recordedLength.load(std::memory_order_acquire));

    return maxRecordedLength;
}

int TapeEngine::readLoopMarkers(std::array<int64_t, maxLoopMarkers>& destination) const noexcept
{
    const auto revisionStart = loopMarkerRevision.load(std::memory_order_acquire);

    if ((revisionStart & 1) != 0)
        return 0;

    const auto markerCount = juce::jlimit(0, maxLoopMarkers, loopMarkerCount.load(std::memory_order_acquire));

    for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex)
        destination[(size_t) markerIndex] = loopMarkerBeats[(size_t) markerIndex].load(std::memory_order_acquire);

    const auto revisionEnd = loopMarkerRevision.load(std::memory_order_acquire);

    if (revisionStart != revisionEnd || (revisionEnd & 1) != 0)
        return 0;

    return markerCount;
}

bool TapeEngine::getLoopStartBeat(int64_t beatIndex,
                                  const std::array<int64_t, maxLoopMarkers>& markers,
                                  int markerCount,
                                  int64_t& loopStartBeat) const noexcept
{
    for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex)
    {
        if (markers[(size_t) markerIndex] != beatIndex)
            continue;

        loopStartBeat = markerIndex > 0 ? markers[(size_t) (markerIndex - 1)] : 0;
        return true;
    }

    return false;
}

void TapeEngine::applyPendingRecordModes() noexcept
{
    for (auto& track : tracks)
    {
        const auto pendingMode = (TrackRecordMode) track.pendingRecordMode.exchange((int) TrackRecordMode::off,
                                                                                    std::memory_order_acq_rel);

        if (pendingMode != TrackRecordMode::off)
            track.recordMode.store((int) pendingMode, std::memory_order_release);
    }
}

void TapeEngine::syncWritePositionsToPlayhead(int samplePosition) noexcept
{
    for (auto& track : tracks)
    {
        if ((TrackRecordMode) track.recordMode.load(std::memory_order_relaxed) != TrackRecordMode::off)
            track.writePosition = samplePosition;
    }
}
