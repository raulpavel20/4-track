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

    std::array<std::unique_ptr<juce::AudioBuffer<float>>, maxChunks> ownedChunks;
    std::array<std::atomic<juce::AudioBuffer<float>*>, maxChunks> chunkPointers;
    std::atomic<int> recordMode { (int) TrackRecordMode::off };
    std::atomic<int> pendingRecordMode { (int) TrackRecordMode::off };
    std::atomic<int> inputSource { 0 };
    std::atomic<float> inputGain { 1.0f };
    std::atomic<float> filterMorph { 0.0f };
    std::atomic<bool> muted { false };
    std::atomic<float> peakMeter { 0.0f };
    std::atomic<bool> clipping { false };
    std::atomic<int> recordedLength { 0 };
    int writePosition = 0;
    float lowpassState[numChannels] {};
    float highpassState[numChannels] {};
    float highpassInputState[numChannels] {};
    float bandLowpassState[numChannels] {};
    float bandHighpassState[numChannels] {};
    float bandHighpassInputState[numChannels] {};

    Track()
    {
        for (auto& chunkPointer : chunkPointers)
            chunkPointer.store(nullptr, std::memory_order_relaxed);
    }

    void reset()
    {
        writePosition = 0;
        recordedLength.store(0, std::memory_order_relaxed);
        peakMeter.store(0.0f, std::memory_order_relaxed);
        clipping.store(false, std::memory_order_relaxed);
        muted.store(false, std::memory_order_relaxed);
        pendingRecordMode.store((int) TrackRecordMode::off, std::memory_order_relaxed);
        filterMorph.store(0.0f, std::memory_order_relaxed);
        lowpassState[0] = 0.0f;
        lowpassState[1] = 0.0f;
        highpassState[0] = 0.0f;
        highpassState[1] = 0.0f;
        highpassInputState[0] = 0.0f;
        highpassInputState[1] = 0.0f;
        bandLowpassState[0] = 0.0f;
        bandLowpassState[1] = 0.0f;
        bandHighpassState[0] = 0.0f;
        bandHighpassState[1] = 0.0f;
        bandHighpassInputState[0] = 0.0f;
        bandHighpassInputState[1] = 0.0f;

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
