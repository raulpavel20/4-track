#pragma once

#include <JuceHeader.h>

#include "Track.h"

#include <array>
#include <atomic>
#include <cstdint>

class TapeEngine : public juce::AudioIODeviceCallback
{
public:
    static constexpr int numTracks = 4;
    static constexpr int initialChunkCount = 8;
    static constexpr int allocationLeadChunks = 8;
    static constexpr double rewindSpeedMultiplier = 12.0;
    static constexpr int defaultBeatsPerBar = 4;
    static constexpr int maxLoopMarkers = 128;

    TapeEngine();
    ~TapeEngine() override = default;

    void play();
    void stop();
    void rewind();
    bool isPlaying() const noexcept;
    bool isRewinding() const noexcept;
    double getSampleRate() const noexcept;
    void setBpm(float newBpm);
    float getBpm() const noexcept;
    void setBeatsPerBar(int newBeatsPerBar);
    int getBeatsPerBar() const noexcept;
    void setMetronomeEnabled(bool shouldBeEnabled);
    bool isMetronomeEnabled() const noexcept;
    void toggleLoopMarkerAtNearestBeat();
    bool hasLoopMarkerAtBeat(int64_t beatIndex) const noexcept;
    bool hasLoopMarkerNearPlayhead() const noexcept;
    bool canEditLoopMarkers() const noexcept;

    void setPlayheadSample(double samplePosition);
    double getPlayheadSample() const noexcept;
    double getDisplayPlayheadSample() const noexcept;
    double getDisplayLengthSamples() const noexcept;

    void setTrackRecordArmed(int trackIndex, bool shouldBeArmed);
    bool isTrackRecordArmed(int trackIndex) const noexcept;
    void setTrackRecordMode(int trackIndex, TrackRecordMode mode);
    void requestTrackRecordMode(int trackIndex, TrackRecordMode mode);
    TrackRecordMode getTrackRecordMode(int trackIndex) const noexcept;
    TrackRecordMode getPendingTrackRecordMode(int trackIndex) const noexcept;

    void setTrackInputGain(int trackIndex, float gain);
    float getTrackInputGain(int trackIndex) const noexcept;

    void setTrackInputSource(int trackIndex, int inputSource);
    int getTrackInputSource(int trackIndex) const noexcept;

    int addTrackModule(int trackIndex, ChainModuleType type);
    void removeTrackModule(int trackIndex, int moduleIndex);
    int getTrackModuleCount(int trackIndex) const noexcept;
    ChainModuleType getTrackModuleType(int trackIndex, int moduleIndex) const noexcept;
    bool isTrackModulePresent(int trackIndex, int moduleIndex) const noexcept;
    void setTrackFilterMorph(int trackIndex, int moduleIndex, float morph);
    float getTrackFilterMorph(int trackIndex, int moduleIndex) const noexcept;
    void setTrackEqBandGainDb(int trackIndex, int moduleIndex, int bandIndex, float gainDb);
    float getTrackEqBandGainDb(int trackIndex, int moduleIndex, int bandIndex) const noexcept;
    void setTrackEqBandQ(int trackIndex, int moduleIndex, int bandIndex, float q);
    float getTrackEqBandQ(int trackIndex, int moduleIndex, int bandIndex) const noexcept;
    void setTrackEqBandFrequency(int trackIndex, int moduleIndex, int bandIndex, float frequencyHz);
    float getTrackEqBandFrequency(int trackIndex, int moduleIndex, int bandIndex) const noexcept;
    void setTrackCompressorThresholdDb(int trackIndex, int moduleIndex, float thresholdDb);
    float getTrackCompressorThresholdDb(int trackIndex, int moduleIndex) const noexcept;
    void setTrackCompressorRatio(int trackIndex, int moduleIndex, float ratio);
    float getTrackCompressorRatio(int trackIndex, int moduleIndex) const noexcept;
    void setTrackCompressorAttackMs(int trackIndex, int moduleIndex, float attackMs);
    float getTrackCompressorAttackMs(int trackIndex, int moduleIndex) const noexcept;
    void setTrackCompressorReleaseMs(int trackIndex, int moduleIndex, float releaseMs);
    float getTrackCompressorReleaseMs(int trackIndex, int moduleIndex) const noexcept;
    void setTrackCompressorMakeupGainDb(int trackIndex, int moduleIndex, float gainDb);
    float getTrackCompressorMakeupGainDb(int trackIndex, int moduleIndex) const noexcept;
    void setTrackSaturationMode(int trackIndex, int moduleIndex, SaturationMode mode);
    SaturationMode getTrackSaturationMode(int trackIndex, int moduleIndex) const noexcept;
    void setTrackSaturationAmount(int trackIndex, int moduleIndex, float amount);
    float getTrackSaturationAmount(int trackIndex, int moduleIndex) const noexcept;
    void setTrackGainModuleGainDb(int trackIndex, int moduleIndex, float gainDb);
    float getTrackGainModuleGainDb(int trackIndex, int moduleIndex) const noexcept;
    float getTrackModuleInputMeter(int trackIndex, int moduleIndex) const noexcept;
    float getTrackModuleOutputMeter(int trackIndex, int moduleIndex) const noexcept;
    void setTrackMuted(int trackIndex, bool shouldBeMuted);
    bool isTrackMuted(int trackIndex) const noexcept;
    void setTrackSolo(int trackIndex, bool shouldBeSoloed);
    bool isTrackSolo(int trackIndex) const noexcept;
    int getSoloTrack() const noexcept;

    float getTrackPeakMeter(int trackIndex) const noexcept;
    bool getTrackClipping(int trackIndex) const noexcept;
    void clearTrackClipping(int trackIndex);
    int getTrackRecordedLength(int trackIndex) const noexcept;
    float getTrackSample(int trackIndex, int channel, int samplePosition) const noexcept;

    void servicePendingAllocations();

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

private:
    std::array<Track, numTracks> tracks;
    std::atomic<bool> playing { false };
    std::atomic<bool> rewinding { false };
    std::atomic<bool> startRequested { false };
    std::atomic<float> bpm { 120.0f };
    std::atomic<int> beatsPerBar { defaultBeatsPerBar };
    std::atomic<bool> metronomeEnabled { false };
    std::atomic<double> playhead { 0.0 };
    std::atomic<double> displayPlayhead { 0.0 };
    std::atomic<double> requestedPlayhead { -1.0 };
    std::atomic<int> requiredChunkCount { initialChunkCount };
    std::atomic<int> soloTrack { -1 };
    std::array<std::atomic<int64_t>, maxLoopMarkers> loopMarkerBeats;
    std::atomic<int> loopMarkerCount { 0 };
    std::atomic<int> loopMarkerRevision { 0 };
    double sampleRate = 44100.0;
    int preparedBlockSize = 512;
    juce::AudioBuffer<float> inputScratch;
    int clickSamplesRemaining = 0;
    int clickTotalSamples = 1;
    double clickPhase = 0.0;
    double clickPhaseDelta = 0.0;
    float clickAmplitude = 0.0f;

    float getInputSampleForTrack(const Track& track,
                                 const float* const* inputChannelData,
                                 int numInputChannels,
                                 int outputChannel,
                                 int sampleIndex) const noexcept;
    void applyPendingModuleChanges() noexcept;
    void resetModuleState(Track& track, int moduleIndex, ChainModuleType type) noexcept;
    void resetModuleParameters(Track& track, int moduleIndex, ChainModuleType type) noexcept;
    float processInputSample(Track& track, int channel, float sample) noexcept;
    float processChainModule(Track& track, int moduleIndex, int channel, float sample) noexcept;
    float processFilterModule(Track& track, int moduleIndex, int channel, float sample) noexcept;
    float processEqModule(Track& track, int moduleIndex, int channel, float sample) noexcept;
    float processCompressorModule(Track& track, int moduleIndex, int channel, float sample) noexcept;
    float processSaturationModule(Track& track, int moduleIndex, int channel, float sample) noexcept;
    float processGainModule(Track& track, int moduleIndex, int channel, float sample) noexcept;
    float readTrackSample(const Track& track, int channel, int samplePosition) const noexcept;
    bool writeTrackSample(Track& track, int channel, int samplePosition, float value) noexcept;
    int getMaxRecordedLength() const noexcept;
    int readLoopMarkers(std::array<int64_t, maxLoopMarkers>& destination) const noexcept;
    bool getLoopStartBeat(int64_t beatIndex,
                          const std::array<int64_t, maxLoopMarkers>& markers,
                          int markerCount,
                          int64_t& loopStartBeat) const noexcept;
    void applyPendingRecordModes() noexcept;
    void syncWritePositionsToPlayhead(int samplePosition) noexcept;
};
