#include "../../TapeEngine.h"

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
