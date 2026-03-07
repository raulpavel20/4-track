#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <memory>

enum class TrackRecordMode : int
{
    off = 0,
    overdub,
    replace,
    erase
};

struct Track
{
    static constexpr int numChannels = 2;
    static constexpr int chunkSize = 16384;
    static constexpr int maxChunks = 2048;
    static constexpr int maxFilterModules = 8;

    std::array<std::unique_ptr<juce::AudioBuffer<float>>, maxChunks> ownedChunks;
    std::array<std::atomic<juce::AudioBuffer<float>*>, maxChunks> chunkPointers;
    std::atomic<int> recordMode { (int) TrackRecordMode::off };
    std::atomic<int> pendingRecordMode { (int) TrackRecordMode::off };
    std::atomic<int> inputSource { 0 };
    std::atomic<float> inputGain { 1.0f };
    std::array<std::atomic<bool>, maxFilterModules> filterEnabled;
    std::array<std::atomic<float>, maxFilterModules> filterMorphs;
    std::atomic<bool> muted { false };
    std::atomic<float> peakMeter { 0.0f };
    std::atomic<bool> clipping { false };
    std::atomic<int> recordedLength { 0 };
    int writePosition = 0;
    std::array<std::array<float, numChannels>, maxFilterModules> lowpassStates {};
    std::array<std::array<float, numChannels>, maxFilterModules> highpassStates {};
    std::array<std::array<float, numChannels>, maxFilterModules> highpassInputStates {};
    std::array<std::array<float, numChannels>, maxFilterModules> bandLowpassStates {};
    std::array<std::array<float, numChannels>, maxFilterModules> bandHighpassStates {};
    std::array<std::array<float, numChannels>, maxFilterModules> bandHighpassInputStates {};

    Track()
    {
        for (auto& chunkPointer : chunkPointers)
            chunkPointer.store(nullptr, std::memory_order_relaxed);

        for (auto& enabled : filterEnabled)
            enabled.store(false, std::memory_order_relaxed);

        for (auto& morph : filterMorphs)
            morph.store(0.0f, std::memory_order_relaxed);
    }

    void reset()
    {
        writePosition = 0;
        recordedLength.store(0, std::memory_order_relaxed);
        peakMeter.store(0.0f, std::memory_order_relaxed);
        clipping.store(false, std::memory_order_relaxed);
        muted.store(false, std::memory_order_relaxed);
        pendingRecordMode.store((int) TrackRecordMode::off, std::memory_order_relaxed);

        for (auto& enabled : filterEnabled)
            enabled.store(false, std::memory_order_relaxed);

        for (auto& morph : filterMorphs)
            morph.store(0.0f, std::memory_order_relaxed);

        for (auto& states : lowpassStates)
            states.fill(0.0f);

        for (auto& states : highpassStates)
            states.fill(0.0f);

        for (auto& states : highpassInputStates)
            states.fill(0.0f);

        for (auto& states : bandLowpassStates)
            states.fill(0.0f);

        for (auto& states : bandHighpassStates)
            states.fill(0.0f);

        for (auto& states : bandHighpassInputStates)
            states.fill(0.0f);

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
