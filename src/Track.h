#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cmath>
#include <memory>

enum class TrackRecordMode : int
{
    off = 0,
    overdub,
    replace,
    erase
};

enum class ChainModuleType : int
{
    none = 0,
    filter,
    eq,
    compressor,
    saturation,
    delay,
    reverb,
    gain
};

enum class SaturationMode : int
{
    light = 0,
    heavy
};

enum class InputSourceType : int
{
    hardwareStereo = 0,
    hardwareMono,
    trackBus,
    masterBus
};

inline constexpr int makeInputSourceId(InputSourceType type, int index = 0) noexcept
{
    return (((int) type & 0xff) << 24) | (index & 0x00ffffff);
}

inline constexpr InputSourceType getInputSourceTypeFromId(int sourceId) noexcept
{
    return (InputSourceType) ((sourceId >> 24) & 0xff);
}

inline constexpr int getInputSourceIndexFromId(int sourceId) noexcept
{
    return sourceId & 0x00ffffff;
}

struct FilterModuleState
{
    std::array<float, 2> lowpassState {};
    std::array<float, 2> highpassState {};
    std::array<float, 2> highpassInputState {};
    std::array<float, 2> bandLowpassState {};
    std::array<float, 2> bandHighpassState {};
    std::array<float, 2> bandHighpassInputState {};

    void reset() noexcept
    {
        lowpassState.fill(0.0f);
        highpassState.fill(0.0f);
        highpassInputState.fill(0.0f);
        bandLowpassState.fill(0.0f);
        bandHighpassState.fill(0.0f);
        bandHighpassInputState.fill(0.0f);
    }
};

struct EqBandState
{
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    std::array<float, 2> z1 {};
    std::array<float, 2> z2 {};
    float lastGainDb = 0.0f;
    float lastQ = 1.0f;
    float lastFrequency = -1.0f;

    void reset() noexcept
    {
        b0 = 1.0f;
        b1 = 0.0f;
        b2 = 0.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        z1.fill(0.0f);
        z2.fill(0.0f);
        lastGainDb = 0.0f;
        lastQ = 1.0f;
        lastFrequency = -1.0f;
    }
};

struct CompressorModuleState
{
    std::array<float, 2> envelopes {};

    void reset() noexcept
    {
        envelopes.fill(0.0f);
    }
};

struct DelayModuleState
{
    juce::AudioBuffer<float> buffer;
    int writePosition = 0;
    int maxDelaySamples = 1;
    double preparedSampleRate = 0.0;

    void reset() noexcept
    {
        writePosition = 0;

        if (buffer.getNumSamples() > 0)
            buffer.clear();
    }

    void prepare(double sampleRate)
    {
        if (sampleRate <= 0.0)
            return;

        const auto requiredSamples = juce::jmax(1, (int) std::ceil(sampleRate * 2.0));

        if (buffer.getNumChannels() != 2 || buffer.getNumSamples() != requiredSamples)
            buffer.setSize(2, requiredSamples, false, false, true);

        buffer.clear();
        writePosition = 0;
        maxDelaySamples = requiredSamples;
        preparedSampleRate = sampleRate;
    }
};

struct ReverbModuleState
{
    std::array<juce::Reverb, 2> reverbs;
    float lastSize = -1.0f;
    float lastDamping = -1.0f;
    double preparedSampleRate = 0.0;

    void reset() noexcept
    {
        for (auto& reverb : reverbs)
            reverb.reset();

        lastSize = -1.0f;
        lastDamping = -1.0f;
        preparedSampleRate = 0.0;
    }

    void prepare(double sampleRate) noexcept
    {
        if (sampleRate <= 0.0)
            return;

        if (std::abs(preparedSampleRate - sampleRate) < 0.01)
            return;

        for (auto& reverb : reverbs)
        {
            reverb.reset();
            reverb.setSampleRate(sampleRate);
        }

        preparedSampleRate = sampleRate;
        lastSize = -1.0f;
        lastDamping = -1.0f;
    }

    void updateParameters(float size, float damping) noexcept
    {
        if (preparedSampleRate <= 0.0)
            return;

        if (std::abs(lastSize - size) < 0.001f && std::abs(lastDamping - damping) < 0.001f)
            return;

        juce::Reverb::Parameters parameters;
        parameters.roomSize = size;
        parameters.damping = damping;
        parameters.wetLevel = 1.0f;
        parameters.dryLevel = 0.0f;
        parameters.width = 1.0f;
        parameters.freezeMode = 0.0f;

        for (auto& reverb : reverbs)
            reverb.setParameters(parameters);

        lastSize = size;
        lastDamping = damping;
    }
};

struct ModuleChain
{
    static constexpr int maxChainModules = 8;
    static constexpr int maxEqBands = 6;
    std::array<std::atomic<int>, maxChainModules> moduleTypes;
    std::array<int, maxChainModules> activeModuleTypes {};
    std::array<std::atomic<bool>, maxChainModules> moduleResetRequested;
    std::array<std::atomic<bool>, maxChainModules> moduleBypassed;
    std::array<std::atomic<float>, maxChainModules> filterMorphs;
    std::array<std::array<std::atomic<float>, maxEqBands>, maxChainModules> eqBandGainsDb;
    std::array<std::array<std::atomic<float>, maxEqBands>, maxChainModules> eqBandQs;
    std::array<std::array<std::atomic<float>, maxEqBands>, maxChainModules> eqBandFrequencies;
    std::array<std::atomic<float>, maxChainModules> compressorThresholdsDb;
    std::array<std::atomic<float>, maxChainModules> compressorRatios;
    std::array<std::atomic<float>, maxChainModules> compressorAttacksMs;
    std::array<std::atomic<float>, maxChainModules> compressorReleasesMs;
    std::array<std::atomic<float>, maxChainModules> compressorMakeupGainsDb;
    std::array<std::atomic<int>, maxChainModules> saturationModes;
    std::array<std::atomic<float>, maxChainModules> saturationAmounts;
    std::array<std::atomic<float>, maxChainModules> delayTimesMs;
    std::array<std::atomic<bool>, maxChainModules> delaySyncEnableds;
    std::array<std::atomic<int>, maxChainModules> delaySyncIndices;
    std::array<std::atomic<float>, maxChainModules> delayFeedbacks;
    std::array<std::atomic<float>, maxChainModules> delayMixes;
    std::array<std::atomic<float>, maxChainModules> reverbSizes;
    std::array<std::atomic<float>, maxChainModules> reverbDampings;
    std::array<std::atomic<float>, maxChainModules> reverbMixes;
    std::array<std::atomic<float>, maxChainModules> gainModuleGainsDb;
    std::array<std::atomic<float>, maxChainModules> gainModulePans;
    std::array<std::atomic<float>, maxChainModules> moduleInputMeters;
    std::array<std::atomic<float>, maxChainModules> moduleOutputMeters;
    std::array<FilterModuleState, maxChainModules> filterStates;
    std::array<std::array<EqBandState, maxEqBands>, maxChainModules> eqStates;
    std::array<CompressorModuleState, maxChainModules> compressorStates;
    std::array<DelayModuleState, maxChainModules> delayStates;
    std::array<ReverbModuleState, maxChainModules> reverbStates;
    std::array<float, maxChainModules> moduleBlockInputPeaks {};
    std::array<float, maxChainModules> moduleBlockOutputPeaks {};

    void resetModuleConfiguration()
    {
        for (int moduleIndex = 0; moduleIndex < maxChainModules; ++moduleIndex)
        {
            moduleTypes[(size_t) moduleIndex].store((int) ChainModuleType::none, std::memory_order_relaxed);
            activeModuleTypes[(size_t) moduleIndex] = (int) ChainModuleType::none;
            moduleResetRequested[(size_t) moduleIndex].store(false, std::memory_order_relaxed);
            moduleBypassed[(size_t) moduleIndex].store(false, std::memory_order_relaxed);
            filterMorphs[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
            compressorThresholdsDb[(size_t) moduleIndex].store(-18.0f, std::memory_order_relaxed);
            compressorRatios[(size_t) moduleIndex].store(4.0f, std::memory_order_relaxed);
            compressorAttacksMs[(size_t) moduleIndex].store(10.0f, std::memory_order_relaxed);
            compressorReleasesMs[(size_t) moduleIndex].store(120.0f, std::memory_order_relaxed);
            compressorMakeupGainsDb[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
            saturationModes[(size_t) moduleIndex].store((int) SaturationMode::light, std::memory_order_relaxed);
            saturationAmounts[(size_t) moduleIndex].store(0.35f, std::memory_order_relaxed);
            delayTimesMs[(size_t) moduleIndex].store(380.0f, std::memory_order_relaxed);
            delaySyncEnableds[(size_t) moduleIndex].store(true, std::memory_order_relaxed);
            delaySyncIndices[(size_t) moduleIndex].store(2, std::memory_order_relaxed);
            delayFeedbacks[(size_t) moduleIndex].store(0.35f, std::memory_order_relaxed);
            delayMixes[(size_t) moduleIndex].store(0.25f, std::memory_order_relaxed);
            reverbSizes[(size_t) moduleIndex].store(0.45f, std::memory_order_relaxed);
            reverbDampings[(size_t) moduleIndex].store(0.35f, std::memory_order_relaxed);
            reverbMixes[(size_t) moduleIndex].store(0.25f, std::memory_order_relaxed);
            gainModuleGainsDb[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
            gainModulePans[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
            moduleInputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
            moduleOutputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
            moduleBlockInputPeaks[(size_t) moduleIndex] = 0.0f;
            moduleBlockOutputPeaks[(size_t) moduleIndex] = 0.0f;
            filterStates[(size_t) moduleIndex].reset();
            compressorStates[(size_t) moduleIndex].reset();
            delayStates[(size_t) moduleIndex].reset();
            reverbStates[(size_t) moduleIndex].reset();

            for (int bandIndex = 0; bandIndex < maxEqBands; ++bandIndex)
            {
                eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].store(0.0f, std::memory_order_relaxed);
                eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].store(1.0f, std::memory_order_relaxed);
                eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].store(getDefaultEqFrequency(bandIndex),
                                                                                  std::memory_order_relaxed);
                eqStates[(size_t) moduleIndex][(size_t) bandIndex].reset();
            }
        }
    }

    static float getDefaultEqFrequency(int bandIndex) noexcept
    {
        static constexpr std::array<float, maxEqBands> defaults { 80.0f, 180.0f, 420.0f, 1200.0f, 4200.0f, 9600.0f };
        return defaults[(size_t) juce::jlimit(0, maxEqBands - 1, bandIndex)];
    }
};

struct SendBus : ModuleChain
{
    std::atomic<float> outputMeter { 0.0f };
    float outputBlockPeak = 0.0f;

    SendBus()
    {
        resetModuleConfiguration();
    }

    void reset()
    {
        outputMeter.store(0.0f, std::memory_order_relaxed);
        outputBlockPeak = 0.0f;
        resetModuleConfiguration();
    }
};

struct Track : ModuleChain
{
    static constexpr int numChannels = 2;
    static constexpr int chunkSize = 16384;
    static constexpr int maxChunks = 2048;
    static constexpr int numSendBuses = 3;

    std::array<std::unique_ptr<juce::AudioBuffer<float>>, maxChunks> ownedChunks;
    std::array<std::atomic<juce::AudioBuffer<float>*>, maxChunks> chunkPointers;
    std::atomic<int> recordMode { (int) TrackRecordMode::off };
    std::atomic<int> pendingRecordMode { (int) TrackRecordMode::off };
    std::atomic<int> inputSource { 0 };
    std::atomic<float> inputGain { 1.0f };
    std::atomic<float> mixerGainDb { 0.0f };
    std::atomic<float> mixerPan { 0.0f };
    std::array<std::atomic<float>, numSendBuses> sendLevels;
    std::atomic<float> mixerMeter { 0.0f };
    std::atomic<bool> muted { false };
    std::atomic<float> peakMeter { 0.0f };
    std::atomic<bool> clipping { false };
    std::atomic<int> recordedLength { 0 };
    int writePosition = 0;
    float mixerBlockPeak = 0.0f;

    Track()
    {
        for (auto& chunkPointer : chunkPointers)
            chunkPointer.store(nullptr, std::memory_order_relaxed);

        resetModuleConfiguration();
    }

    void reset()
    {
        writePosition = 0;
        recordedLength.store(0, std::memory_order_relaxed);
        peakMeter.store(0.0f, std::memory_order_relaxed);
        clipping.store(false, std::memory_order_relaxed);
        muted.store(false, std::memory_order_relaxed);
        pendingRecordMode.store((int) TrackRecordMode::off, std::memory_order_relaxed);
        inputGain.store(1.0f, std::memory_order_relaxed);
        inputSource.store(0, std::memory_order_relaxed);
        mixerGainDb.store(0.0f, std::memory_order_relaxed);
        mixerPan.store(0.0f, std::memory_order_relaxed);
        mixerMeter.store(0.0f, std::memory_order_relaxed);
        mixerBlockPeak = 0.0f;

        for (auto& sendLevel : sendLevels)
            sendLevel.store(0.0f, std::memory_order_relaxed);

        resetModuleConfiguration();

        for (auto& ownedChunk : ownedChunks)
        {
            if (ownedChunk != nullptr)
                ownedChunk->clear();
        }
    }

    bool ensureChunkAllocated(int chunkIndex)
    {
        if (chunkIndex < 0 || chunkIndex >= maxChunks)
            return false;

        if (chunkPointers[(size_t) chunkIndex].load(std::memory_order_acquire) != nullptr)
            return true;

        auto chunk = std::make_unique<juce::AudioBuffer<float>>(numChannels, chunkSize);
        chunk->clear();
        auto* rawChunk = chunk.get();
        ownedChunks[(size_t) chunkIndex] = std::move(chunk);
        chunkPointers[(size_t) chunkIndex].store(rawChunk, std::memory_order_release);
        return true;
    }

    juce::AudioBuffer<float>* getChunk(int chunkIndex) const noexcept
    {
        if (chunkIndex < 0 || chunkIndex >= maxChunks)
            return nullptr;

        return chunkPointers[(size_t) chunkIndex].load(std::memory_order_acquire);
    }
};
