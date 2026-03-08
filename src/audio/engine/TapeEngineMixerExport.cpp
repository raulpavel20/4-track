#include "../../TapeEngine.h"

#include <cmath>
#include <memory>

namespace
{
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

struct DelaySyncOption
{
    const char* label;
    double beats;
};

constexpr std::array<DelaySyncOption, 8> delaySyncOptions
{{
    { "1/1", 4.0 },
    { "1/2", 2.0 },
    { "1/4", 1.0 },
    { "1/8", 0.5 },
    { "1/8.", 0.75 },
    { "1/8T", 1.0 / 3.0 },
    { "1/16", 0.25 },
    { "1/16T", 1.0 / 6.0 }
}};

struct OfflineDelayState
{
    juce::AudioBuffer<float> buffer;
    int writePosition = 0;
    int maxDelaySamples = 1;

    void prepare(double sampleRate)
    {
        maxDelaySamples = juce::jmax(1, (int) std::ceil(sampleRate * 2.0));
        buffer.setSize(Track::numChannels, maxDelaySamples, false, false, true);
        buffer.clear();
        writePosition = 0;
    }
};

void processDelayReturnOffline(OfflineDelayState& state,
                               double sampleRate,
                               float delayTimeMs,
                               float feedback,
                               float wet,
                               float inputLeft,
                               float inputRight,
                               float& outputLeft,
                               float& outputRight) noexcept
{
    outputLeft = 0.0f;
    outputRight = 0.0f;

    if (state.buffer.getNumSamples() <= 0)
        return;

    const auto delaySamples = juce::jlimit(1,
                                           juce::jmax(1, state.maxDelaySamples - 1),
                                           (int) std::round((sampleRate * clampDelayTimeMs(delayTimeMs)) * 0.001));
    const auto readPosition = (state.writePosition - delaySamples + state.maxDelaySamples) % state.maxDelaySamples;
    const auto delayedLeft = state.buffer.getSample(0, readPosition);
    const auto delayedRight = state.buffer.getSample(1, readPosition);
    state.buffer.setSample(0, state.writePosition, inputLeft + (delayedLeft * feedback));
    state.buffer.setSample(1, state.writePosition, inputRight + (delayedRight * feedback));
    state.writePosition = (state.writePosition + 1) % state.maxDelaySamples;
    outputLeft = delayedLeft * wet;
    outputRight = delayedRight * wet;
}

void processReverbReturnOffline(juce::Reverb& reverb,
                                float wet,
                                float inputLeft,
                                float inputRight,
                                float& outputLeft,
                                float& outputRight) noexcept
{
    outputLeft = inputLeft;
    outputRight = inputRight;
    reverb.processStereo(&outputLeft, &outputRight, 1);
    outputLeft *= wet;
    outputRight *= wet;
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

void TapeEngine::setDelaySyncEnabled(bool shouldBeEnabled)
{
    delaySyncEnabled.store(shouldBeEnabled, std::memory_order_release);
}

bool TapeEngine::isDelaySyncEnabled() const noexcept
{
    return delaySyncEnabled.load(std::memory_order_acquire);
}

void TapeEngine::setDelaySyncIndex(int index)
{
    delaySyncIndex.store(juce::jlimit(0, getNumDelaySyncOptions() - 1, index), std::memory_order_release);
}

int TapeEngine::getDelaySyncIndex() const noexcept
{
    return juce::jlimit(0, getNumDelaySyncOptions() - 1, delaySyncIndex.load(std::memory_order_acquire));
}

int TapeEngine::getNumDelaySyncOptions() noexcept
{
    return (int) delaySyncOptions.size();
}

juce::String TapeEngine::getDelaySyncLabel(int index)
{
    return delaySyncOptions[(size_t) juce::jlimit(0, getNumDelaySyncOptions() - 1, index)].label;
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
    const auto delayTimeSnapshot = isDelaySyncEnabled() ? getResolvedDelayTimeMs()
                                                        : delayTimeMs.load(std::memory_order_acquire);
    const auto delayFeedbackSnapshot = delayFeedback.load(std::memory_order_acquire);
    const auto delayMixSnapshot = delayMix.load(std::memory_order_acquire);
    const auto reverbMixSnapshot = reverbMix.load(std::memory_order_acquire);
    std::array<bool, numTracks> mutedSnapshots {};
    std::array<float, numTracks> gainSnapshots {};
    std::array<float, numTracks> panSnapshots {};
    std::array<float, numTracks> delaySendSnapshots {};
    std::array<float, numTracks> reverbSendSnapshots {};

    for (int trackIndex = 0; trackIndex < numTracks; ++trackIndex)
    {
        const auto& track = tracks[(size_t) trackIndex];
        mutedSnapshots[(size_t) trackIndex] = track.muted.load(std::memory_order_acquire);
        gainSnapshots[(size_t) trackIndex] = track.mixerGainDb.load(std::memory_order_acquire);
        panSnapshots[(size_t) trackIndex] = track.mixerPan.load(std::memory_order_acquire);
        delaySendSnapshots[(size_t) trackIndex] = track.delaySend.load(std::memory_order_acquire);
        reverbSendSnapshots[(size_t) trackIndex] = track.reverbSend.load(std::memory_order_acquire);
    }

    juce::Reverb localReverb;
    juce::Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = reverbSize.load(std::memory_order_acquire);
    reverbParameters.damping = reverbDamping.load(std::memory_order_acquire);
    reverbParameters.wetLevel = 1.0f;
    reverbParameters.dryLevel = 0.0f;
    reverbParameters.width = 1.0f;
    reverbParameters.freezeMode = 0.0f;
    localReverb.setSampleRate(targetSampleRate);
    localReverb.setParameters(reverbParameters);

    OfflineDelayState localDelay;
    localDelay.prepare(targetSampleRate);

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
            auto delayBusLeft = 0.0f;
            auto delayBusRight = 0.0f;
            auto reverbBusLeft = 0.0f;
            auto reverbBusRight = 0.0f;

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
                mixedLeft += trackLeft;
                mixedRight += trackRight;
                delayBusLeft += trackLeft * delaySendSnapshots[(size_t) trackIndex];
                delayBusRight += trackRight * delaySendSnapshots[(size_t) trackIndex];
                reverbBusLeft += trackLeft * reverbSendSnapshots[(size_t) trackIndex];
                reverbBusRight += trackRight * reverbSendSnapshots[(size_t) trackIndex];
            }

            auto delayReturnLeft = 0.0f;
            auto delayReturnRight = 0.0f;
            processDelayReturnOffline(localDelay,
                                      targetSampleRate,
                                      delayTimeSnapshot,
                                      delayFeedbackSnapshot,
                                      delayMixSnapshot,
                                      delayBusLeft,
                                      delayBusRight,
                                      delayReturnLeft,
                                      delayReturnRight);
            mixedLeft += delayReturnLeft;
            mixedRight += delayReturnRight;

            auto reverbReturnLeft = 0.0f;
            auto reverbReturnRight = 0.0f;
            processReverbReturnOffline(localReverb,
                                       reverbMixSnapshot,
                                       reverbBusLeft,
                                       reverbBusRight,
                                       reverbReturnLeft,
                                       reverbReturnRight);
            mixedLeft += reverbReturnLeft;
            mixedRight += reverbReturnRight;

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

void TapeEngine::processDelayReturn(float inputLeft, float inputRight, float& outputLeft, float& outputRight) noexcept
{
    outputLeft = 0.0f;
    outputRight = 0.0f;

    if (delayBuffer.getNumSamples() <= 0)
        return;

    const auto delaySamples = juce::jlimit(1,
                                           juce::jmax(1, maxDelaySamples - 1),
                                           (int) std::round((sampleRate * getResolvedDelayTimeMs()) * 0.001));
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

float TapeEngine::getResolvedDelayTimeMs() const noexcept
{
    if (! isDelaySyncEnabled())
        return clampDelayTimeMs(delayTimeMs.load(std::memory_order_relaxed));

    const auto beats = delaySyncOptions[(size_t) getDelaySyncIndex()].beats;
    const auto currentBpm = juce::jmax(30.0, (double) bpm.load(std::memory_order_relaxed));
    return clampDelayTimeMs((float) ((60000.0 / currentBpm) * beats));
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
