#include "../../TapeEngine.h"

#include <cmath>
#include <memory>

namespace
{
float clampDbGain(float gainDb, float minimum, float maximum) noexcept
{
    return juce::jlimit(minimum, maximum, gainDb);
}

float clampPan(float value) noexcept
{
    return juce::jlimit(-1.0f, 1.0f, value);
}

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

void TapeEngine::setTrackSendLevel(int trackIndex, int sendIndex, float amount)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return;

    tracks[(size_t) trackIndex].sendLevels[(size_t) sendIndex].store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_release);
}

float TapeEngine::getTrackSendLevel(int trackIndex, int sendIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return 0.0f;

    return tracks[(size_t) trackIndex].sendLevels[(size_t) sendIndex].load(std::memory_order_acquire);
}

float TapeEngine::getTrackMixerPan(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].mixerPan.load(std::memory_order_acquire);
}

float TapeEngine::getTrackMixerMeter(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    return tracks[(size_t) trackIndex].mixerMeter.load(std::memory_order_acquire);
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

juce::Result TapeEngine::exportMixToFile(const juce::File& file, const ExportSettings& settings) const
{
    if (file == juce::File())
        return juce::Result::fail("No export file selected.");

    const auto sourceSampleRate = juce::jmax(1.0, sampleRate);
    const auto targetSampleRate = juce::jmax(1.0, settings.sampleRate);
    const auto lastAudibleSample = getLastAudibleSample();

    if (lastAudibleSample <= 0)
        return juce::Result::fail("There is no recorded audio to export.");

    const auto tailSeconds = juce::jlimit(0.0, 30.0, settings.tailSeconds);
    const auto outputDurationSeconds = ((double) lastAudibleSample / sourceSampleRate) + tailSeconds;
    const auto totalOutputSamples = juce::jmax<int64_t>(1, (int64_t) std::ceil(outputDurationSeconds * targetSampleRate));
    const auto soloedTrackSnapshot = soloTrack.load(std::memory_order_acquire);
    std::array<bool, numTracks> mutedSnapshots {};
    std::array<float, numTracks> gainSnapshots {};
    std::array<float, numTracks> panSnapshots {};
    std::array<std::array<float, numSendBuses>, numTracks> sendSnapshots {};
    std::array<SendBus, numSendBuses> localSendBuses;

    for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
    {
        const auto& track = tracks[(size_t) trackIndex];
        mutedSnapshots[(size_t) trackIndex] = track.muted.load(std::memory_order_acquire);
        gainSnapshots[(size_t) trackIndex] = track.mixerGainDb.load(std::memory_order_acquire);
        panSnapshots[(size_t) trackIndex] = track.mixerPan.load(std::memory_order_acquire);

        for (int sendIndex = 0; sendIndex < numSendBuses; ++sendIndex)
            sendSnapshots[(size_t) trackIndex][(size_t) sendIndex] = track.sendLevels[(size_t) sendIndex].load(std::memory_order_acquire);
    }

    for (int sendIndex = 0; sendIndex < numSendBuses; ++sendIndex)
    {
        const auto& sourceSendBus = sendBuses[(size_t) sendIndex];
        auto& localSendBus = localSendBuses[(size_t) sendIndex];

        for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
        {
            const auto sourceType = (ChainModuleType) sourceSendBus.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire);
            const auto type = sourceType == ChainModuleType::spectrumAnalyzer ? ChainModuleType::none : sourceType;
            localSendBus.moduleTypes[(size_t) moduleIndex].store((int) type, std::memory_order_relaxed);
            localSendBus.activeModuleTypes[(size_t) moduleIndex] = (int) type;
            localSendBus.moduleBypassed[(size_t) moduleIndex].store(sourceSendBus.moduleBypassed[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                    std::memory_order_relaxed);
            localSendBus.filterMorphs[(size_t) moduleIndex].store(sourceSendBus.filterMorphs[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                  std::memory_order_relaxed);
            localSendBus.compressorThresholdsDb[(size_t) moduleIndex].store(sourceSendBus.compressorThresholdsDb[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                            std::memory_order_relaxed);
            localSendBus.compressorRatios[(size_t) moduleIndex].store(sourceSendBus.compressorRatios[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                      std::memory_order_relaxed);
            localSendBus.compressorAttacksMs[(size_t) moduleIndex].store(sourceSendBus.compressorAttacksMs[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                         std::memory_order_relaxed);
            localSendBus.compressorReleasesMs[(size_t) moduleIndex].store(sourceSendBus.compressorReleasesMs[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                          std::memory_order_relaxed);
            localSendBus.compressorMakeupGainsDb[(size_t) moduleIndex].store(sourceSendBus.compressorMakeupGainsDb[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                             std::memory_order_relaxed);
            localSendBus.saturationModes[(size_t) moduleIndex].store(sourceSendBus.saturationModes[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                     std::memory_order_relaxed);
            localSendBus.saturationAmounts[(size_t) moduleIndex].store(sourceSendBus.saturationAmounts[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                       std::memory_order_relaxed);
            localSendBus.delayTimesMs[(size_t) moduleIndex].store(sourceSendBus.delayTimesMs[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                  std::memory_order_relaxed);
            localSendBus.delaySyncEnableds[(size_t) moduleIndex].store(sourceSendBus.delaySyncEnableds[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                       std::memory_order_relaxed);
            localSendBus.delaySyncIndices[(size_t) moduleIndex].store(sourceSendBus.delaySyncIndices[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                      std::memory_order_relaxed);
            localSendBus.delayFeedbacks[(size_t) moduleIndex].store(sourceSendBus.delayFeedbacks[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                    std::memory_order_relaxed);
            localSendBus.delayMixes[(size_t) moduleIndex].store(sourceSendBus.delayMixes[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                std::memory_order_relaxed);
            localSendBus.reverbSizes[(size_t) moduleIndex].store(sourceSendBus.reverbSizes[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                 std::memory_order_relaxed);
            localSendBus.reverbDampings[(size_t) moduleIndex].store(sourceSendBus.reverbDampings[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                    std::memory_order_relaxed);
            localSendBus.reverbMixes[(size_t) moduleIndex].store(sourceSendBus.reverbMixes[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                 std::memory_order_relaxed);
            localSendBus.phaserRates[(size_t) moduleIndex].store(sourceSendBus.phaserRates[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                 std::memory_order_relaxed);
            localSendBus.phaserSyncEnableds[(size_t) moduleIndex].store(sourceSendBus.phaserSyncEnableds[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                        std::memory_order_relaxed);
            localSendBus.phaserSyncIndices[(size_t) moduleIndex].store(sourceSendBus.phaserSyncIndices[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                       std::memory_order_relaxed);
            localSendBus.phaserDepths[(size_t) moduleIndex].store(sourceSendBus.phaserDepths[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                  std::memory_order_relaxed);
            localSendBus.phaserCentreFrequencies[(size_t) moduleIndex].store(sourceSendBus.phaserCentreFrequencies[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                             std::memory_order_relaxed);
            localSendBus.phaserFeedbacks[(size_t) moduleIndex].store(sourceSendBus.phaserFeedbacks[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                     std::memory_order_relaxed);
            localSendBus.phaserMixes[(size_t) moduleIndex].store(sourceSendBus.phaserMixes[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                 std::memory_order_relaxed);
            localSendBus.gainModuleGainsDb[(size_t) moduleIndex].store(sourceSendBus.gainModuleGainsDb[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                       std::memory_order_relaxed);
            localSendBus.gainModulePans[(size_t) moduleIndex].store(sourceSendBus.gainModulePans[(size_t) moduleIndex].load(std::memory_order_acquire),
                                                                    std::memory_order_relaxed);

            for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
            {
                localSendBus.eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].store(sourceSendBus.eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire),
                                                                                           std::memory_order_relaxed);
                localSendBus.eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].store(sourceSendBus.eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire),
                                                                                      std::memory_order_relaxed);
                localSendBus.eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].store(sourceSendBus.eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire),
                                                                                               std::memory_order_relaxed);
            }

            if (type == ChainModuleType::delay)
                localSendBus.delayStates[(size_t) moduleIndex].prepare(targetSampleRate);

            if (type == ChainModuleType::reverb)
                localSendBus.reverbStates[(size_t) moduleIndex].prepare(targetSampleRate);

            if (type == ChainModuleType::phaser)
                localSendBus.phaserStates[(size_t) moduleIndex].prepare(targetSampleRate);
        }
    }

    if (file.existsAsFile() && ! file.deleteFile())
        return juce::Result::fail("Could not replace the existing export file.");

    auto outputStream = file.createOutputStream();

    if (outputStream == nullptr || ! outputStream->openedOk())
        return juce::Result::fail("Could not create the export file.");

    std::unique_ptr<juce::AudioFormat> format;

    if (settings.format == ExportFormat::aiff)
        format = std::make_unique<juce::AiffAudioFormat>();
    else
        format = std::make_unique<juce::WavAudioFormat>();

    auto writer = std::unique_ptr<juce::AudioFormatWriter>(format->createWriterFor(outputStream.get(),
                                                                                   targetSampleRate,
                                                                                   Track::numChannels,
                                                                                   settings.bitDepth,
                                                                                   {},
                                                                                   0));

    if (writer == nullptr)
        return juce::Result::fail("Could not initialise the export writer.");

    outputStream.release();

    constexpr int blockSize = 512;
    juce::AudioBuffer<float> renderBuffer(Track::numChannels, blockSize);
    const auto sourceIncrement = sourceSampleRate / targetSampleRate;
    auto sourcePosition = 0.0;

    for (int64_t outputStart = 0; outputStart < totalOutputSamples; outputStart += blockSize)
    {
        const auto samplesThisBlock = (int) juce::jmin<int64_t>(blockSize, totalOutputSamples - outputStart);
        renderBuffer.clear();

        for (int sampleIndex = 0; sampleIndex < samplesThisBlock; ++sampleIndex)
        {
            auto mixedLeft = 0.0f;
            auto mixedRight = 0.0f;
            std::array<std::array<float, Track::numChannels>, numTracks> trackPostMixer {};

            for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
            {
                if ((soloedTrackSnapshot >= 0 && soloedTrackSnapshot != trackIndex) || mutedSnapshots[(size_t) trackIndex])
                    continue;

                const auto& track = tracks[(size_t) trackIndex];
                auto trackLeft = readTrackSampleInterpolated(track, 0, sourcePosition);
                auto trackRight = readTrackSampleInterpolated(track, 1, sourcePosition);
                const auto gain = juce::Decibels::decibelsToGain(gainSnapshots[(size_t) trackIndex]);
                trackLeft *= gain;
                trackRight *= gain;
                applyTrackMixer(panSnapshots[(size_t) trackIndex], trackLeft, trackRight);
                trackPostMixer[(size_t) trackIndex][0] = trackLeft;
                trackPostMixer[(size_t) trackIndex][1] = trackRight;
                mixedLeft += trackLeft;
                mixedRight += trackRight;
            }

            for (int sendIndex = 0; sendIndex < numSendBuses; ++sendIndex)
            {
                auto sendLeft = 0.0f;
                auto sendRight = 0.0f;

                for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
                {
                    const auto sendLevel = sendSnapshots[(size_t) trackIndex][(size_t) sendIndex];

                    if (sendLevel <= 0.0f)
                        continue;

                    sendLeft += trackPostMixer[(size_t) trackIndex][0] * sendLevel;
                    sendRight += trackPostMixer[(size_t) trackIndex][1] * sendLevel;
                }

                auto& localSendBus = localSendBuses[(size_t) sendIndex];
                auto sendReturnLeft = sendLeft;
                auto sendReturnRight = sendRight;

                for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
                {
                    if ((ChainModuleType) localSendBus.activeModuleTypes[(size_t) moduleIndex] == ChainModuleType::none)
                        continue;

                    sendReturnLeft = processChainModule(localSendBus, moduleIndex, 0, sendReturnLeft);
                    sendReturnRight = processChainModule(localSendBus, moduleIndex, 1, sendReturnRight);
                }

                mixedLeft += sendReturnLeft;
                mixedRight += sendReturnRight;
            }

            renderBuffer.setSample(0, sampleIndex, mixedLeft);
            renderBuffer.setSample(1, sampleIndex, mixedRight);
            sourcePosition += sourceIncrement;
        }

        if (! writer->writeFromAudioSampleBuffer(renderBuffer, 0, samplesThisBlock))
            return juce::Result::fail("Failed while writing exported audio.");
    }

    return juce::Result::ok();
}

void TapeEngine::applyTrackMixer(float pan, float& left, float& right) const noexcept
{
    const auto clampedPan = clampPan(pan);

    if (clampedPan < 0.0f)
        right *= 1.0f + clampedPan;
    else if (clampedPan > 0.0f)
        left *= 1.0f - clampedPan;
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

float TapeEngine::readTrackSampleInterpolated(const Track& track, int channel, double samplePosition) const noexcept
{
    if (samplePosition <= 0.0)
        return readTrackSample(track, channel, 0);

    const auto floorSample = (int) std::floor(samplePosition);
    const auto fraction = (float) (samplePosition - (double) floorSample);
    const auto firstSample = readTrackSample(track, channel, floorSample);
    const auto secondSample = readTrackSample(track, channel, floorSample + 1);
    return firstSample + ((secondSample - firstSample) * fraction);
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

int TapeEngine::getLastAudibleSample() const noexcept
{
    constexpr float silenceThreshold = 1.0e-5f;
    auto lastAudibleSample = 0;

    for (const auto& track : tracks)
    {
        const auto recordedLength = track.recordedLength.load(std::memory_order_acquire);

        for (int sampleIndex = recordedLength - 1; sampleIndex >= 0; --sampleIndex)
        {
            if (std::abs(readTrackSample(track, 0, sampleIndex)) > silenceThreshold
                || std::abs(readTrackSample(track, 1, sampleIndex)) > silenceThreshold)
            {
                lastAudibleSample = juce::jmax(lastAudibleSample, sampleIndex + 1);
                break;
            }
        }
    }

    return lastAudibleSample;
}
