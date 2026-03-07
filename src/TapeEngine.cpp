#include "TapeEngine.h"

#include <cmath>

namespace
{
float clampEqFrequency(float frequency) noexcept
{
    return juce::jlimit(20.0f, 20000.0f, frequency);
}

float clampEqQ(float q) noexcept
{
    return juce::jlimit(0.1f, 10.0f, q);
}

float clampDbGain(float gainDb, float minimum, float maximum) noexcept
{
    return juce::jlimit(minimum, maximum, gainDb);
}

float clampUnit(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float clampPan(float value) noexcept
{
    return juce::jlimit(-1.0f, 1.0f, value);
}

float clampDelayTimeMs(float value) noexcept
{
    return juce::jlimit(20.0f, 2000.0f, value);
}

float calculatePeakFilterSample(float input, EqBandState& state, size_t channelIndex) noexcept
{
    const auto output = (state.b0 * input) + state.z1[channelIndex];
    state.z1[channelIndex] = (state.b1 * input) - (state.a1 * output) + state.z2[channelIndex];
    state.z2[channelIndex] = (state.b2 * input) - (state.a2 * output);
    return output;
}

void updatePeakFilterCoefficients(EqBandState& state, double sampleRate, float frequency, float q, float gainDb) noexcept
{
    const auto clampedFrequency = clampEqFrequency(frequency);
    const auto clampedQ = clampEqQ(q);

    if (std::abs(state.lastFrequency - clampedFrequency) < 0.001f
        && std::abs(state.lastQ - clampedQ) < 0.001f
        && std::abs(state.lastGainDb - gainDb) < 0.001f)
        return;

    const auto a = std::pow(10.0f, gainDb / 40.0f);
    const auto omega = juce::MathConstants<float>::twoPi * (clampedFrequency / (float) sampleRate);
    const auto sn = std::sin(omega);
    const auto cs = std::cos(omega);
    const auto alpha = sn / (2.0f * clampedQ);

    const auto b0 = 1.0f + (alpha * a);
    const auto b1 = -2.0f * cs;
    const auto b2 = 1.0f - (alpha * a);
    const auto a0 = 1.0f + (alpha / a);
    const auto a1 = -2.0f * cs;
    const auto a2 = 1.0f - (alpha / a);

    state.b0 = b0 / a0;
    state.b1 = b1 / a0;
    state.b2 = b2 / a0;
    state.a1 = a1 / a0;
    state.a2 = a2 / a0;
    state.lastFrequency = clampedFrequency;
    state.lastQ = clampedQ;
    state.lastGainDb = gainDb;
}
}

TapeEngine::TapeEngine()
{
    for (auto& loopMarkerBeat : loopMarkerBeats)
        loopMarkerBeat.store(-1, std::memory_order_relaxed);
}

void TapeEngine::play()
{
    rewinding.store(false, std::memory_order_release);
    reversePlaying.store(false, std::memory_order_release);
    startRequested.store(true, std::memory_order_release);
    playing.store(true, std::memory_order_release);
}

void TapeEngine::stop()
{
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
    playing.store(false, std::memory_order_release);
    reversePlaying.store(false, std::memory_order_release);
    rewinding.store(true, std::memory_order_release);
}

void TapeEngine::startReversePlayback()
{
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
    return ! isPlaying() && ! isRewinding() && ! isReversePlaying();
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
        return makeInputSourceId(InputSourceType::hardwareStereo, 0);

    return tracks[(size_t) trackIndex].inputSource.load(std::memory_order_acquire);
}

juce::Array<TapeEngine::InputSourceOption> TapeEngine::getAvailableInputSources(const juce::StringArray& hardwareInputNames,
                                                                                int destinationTrackIndex) const
{
    juce::Array<InputSourceOption> options;

    for (int pairIndex = 0; pairIndex + 1 < hardwareInputNames.size(); pairIndex += 2)
    {
        options.add({ makeInputSourceId(InputSourceType::hardwareStereo, pairIndex / 2),
                      "Stereo " + juce::String(pairIndex + 1) + "/" + juce::String(pairIndex + 2) });
    }

    for (int channelIndex = 0; channelIndex < hardwareInputNames.size(); ++channelIndex)
    {
        auto label = hardwareInputNames[channelIndex].trim();

        if (label.isEmpty())
            label = "Input " + juce::String(channelIndex + 1);

        options.add({ makeInputSourceId(InputSourceType::hardwareMono, channelIndex),
                      "Mono " + juce::String(channelIndex + 1) + " - " + label });
    }

    for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
    {
        if (trackIndex == destinationTrackIndex)
            continue;

        options.add({ makeInputSourceId(InputSourceType::trackBus, trackIndex),
                      "Track " + juce::String(trackIndex + 1) });
    }

    options.add({ makeInputSourceId(InputSourceType::masterBus, 0), "Master" });

    return options;
}

int TapeEngine::addTrackModule(int trackIndex, ChainModuleType type)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return -1;

    if (type == ChainModuleType::none)
        return -1;

    auto& track = tracks[(size_t) trackIndex];
    auto targetIndex = -1;
    auto lastActiveIndex = -1;

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        if ((ChainModuleType) track.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire) != ChainModuleType::none)
            lastActiveIndex = moduleIndex;
    }

    for (int moduleIndex = lastActiveIndex + 1; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        if ((ChainModuleType) track.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire) == ChainModuleType::none)
        {
            targetIndex = moduleIndex;
            break;
        }
    }

    if (targetIndex < 0)
    {
        for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
        {
            if ((ChainModuleType) track.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire) == ChainModuleType::none)
            {
                targetIndex = moduleIndex;
                break;
            }
        }
    }

    if (targetIndex < 0)
        return -1;

    resetModuleParameters(track, targetIndex, type);
    track.moduleTypes[(size_t) targetIndex].store((int) type, std::memory_order_release);
    track.moduleResetRequested[(size_t) targetIndex].store(true, std::memory_order_release);
    return targetIndex;
}

void TapeEngine::removeTrackModule(int trackIndex, int moduleIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    auto& track = tracks[(size_t) trackIndex];
    resetModuleParameters(track, moduleIndex, ChainModuleType::none);
    track.moduleTypes[(size_t) moduleIndex].store((int) ChainModuleType::none, std::memory_order_release);
    track.moduleResetRequested[(size_t) moduleIndex].store(true, std::memory_order_release);
}

int TapeEngine::getTrackModuleCount(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0;

    auto count = 0;
    const auto& track = tracks[(size_t) trackIndex];

    for (const auto& moduleType : track.moduleTypes)
    {
        if ((ChainModuleType) moduleType.load(std::memory_order_acquire) != ChainModuleType::none)
            ++count;
    }

    return count;
}

ChainModuleType TapeEngine::getTrackModuleType(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return ChainModuleType::none;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return ChainModuleType::none;

    return (ChainModuleType) tracks[(size_t) trackIndex].moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

bool TapeEngine::isTrackModulePresent(int trackIndex, int moduleIndex) const noexcept
{
    return getTrackModuleType(trackIndex, moduleIndex) != ChainModuleType::none;
}

void TapeEngine::setTrackModuleBypassed(int trackIndex, int moduleIndex, bool shouldBeBypassed)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].moduleBypassed[(size_t) moduleIndex].store(shouldBeBypassed, std::memory_order_release);
}

bool TapeEngine::isTrackModuleBypassed(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return false;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return false;

    return tracks[(size_t) trackIndex].moduleBypassed[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackFilterMorph(int trackIndex, int moduleIndex, float morph)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].filterMorphs[(size_t) moduleIndex].store(juce::jlimit(-1.0f, 1.0f, morph), std::memory_order_release);
}

float TapeEngine::getTrackFilterMorph(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].filterMorphs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackEqBandGainDb(int trackIndex, int moduleIndex, int bandIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return;

    tracks[(size_t) trackIndex].eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].store(clampDbGain(gainDb, -18.0f, 18.0f),
                                                                                                std::memory_order_release);
}

float TapeEngine::getTrackEqBandGainDb(int trackIndex, int moduleIndex, int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return 0.0f;

    return tracks[(size_t) trackIndex].eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackEqBandQ(int trackIndex, int moduleIndex, int bandIndex, float q)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return;

    tracks[(size_t) trackIndex].eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].store(clampEqQ(q), std::memory_order_release);
}

float TapeEngine::getTrackEqBandQ(int trackIndex, int moduleIndex, int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return 1.0f;

    return tracks[(size_t) trackIndex].eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackEqBandFrequency(int trackIndex, int moduleIndex, int bandIndex, float frequencyHz)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return;

    tracks[(size_t) trackIndex].eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].store(clampEqFrequency(frequencyHz),
                                                                                                    std::memory_order_release);
}

float TapeEngine::getTrackEqBandFrequency(int trackIndex, int moduleIndex, int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return 1000.0f;

    return tracks[(size_t) trackIndex].eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorThresholdDb(int trackIndex, int moduleIndex, float thresholdDb)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorThresholdsDb[(size_t) moduleIndex].store(clampDbGain(thresholdDb, -60.0f, 0.0f),
                                                                                   std::memory_order_release);
}

float TapeEngine::getTrackCompressorThresholdDb(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return -18.0f;

    return tracks[(size_t) trackIndex].compressorThresholdsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorRatio(int trackIndex, int moduleIndex, float ratio)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorRatios[(size_t) moduleIndex].store(juce::jlimit(1.0f, 20.0f, ratio),
                                                                             std::memory_order_release);
}

float TapeEngine::getTrackCompressorRatio(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 4.0f;

    return tracks[(size_t) trackIndex].compressorRatios[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorAttackMs(int trackIndex, int moduleIndex, float attackMs)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorAttacksMs[(size_t) moduleIndex].store(juce::jlimit(0.1f, 100.0f, attackMs),
                                                                                std::memory_order_release);
}

float TapeEngine::getTrackCompressorAttackMs(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 10.0f;

    return tracks[(size_t) trackIndex].compressorAttacksMs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorReleaseMs(int trackIndex, int moduleIndex, float releaseMs)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorReleasesMs[(size_t) moduleIndex].store(juce::jlimit(10.0f, 1000.0f, releaseMs),
                                                                                 std::memory_order_release);
}

float TapeEngine::getTrackCompressorReleaseMs(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 120.0f;

    return tracks[(size_t) trackIndex].compressorReleasesMs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorMakeupGainDb(int trackIndex, int moduleIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorMakeupGainsDb[(size_t) moduleIndex].store(clampDbGain(gainDb, 0.0f, 24.0f),
                                                                                    std::memory_order_release);
}

float TapeEngine::getTrackCompressorMakeupGainDb(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].compressorMakeupGainsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackSaturationMode(int trackIndex, int moduleIndex, SaturationMode mode)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].saturationModes[(size_t) moduleIndex].store((int) mode, std::memory_order_release);
}

SaturationMode TapeEngine::getTrackSaturationMode(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return SaturationMode::light;

    return (SaturationMode) tracks[(size_t) trackIndex].saturationModes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackSaturationAmount(int trackIndex, int moduleIndex, float amount)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].saturationAmounts[(size_t) moduleIndex].store(juce::jlimit(0.0f, 1.0f, amount),
                                                                              std::memory_order_release);
}

float TapeEngine::getTrackSaturationAmount(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.35f;

    return tracks[(size_t) trackIndex].saturationAmounts[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackGainModuleGainDb(int trackIndex, int moduleIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].gainModuleGainsDb[(size_t) moduleIndex].store(clampDbGain(gainDb, -24.0f, 24.0f),
                                                                              std::memory_order_release);
}

float TapeEngine::getTrackGainModuleGainDb(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].gainModuleGainsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getTrackModuleInputMeter(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].moduleInputMeters[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getTrackModuleOutputMeter(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].moduleOutputMeters[(size_t) moduleIndex].load(std::memory_order_acquire);
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

void TapeEngine::setTrackMixerGainDb(int trackIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].mixerGainDb.store(clampDbGain(gainDb, -24.0f, 12.0f), std::memory_order_release);
}

float TapeEngine::getTrackMixerGainDb(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].mixerGainDb.load(std::memory_order_acquire);
}

void TapeEngine::setTrackMixerPan(int trackIndex, float pan)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].mixerPan.store(clampPan(pan), std::memory_order_release);
}

float TapeEngine::getTrackMixerPan(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].mixerPan.load(std::memory_order_acquire);
}

void TapeEngine::setTrackDelaySend(int trackIndex, float amount)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].delaySend.store(clampUnit(amount), std::memory_order_release);
}

float TapeEngine::getTrackDelaySend(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].delaySend.load(std::memory_order_acquire);
}

void TapeEngine::setTrackReverbSend(int trackIndex, float amount)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].reverbSend.store(clampUnit(amount), std::memory_order_release);
}

float TapeEngine::getTrackReverbSend(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].reverbSend.load(std::memory_order_acquire);
}

float TapeEngine::getTrackMixerMeter(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].mixerMeter.load(std::memory_order_acquire);
}

void TapeEngine::setDelayTimeMs(float timeMs)
{
    delayTimeMs.store(clampDelayTimeMs(timeMs), std::memory_order_release);
}

float TapeEngine::getDelayTimeMs() const noexcept
{
    return delayTimeMs.load(std::memory_order_acquire);
}

void TapeEngine::setDelayFeedback(float feedback)
{
    delayFeedback.store(juce::jlimit(0.0f, 0.95f, feedback), std::memory_order_release);
}

float TapeEngine::getDelayFeedback() const noexcept
{
    return delayFeedback.load(std::memory_order_acquire);
}

void TapeEngine::setDelayMix(float mix)
{
    delayMix.store(clampUnit(mix), std::memory_order_release);
}

float TapeEngine::getDelayMix() const noexcept
{
    return delayMix.load(std::memory_order_acquire);
}

void TapeEngine::setReverbSize(float size)
{
    reverbSize.store(clampUnit(size), std::memory_order_release);
}

float TapeEngine::getReverbSize() const noexcept
{
    return reverbSize.load(std::memory_order_acquire);
}

void TapeEngine::setReverbDamping(float damping)
{
    reverbDamping.store(clampUnit(damping), std::memory_order_release);
}

float TapeEngine::getReverbDamping() const noexcept
{
    return reverbDamping.load(std::memory_order_acquire);
}

void TapeEngine::setReverbMix(float mix)
{
    reverbMix.store(clampUnit(mix), std::memory_order_release);
}

float TapeEngine::getReverbMix() const noexcept
{
    return reverbMix.load(std::memory_order_acquire);
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
    reversePlaying.store(false, std::memory_order_release);
    clickSamplesRemaining = 0;
    delayWritePosition = 0;
    delayBuffer.clear();
    reverb.reset();
    lastMasterInputBus.fill(0.0f);

    for (auto& trackBus : lastTrackInputBuses)
        trackBus.fill(0.0f);
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

    applyPendingModuleChanges();

    displayPlayhead.store((double) localPlayhead, std::memory_order_release);

    const auto shouldPlay = playing.load(std::memory_order_acquire);
    const auto shouldRewind = rewinding.load(std::memory_order_acquire);
    const auto shouldReversePlay = reversePlaying.load(std::memory_order_acquire);
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
    juce::Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = reverbSize.load(std::memory_order_acquire);
    reverbParameters.damping = reverbDamping.load(std::memory_order_acquire);
    reverbParameters.wetLevel = 1.0f;
    reverbParameters.dryLevel = 0.0f;
    reverbParameters.width = 1.0f;
    reverbParameters.freezeMode = 0.0f;
    reverb.setParameters(reverbParameters);

    for (auto& track : tracks)
    {
        track.moduleBlockInputPeaks.fill(0.0f);
        track.moduleBlockOutputPeaks.fill(0.0f);
        track.mixerBlockPeak = 0.0f;
    }

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
        auto delayBusLeft = 0.0f;
        auto delayBusRight = 0.0f;
        auto reverbBusLeft = 0.0f;
        auto reverbBusRight = 0.0f;
        std::array<std::array<float, Track::numChannels>, numTracks> trackDryContributions {};
        std::array<std::array<float, Track::numChannels>, numTracks> trackDelayContributions {};
        std::array<std::array<float, Track::numChannels>, numTracks> trackReverbContributions {};

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
        {
            auto& track = tracks[(size_t) trackIndex];
            const auto recordMode = (TrackRecordMode) track.recordMode.load(std::memory_order_acquire);
            const auto isMuted = track.muted.load(std::memory_order_acquire);
            const auto isAudible = (soloedTrack < 0 || soloedTrack == trackIndex) && ! isMuted;

            const auto transportReadsTape = shouldPlay || shouldReversePlay;
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
                const auto reverbSendAmount = track.reverbSend.load(std::memory_order_relaxed);
                mixedLeft += postMixerLeft;
                mixedRight += postMixerRight;
                delayBusLeft += postMixerLeft * delaySendAmount;
                delayBusRight += postMixerRight * delaySendAmount;
                reverbBusLeft += postMixerLeft * reverbSendAmount;
                reverbBusRight += postMixerRight * reverbSendAmount;
                trackDryContributions[(size_t) trackIndex][0] = postMixerLeft;
                trackDryContributions[(size_t) trackIndex][1] = postMixerRight;
                trackDelayContributions[(size_t) trackIndex][0] = postMixerLeft * delaySendAmount;
                trackDelayContributions[(size_t) trackIndex][1] = postMixerRight * delaySendAmount;
                trackReverbContributions[(size_t) trackIndex][0] = postMixerLeft * reverbSendAmount;
                trackReverbContributions[(size_t) trackIndex][1] = postMixerRight * reverbSendAmount;
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

        auto reverbReturnLeft = 0.0f;
        auto reverbReturnRight = 0.0f;
        processReverbReturn(reverbBusLeft, reverbBusRight, reverbReturnLeft, reverbReturnRight);
        mixedLeft += reverbReturnLeft;
        mixedRight += reverbReturnRight;

        auto totalDelayWeight = 0.0f;
        auto totalReverbWeight = 0.0f;
        std::array<std::array<float, Track::numChannels>, numTracks> currentTrackInputBuses {};

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
        {
            totalDelayWeight += std::abs(trackDelayContributions[(size_t) trackIndex][0]) + std::abs(trackDelayContributions[(size_t) trackIndex][1]);
            totalReverbWeight += std::abs(trackReverbContributions[(size_t) trackIndex][0]) + std::abs(trackReverbContributions[(size_t) trackIndex][1]);
        }

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
        {
            const auto delayWeight = std::abs(trackDelayContributions[(size_t) trackIndex][0]) + std::abs(trackDelayContributions[(size_t) trackIndex][1]);
            const auto reverbWeight = std::abs(trackReverbContributions[(size_t) trackIndex][0]) + std::abs(trackReverbContributions[(size_t) trackIndex][1]);
            const auto delayShare = totalDelayWeight > 0.0f ? delayWeight / totalDelayWeight : 0.0f;
            const auto reverbShare = totalReverbWeight > 0.0f ? reverbWeight / totalReverbWeight : 0.0f;
            currentTrackInputBuses[(size_t) trackIndex][0] = trackDryContributions[(size_t) trackIndex][0] + (delayReturnLeft * delayShare) + (reverbReturnLeft * reverbShare);
            currentTrackInputBuses[(size_t) trackIndex][1] = trackDryContributions[(size_t) trackIndex][1] + (delayReturnRight * delayShare) + (reverbReturnRight * reverbShare);
        }

        lastMasterInputBus[0] = mixedLeft;
        lastMasterInputBus[1] = mixedRight;

        for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
            lastTrackInputBuses[(size_t) trackIndex] = currentTrackInputBuses[(size_t) trackIndex];

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
        else
        {
            transportSample = tapeSample + 1;
        }
    }

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

    if (! shouldPlay)
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

float TapeEngine::getInputSampleForTrack(int destinationTrackIndex,
                                         const Track& track,
                                         const float* const* inputChannelData,
                                         int numInputChannels,
                                         int outputChannel,
                                         int sampleIndex) const noexcept
{
    const auto selectedSource = juce::jmax(0, track.inputSource.load(std::memory_order_relaxed));
    const auto sourceType = getInputSourceTypeFromId(selectedSource);
    const auto sourceIndex = getInputSourceIndexFromId(selectedSource);

    if (sourceType == InputSourceType::trackBus)
    {
        if (! juce::isPositiveAndBelow(sourceIndex, numTracks) || sourceIndex == destinationTrackIndex)
            return 0.0f;

        return lastTrackInputBuses[(size_t) sourceIndex][(size_t) juce::jlimit(0, Track::numChannels - 1, outputChannel)];
    }

    if (sourceType == InputSourceType::masterBus)
    {
        const auto channelIndex = (size_t) juce::jlimit(0, Track::numChannels - 1, outputChannel);
        return lastMasterInputBus[channelIndex] - lastTrackInputBuses[(size_t) juce::jlimit(0, numTracks - 1, destinationTrackIndex)][channelIndex];
    }

    if (numInputChannels <= 0 || inputChannelData == nullptr)
        return 0.0f;

    if (sourceType == InputSourceType::hardwareStereo)
    {
        const auto leftChannel = juce::jlimit(0, juce::jmax(0, numInputChannels - 1), sourceIndex * 2);
        const auto rightChannel = juce::jmin(leftChannel + 1, numInputChannels - 1);
        const auto channelIndex = outputChannel == 0 ? leftChannel : rightChannel;
        const auto* source = inputChannelData[channelIndex];
        return source != nullptr ? source[sampleIndex] : 0.0f;
    }

    const auto monoChannel = juce::jlimit(0, numInputChannels - 1, sourceIndex);
    const auto* source = inputChannelData[monoChannel];
    return source != nullptr ? source[sampleIndex] : 0.0f;
}

void TapeEngine::applyPendingModuleChanges() noexcept
{
    for (auto& track : tracks)
    {
        for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
        {
            const auto requestedType = (ChainModuleType) track.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire);
            const auto activeType = (ChainModuleType) track.activeModuleTypes[(size_t) moduleIndex];
            const auto needsReset = track.moduleResetRequested[(size_t) moduleIndex].exchange(false, std::memory_order_acq_rel);

            if (! needsReset && requestedType == activeType)
                continue;

            resetModuleState(track, moduleIndex, requestedType);
            track.activeModuleTypes[(size_t) moduleIndex] = (int) requestedType;
        }
    }
}

void TapeEngine::resetModuleState(Track& track, int moduleIndex, ChainModuleType type) noexcept
{
    track.filterStates[(size_t) moduleIndex].reset();
    track.compressorStates[(size_t) moduleIndex].reset();
    track.moduleBlockInputPeaks[(size_t) moduleIndex] = 0.0f;
    track.moduleBlockOutputPeaks[(size_t) moduleIndex] = 0.0f;
    track.moduleInputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
    track.moduleOutputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);

    for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
        track.eqStates[(size_t) moduleIndex][(size_t) bandIndex].reset();

    track.activeModuleTypes[(size_t) moduleIndex] = (int) type;
}

void TapeEngine::resetModuleParameters(Track& track, int moduleIndex, ChainModuleType type) noexcept
{
    track.moduleBypassed[(size_t) moduleIndex].store(false, std::memory_order_relaxed);
    track.filterMorphs[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
    track.compressorThresholdsDb[(size_t) moduleIndex].store(-18.0f, std::memory_order_relaxed);
    track.compressorRatios[(size_t) moduleIndex].store(4.0f, std::memory_order_relaxed);
    track.compressorAttacksMs[(size_t) moduleIndex].store(10.0f, std::memory_order_relaxed);
    track.compressorReleasesMs[(size_t) moduleIndex].store(120.0f, std::memory_order_relaxed);
    track.compressorMakeupGainsDb[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
    track.saturationModes[(size_t) moduleIndex].store((int) SaturationMode::light, std::memory_order_relaxed);
    track.saturationAmounts[(size_t) moduleIndex].store(0.35f, std::memory_order_relaxed);
    track.gainModuleGainsDb[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
    track.moduleInputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
    track.moduleOutputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);

    for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
    {
        track.eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].store(0.0f, std::memory_order_relaxed);
        track.eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].store(1.0f, std::memory_order_relaxed);
        track.eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].store(Track::getDefaultEqFrequency(bandIndex),
                                                                                 std::memory_order_relaxed);
    }

    juce::ignoreUnused(type);
}

float TapeEngine::processInputSample(Track& track, int channel, float sample) noexcept
{
    auto processed = sample * track.inputGain.load(std::memory_order_relaxed);

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        if ((ChainModuleType) track.activeModuleTypes[(size_t) moduleIndex] == ChainModuleType::none)
            continue;

        processed = processChainModule(track, moduleIndex, channel, processed);
    }

    return processed;
}

float TapeEngine::processChainModule(Track& track, int moduleIndex, int channel, float sample) noexcept
{
    const auto inputPeak = std::abs(sample);
    track.moduleBlockInputPeaks[(size_t) moduleIndex] = juce::jmax(track.moduleBlockInputPeaks[(size_t) moduleIndex], inputPeak);

    if (track.moduleBypassed[(size_t) moduleIndex].load(std::memory_order_relaxed))
    {
        track.moduleBlockOutputPeaks[(size_t) moduleIndex] = juce::jmax(track.moduleBlockOutputPeaks[(size_t) moduleIndex], inputPeak);
        return sample;
    }

    auto output = sample;

    switch ((ChainModuleType) track.activeModuleTypes[(size_t) moduleIndex])
    {
        case ChainModuleType::filter:
            output = processFilterModule(track, moduleIndex, channel, sample);
            break;
        case ChainModuleType::eq:
            output = processEqModule(track, moduleIndex, channel, sample);
            break;
        case ChainModuleType::compressor:
            output = processCompressorModule(track, moduleIndex, channel, sample);
            break;
        case ChainModuleType::saturation:
            output = processSaturationModule(track, moduleIndex, channel, sample);
            break;
        case ChainModuleType::gain:
            output = processGainModule(track, moduleIndex, channel, sample);
            break;
        case ChainModuleType::none:
            break;
    }

    track.moduleBlockOutputPeaks[(size_t) moduleIndex] = juce::jmax(track.moduleBlockOutputPeaks[(size_t) moduleIndex], std::abs(output));
    return output;
}

float TapeEngine::processFilterModule(Track& track, int moduleIndex, int channel, float sample) noexcept
{
    const auto channelIndex = (size_t) juce::jlimit(0, Track::numChannels - 1, channel);
    const auto morph = juce::jlimit(-1.0f,
                                    1.0f,
                                    track.filterMorphs[(size_t) moduleIndex].load(std::memory_order_relaxed));
    const auto lowpassCutoff = juce::jmap(-juce::jmin(morph, 0.0f), 16000.0f, 260.0f);
    const auto highpassCutoff = juce::jmap(juce::jmax(morph, 0.0f), 40.0f, 5200.0f);
    const auto bandHighpassCutoff = 450.0f;
    const auto bandLowpassCutoff = 2800.0f;
    auto& state = track.filterStates[(size_t) moduleIndex];

    const auto lowpassAlpha = std::exp(-juce::MathConstants<float>::twoPi * (lowpassCutoff / (float) sampleRate));
    auto& lowpassState = state.lowpassState[channelIndex];
    const auto lowpassOutput = ((1.0f - lowpassAlpha) * sample) + (lowpassAlpha * lowpassState);
    lowpassState = lowpassOutput;

    const auto highpassAlpha = std::exp(-juce::MathConstants<float>::twoPi * (highpassCutoff / (float) sampleRate));
    auto& highpassOutputState = state.highpassState[channelIndex];
    auto& highpassInputState = state.highpassInputState[channelIndex];
    const auto highpassOutput = highpassAlpha * (highpassOutputState + sample - highpassInputState);
    highpassOutputState = highpassOutput;
    highpassInputState = sample;

    const auto bandHighpassAlpha = std::exp(-juce::MathConstants<float>::twoPi * (bandHighpassCutoff / (float) sampleRate));
    auto& bandHighpassOutputState = state.bandHighpassState[channelIndex];
    auto& bandHighpassInputState = state.bandHighpassInputState[channelIndex];
    const auto bandHighpassOutput = bandHighpassAlpha * (bandHighpassOutputState + sample - bandHighpassInputState);
    bandHighpassOutputState = bandHighpassOutput;
    bandHighpassInputState = sample;

    const auto bandLowpassAlpha = std::exp(-juce::MathConstants<float>::twoPi * (bandLowpassCutoff / (float) sampleRate));
    auto& bandLowpassState = state.bandLowpassState[channelIndex];
    const auto bandpassOutput = ((1.0f - bandLowpassAlpha) * bandHighpassOutput) + (bandLowpassAlpha * bandLowpassState);
    bandLowpassState = bandpassOutput;

    if (morph < 0.0f)
        return juce::jmap(morph + 1.0f, lowpassOutput, bandpassOutput);

    return juce::jmap(morph, bandpassOutput, highpassOutput);
}

float TapeEngine::processEqModule(Track& track, int moduleIndex, int channel, float sample) noexcept
{
    auto processed = sample;
    const auto channelIndex = (size_t) juce::jlimit(0, Track::numChannels - 1, channel);

    for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
    {
        auto& state = track.eqStates[(size_t) moduleIndex][(size_t) bandIndex];
        const auto gainDb = track.eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_relaxed);
        const auto q = track.eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_relaxed);
        const auto frequency = track.eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_relaxed);
        updatePeakFilterCoefficients(state, sampleRate, frequency, q, gainDb);
        processed = calculatePeakFilterSample(processed, state, channelIndex);
    }

    return processed;
}

float TapeEngine::processCompressorModule(Track& track, int moduleIndex, int channel, float sample) noexcept
{
    const auto channelIndex = (size_t) juce::jlimit(0, Track::numChannels - 1, channel);
    const auto thresholdDb = track.compressorThresholdsDb[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto ratio = track.compressorRatios[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto attackMs = track.compressorAttacksMs[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto releaseMs = track.compressorReleasesMs[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto makeupGainDb = track.compressorMakeupGainsDb[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto inputLevel = std::abs(sample);
    const auto inputDb = juce::Decibels::gainToDecibels(inputLevel + 1.0e-6f, -120.0f);
    auto gainReductionDb = 0.0f;

    if (inputDb > thresholdDb)
        gainReductionDb = (thresholdDb + ((inputDb - thresholdDb) / juce::jmax(1.0f, ratio))) - inputDb;

    const auto targetGain = juce::Decibels::decibelsToGain(gainReductionDb);
    const auto attackCoeff = std::exp(-1.0f / juce::jmax(1.0f, (attackMs * 0.001f) * (float) sampleRate));
    const auto releaseCoeff = std::exp(-1.0f / juce::jmax(1.0f, (releaseMs * 0.001f) * (float) sampleRate));
    auto& envelope = track.compressorStates[(size_t) moduleIndex].envelopes[channelIndex];
    const auto coeff = targetGain < envelope ? attackCoeff : releaseCoeff;
    envelope = targetGain + (coeff * (envelope - targetGain));
    return sample * envelope * juce::Decibels::decibelsToGain(makeupGainDb);
}

float TapeEngine::processSaturationModule(Track& track, int moduleIndex, int channel, float sample) noexcept
{
    juce::ignoreUnused(channel);
    const auto amount = track.saturationAmounts[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto drive = 1.0f + (amount * 7.0f);
    const auto pushed = sample * drive;
    const auto mode = (SaturationMode) track.saturationModes[(size_t) moduleIndex].load(std::memory_order_relaxed);

    if (mode == SaturationMode::heavy)
        return std::tanh(pushed) / std::tanh(drive);

    return pushed / (1.0f + (0.6f * std::abs(pushed)));
}

float TapeEngine::processGainModule(Track& track, int moduleIndex, int channel, float sample) noexcept
{
    juce::ignoreUnused(channel);
    return sample * juce::Decibels::decibelsToGain(track.gainModuleGainsDb[(size_t) moduleIndex].load(std::memory_order_relaxed));
}

void TapeEngine::applyTrackMixer(float pan, float& left, float& right) const noexcept
{
    const auto clampedPan = clampPan(pan);

    if (clampedPan < 0.0f)
        right *= 1.0f + clampedPan;
    else if (clampedPan > 0.0f)
        left *= 1.0f - clampedPan;
}

void TapeEngine::processDelayReturn(float inputLeft, float inputRight, float& outputLeft, float& outputRight) noexcept
{
    outputLeft = 0.0f;
    outputRight = 0.0f;

    if (delayBuffer.getNumSamples() <= 0)
        return;

    const auto delaySamples = juce::jlimit(1,
                                           juce::jmax(1, maxDelaySamples - 1),
                                           (int) std::round((sampleRate * clampDelayTimeMs(delayTimeMs.load(std::memory_order_relaxed))) * 0.001));
    const auto readPosition = (delayWritePosition - delaySamples + maxDelaySamples) % maxDelaySamples;
    const auto delayedLeft = delayBuffer.getSample(0, readPosition);
    const auto delayedRight = delayBuffer.getSample(1, readPosition);
    const auto feedback = delayFeedback.load(std::memory_order_relaxed);
    delayBuffer.setSample(0, delayWritePosition, inputLeft + (delayedLeft * feedback));
    delayBuffer.setSample(1, delayWritePosition, inputRight + (delayedRight * feedback));
    delayWritePosition = (delayWritePosition + 1) % maxDelaySamples;
    const auto wet = delayMix.load(std::memory_order_relaxed);
    outputLeft = delayedLeft * wet;
    outputRight = delayedRight * wet;
}

void TapeEngine::processReverbReturn(float inputLeft, float inputRight, float& outputLeft, float& outputRight) noexcept
{
    outputLeft = inputLeft;
    outputRight = inputRight;
    reverb.processStereo(&outputLeft, &outputRight, 1);
    const auto wet = reverbMix.load(std::memory_order_relaxed);
    outputLeft *= wet;
    outputRight *= wet;
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
