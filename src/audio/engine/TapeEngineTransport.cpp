#include "../../TapeEngine.h"

#include "../../AppSettings.h"

#include <cmath>

TapeEngine::TapeEngine()
{
    const auto& settings = AppSettings::getInstance();
    bpm.store((float) settings.getDefaultBpm(), std::memory_order_relaxed);
    beatsPerBar.store(settings.getDefaultBeatsPerBar(), std::memory_order_relaxed);
    metronomeEnabled.store(settings.getDefaultMetronomeEnabled(), std::memory_order_relaxed);
    countInEnabled.store(settings.getDefaultCountInEnabled(), std::memory_order_relaxed);

    for (auto& loopMarkerBeat : loopMarkerBeats)
        loopMarkerBeat.store(-1, std::memory_order_relaxed);
}

void TapeEngine::play()
{
    rewinding.store(false, std::memory_order_release);
    reversePlaying.store(false, std::memory_order_release);

    if (shouldStartCountIn())
    {
        resetCountInState();
        countInRequested.store(true, std::memory_order_release);
        countInActive.store(true, std::memory_order_release);
        countInAudible.store(true, std::memory_order_release);
        startRequested.store(false, std::memory_order_release);
        playing.store(false, std::memory_order_release);
        return;
    }

    resetCountInState();
    startRequested.store(true, std::memory_order_release);
    playing.store(true, std::memory_order_release);
}

void TapeEngine::stop()
{
    resetCountInState();
    playing.store(false, std::memory_order_release);
    rewinding.store(false, std::memory_order_release);
    reversePlaying.store(false, std::memory_order_release);
    clickSamplesRemaining = 0;
    applyPendingRecordModes();

    for (auto& track : tracks)
        track.peakMeter.store(0.0f, std::memory_order_relaxed);
}

void TapeEngine::rewind()
{
    resetCountInState();
    playing.store(false, std::memory_order_release);
    reversePlaying.store(false, std::memory_order_release);
    rewinding.store(true, std::memory_order_release);
}

void TapeEngine::startReversePlayback()
{
    resetCountInState();
    rewinding.store(false, std::memory_order_release);
    playing.store(false, std::memory_order_release);
    reversePlaying.store(true, std::memory_order_release);
}

void TapeEngine::stopReversePlayback()
{
    reversePlaying.store(false, std::memory_order_release);
}

bool TapeEngine::isPlaying() const noexcept
{
    return playing.load(std::memory_order_acquire);
}

bool TapeEngine::isRewinding() const noexcept
{
    return rewinding.load(std::memory_order_acquire);
}

bool TapeEngine::isReversePlaying() const noexcept
{
    return reversePlaying.load(std::memory_order_acquire);
}

void TapeEngine::setCountInEnabled(bool shouldBeEnabled)
{
    countInEnabled.store(shouldBeEnabled, std::memory_order_release);
}

bool TapeEngine::isCountInEnabled() const noexcept
{
    return countInEnabled.load(std::memory_order_acquire);
}

bool TapeEngine::isCountInActive() const noexcept
{
    return countInActive.load(std::memory_order_acquire);
}

int TapeEngine::getMetronomePulseRevision() const noexcept
{
    return metronomePulseRevision.load(std::memory_order_acquire);
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
    return ! isPlaying() && ! isCountInActive() && ! isRewinding() && ! isReversePlaying();
}

bool TapeEngine::getAdjacentLoopMarkerSample(bool forward, double fromSample, double& targetSample) const noexcept
{
    const auto samplesPerBeat = (sampleRate * 60.0) / juce::jmax(30.0, (double) bpm.load(std::memory_order_acquire));
    std::array<int64_t, maxLoopMarkers> markers {};
    const auto markerCount = readLoopMarkers(markers);

    if (markerCount <= 0)
        return false;

    const auto currentBeat = juce::jmax<int64_t>(0, (int64_t) std::llround(fromSample / samplesPerBeat));

    if (forward)
    {
        for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex)
        {
            if (markers[(size_t) markerIndex] > currentBeat)
            {
                targetSample = (double) markers[(size_t) markerIndex] * samplesPerBeat;
                return true;
            }
        }

        return false;
    }

    for (int markerIndex = markerCount - 1; markerIndex >= 0; --markerIndex)
    {
        if (markers[(size_t) markerIndex] < currentBeat)
        {
            targetSample = (double) markers[(size_t) markerIndex] * samplesPerBeat;
            return true;
        }
    }

    targetSample = 0.0;
    return true;
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
    resetCountInState();
    rewinding.store(false, std::memory_order_release);
    reversePlaying.store(false, std::memory_order_release);
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

bool TapeEngine::shouldStartCountIn() const noexcept
{
    if (! countInEnabled.load(std::memory_order_acquire))
        return false;

    if (! metronomeEnabled.load(std::memory_order_acquire))
        return false;

    for (const auto& track : tracks)
    {
        const auto mode = (TrackRecordMode) track.recordMode.load(std::memory_order_acquire);

        if (mode == TrackRecordMode::overdub || mode == TrackRecordMode::replace)
            return true;
    }

    return false;
}

void TapeEngine::resetCountInState() noexcept
{
    countInRequested.store(false, std::memory_order_release);
    countInActive.store(false, std::memory_order_release);
    countInAudible.store(false, std::memory_order_release);
    transportStartPulsePending.store(false, std::memory_order_release);
    countInSamplePosition = 0.0;
    countInTotalBeats = 0;
}

void TapeEngine::triggerMetronomePulse(bool isBarStart) noexcept
{
    clickTotalSamples = juce::jmax(1, (int) std::round(sampleRate * (isBarStart ? 0.045 : 0.03)));
    clickSamplesRemaining = clickTotalSamples;
    clickPhase = 0.0;
    clickPhaseDelta = juce::MathConstants<double>::twoPi * (isBarStart ? 1760.0 : 1320.0) / sampleRate;
    clickAmplitude = isBarStart ? 0.24f : 0.16f;
    metronomePulseRevision.fetch_add(1, std::memory_order_acq_rel);
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
