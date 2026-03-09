#include "../../TapeEngine.h"

#include <algorithm>
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

float clampDelayTimeMs(float value) noexcept
{
    return juce::jlimit(20.0f, 2000.0f, value);
}

float clampPhaserRate(float value) noexcept
{
    return juce::jlimit(0.05f, 10.0f, value);
}

float clampPhaserDepth(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, value);
}

float clampPhaserCentreFrequency(float value) noexcept
{
    return juce::jlimit(200.0f, 2000.0f, value);
}

float clampPhaserFeedback(float value) noexcept
{
    return juce::jlimit(0.0f, 0.95f, value);
}

float clampPhaserMix(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, value);
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

std::array<float, ModuleChain::spectrumAnalyzerBinCount> loadSpectrumAnalyzerData(const SpectrumAnalyzerState& state) noexcept
{
    std::array<float, ModuleChain::spectrumAnalyzerBinCount> data {};

    for (int binIndex = 0; binIndex < ModuleChain::spectrumAnalyzerBinCount; ++binIndex)
        data[(size_t) binIndex] = state.publishedBins[(size_t) binIndex].load(std::memory_order_acquire);

    return data;
}

float normalizeSpectrumMagnitude(float magnitude) noexcept
{
    constexpr auto floorDb = -72.0f;
    const auto normalizedMagnitude = juce::jmax(1.0e-5f, magnitude / (float) SpectrumAnalyzerState::fftSize);
    const auto magnitudeDb = juce::Decibels::gainToDecibels(normalizedMagnitude, floorDb);
    return juce::jlimit(0.0f, 1.0f, juce::jmap(magnitudeDb, floorDb, 0.0f, 0.0f, 1.0f));
}

void publishSpectrumAnalyzerFrame(SpectrumAnalyzerState& state, double sampleRate) noexcept
{
    if (sampleRate <= 0.0)
        return;

    std::copy(state.fifo.begin(), state.fifo.end(), state.fftData.begin());
    std::fill(state.fftData.begin() + SpectrumAnalyzerState::fftSize, state.fftData.end(), 0.0f);
    state.window.multiplyWithWindowingTable(state.fftData.data(), SpectrumAnalyzerState::fftSize);
    state.fft.performFrequencyOnlyForwardTransform(state.fftData.data());

    const auto nyquist = juce::jmax(60.0, juce::jmin(20000.0, sampleRate * 0.5));
    const auto binLimit = SpectrumAnalyzerState::fftSize / 2;

    for (int displayBin = 0; displayBin < ModuleChain::spectrumAnalyzerBinCount; ++displayBin)
    {
        const auto startRatio = (double) displayBin / (double) ModuleChain::spectrumAnalyzerBinCount;
        const auto endRatio = (double) (displayBin + 1) / (double) ModuleChain::spectrumAnalyzerBinCount;
        const auto startFrequency = 30.0 * std::pow(nyquist / 30.0, startRatio);
        const auto endFrequency = 30.0 * std::pow(nyquist / 30.0, endRatio);
        const auto startIndex = juce::jlimit(1,
                                             binLimit - 1,
                                             (int) std::floor(startFrequency * (double) SpectrumAnalyzerState::fftSize / sampleRate));
        const auto endIndex = juce::jlimit(startIndex,
                                           binLimit - 1,
                                           (int) std::ceil(endFrequency * (double) SpectrumAnalyzerState::fftSize / sampleRate));
        auto peakMagnitude = 0.0f;

        for (int fftIndex = startIndex; fftIndex <= endIndex; ++fftIndex)
            peakMagnitude = juce::jmax(peakMagnitude, state.fftData[(size_t) fftIndex]);

        const auto normalized = normalizeSpectrumMagnitude(peakMagnitude);
        const auto previous = state.publishedBins[(size_t) displayBin].load(std::memory_order_relaxed);
        state.publishedBins[(size_t) displayBin].store(juce::jmax(normalized, previous * 0.86f), std::memory_order_release);
    }
}

void pushSpectrumAnalyzerSample(SpectrumAnalyzerState& state, float sample, double sampleRate) noexcept
{
    state.fifo[(size_t) state.fifoIndex] = sample;
    ++state.fifoIndex;

    if (state.fifoIndex < SpectrumAnalyzerState::fftSize)
        return;

    publishSpectrumAnalyzerFrame(state, sampleRate);
    state.fifoIndex = 0;
}
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

void TapeEngine::setTrackMonitoringMode(int trackIndex, TrackMonitoringMode mode)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    tracks[(size_t) trackIndex].monitoringMode.store((int) mode, std::memory_order_release);
}

TrackMonitoringMode TapeEngine::getTrackMonitoringMode(int trackIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return TrackMonitoringMode::autoMode;

    const auto storedMode = tracks[(size_t) trackIndex].monitoringMode.load(std::memory_order_acquire);

    if (storedMode == (int) TrackMonitoringMode::in)
        return TrackMonitoringMode::in;

    if (storedMode == (int) TrackMonitoringMode::off)
        return TrackMonitoringMode::off;

    return TrackMonitoringMode::autoMode;
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

    if (type == ChainModuleType::delay)
        track.delayStates[(size_t) targetIndex].prepare(sampleRate);

    track.moduleTypes[(size_t) targetIndex].store((int) type, std::memory_order_release);
    track.moduleResetRequested[(size_t) targetIndex].store(true, std::memory_order_release);
    return targetIndex;
}

int TapeEngine::addSendBusModule(int sendIndex, ChainModuleType type)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return -1;

    if (type == ChainModuleType::none)
        return -1;

    auto& sendBus = sendBuses[(size_t) sendIndex];
    auto targetIndex = -1;
    auto lastActiveIndex = -1;

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        if ((ChainModuleType) sendBus.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire) != ChainModuleType::none)
            lastActiveIndex = moduleIndex;
    }

    for (int moduleIndex = lastActiveIndex + 1; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        if ((ChainModuleType) sendBus.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire) == ChainModuleType::none)
        {
            targetIndex = moduleIndex;
            break;
        }
    }

    if (targetIndex < 0)
    {
        for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
        {
            if ((ChainModuleType) sendBus.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire) == ChainModuleType::none)
            {
                targetIndex = moduleIndex;
                break;
            }
        }
    }

    if (targetIndex < 0)
        return -1;

    resetModuleParameters(sendBus, targetIndex, type);

    if (type == ChainModuleType::delay)
        sendBus.delayStates[(size_t) targetIndex].prepare(sampleRate);

    sendBus.moduleTypes[(size_t) targetIndex].store((int) type, std::memory_order_release);
    sendBus.moduleResetRequested[(size_t) targetIndex].store(true, std::memory_order_release);
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

void TapeEngine::removeSendBusModule(int sendIndex, int moduleIndex)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    auto& sendBus = sendBuses[(size_t) sendIndex];
    resetModuleParameters(sendBus, moduleIndex, ChainModuleType::none);
    sendBus.moduleTypes[(size_t) moduleIndex].store((int) ChainModuleType::none, std::memory_order_release);
    sendBus.moduleResetRequested[(size_t) moduleIndex].store(true, std::memory_order_release);
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

int TapeEngine::getSendBusModuleCount(int sendIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return 0;

    auto count = 0;
    const auto& sendBus = sendBuses[(size_t) sendIndex];

    for (const auto& moduleType : sendBus.moduleTypes)
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

ChainModuleType TapeEngine::getSendBusModuleType(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return ChainModuleType::none;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return ChainModuleType::none;

    return (ChainModuleType) sendBuses[(size_t) sendIndex].moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

bool TapeEngine::isTrackModulePresent(int trackIndex, int moduleIndex) const noexcept
{
    return getTrackModuleType(trackIndex, moduleIndex) != ChainModuleType::none;
}

bool TapeEngine::isSendBusModulePresent(int sendIndex, int moduleIndex) const noexcept
{
    return getSendBusModuleType(sendIndex, moduleIndex) != ChainModuleType::none;
}

void TapeEngine::setTrackModuleBypassed(int trackIndex, int moduleIndex, bool shouldBeBypassed)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].moduleBypassed[(size_t) moduleIndex].store(shouldBeBypassed, std::memory_order_release);
}

void TapeEngine::setSendBusModuleBypassed(int sendIndex, int moduleIndex, bool shouldBeBypassed)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].moduleBypassed[(size_t) moduleIndex].store(shouldBeBypassed, std::memory_order_release);
}

bool TapeEngine::isTrackModuleBypassed(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return false;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return false;

    return tracks[(size_t) trackIndex].moduleBypassed[(size_t) moduleIndex].load(std::memory_order_acquire);
}

bool TapeEngine::isSendBusModuleBypassed(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return false;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return false;

    return sendBuses[(size_t) sendIndex].moduleBypassed[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackFilterMorph(int trackIndex, int moduleIndex, float morph)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].filterMorphs[(size_t) moduleIndex].store(juce::jlimit(-1.0f, 1.0f, morph), std::memory_order_release);
}

void TapeEngine::setSendBusFilterMorph(int sendIndex, int moduleIndex, float morph)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].filterMorphs[(size_t) moduleIndex].store(juce::jlimit(-1.0f, 1.0f, morph), std::memory_order_release);
}

float TapeEngine::getTrackFilterMorph(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks))
        return 0.0f;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].filterMorphs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusFilterMorph(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return 0.0f;

    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return sendBuses[(size_t) sendIndex].filterMorphs[(size_t) moduleIndex].load(std::memory_order_acquire);
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

void TapeEngine::setSendBusEqBandGainDb(int sendIndex, int moduleIndex, int bandIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return;

    sendBuses[(size_t) sendIndex].eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].store(clampDbGain(gainDb, -18.0f, 18.0f),
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

float TapeEngine::getSendBusEqBandGainDb(int sendIndex, int moduleIndex, int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return 0.0f;

    return sendBuses[(size_t) sendIndex].eqBandGainsDb[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackEqBandQ(int trackIndex, int moduleIndex, int bandIndex, float q)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return;

    tracks[(size_t) trackIndex].eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].store(clampEqQ(q), std::memory_order_release);
}

void TapeEngine::setSendBusEqBandQ(int sendIndex, int moduleIndex, int bandIndex, float q)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return;

    sendBuses[(size_t) sendIndex].eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].store(clampEqQ(q), std::memory_order_release);
}

float TapeEngine::getTrackEqBandQ(int trackIndex, int moduleIndex, int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return 1.0f;

    return tracks[(size_t) trackIndex].eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusEqBandQ(int sendIndex, int moduleIndex, int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return 1.0f;

    return sendBuses[(size_t) sendIndex].eqBandQs[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire);
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

void TapeEngine::setSendBusEqBandFrequency(int sendIndex, int moduleIndex, int bandIndex, float frequencyHz)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return;

    sendBuses[(size_t) sendIndex].eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].store(clampEqFrequency(frequencyHz),
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

float TapeEngine::getSendBusEqBandFrequency(int sendIndex, int moduleIndex, int bandIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses)
        || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules)
        || ! juce::isPositiveAndBelow(bandIndex, Track::maxEqBands))
        return 1000.0f;

    return sendBuses[(size_t) sendIndex].eqBandFrequencies[(size_t) moduleIndex][(size_t) bandIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorThresholdDb(int trackIndex, int moduleIndex, float thresholdDb)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorThresholdsDb[(size_t) moduleIndex].store(clampDbGain(thresholdDb, -60.0f, 0.0f),
                                                                                   std::memory_order_release);
}

void TapeEngine::setSendBusCompressorThresholdDb(int sendIndex, int moduleIndex, float thresholdDb)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].compressorThresholdsDb[(size_t) moduleIndex].store(clampDbGain(thresholdDb, -60.0f, 0.0f),
                                                                                      std::memory_order_release);
}

float TapeEngine::getTrackCompressorThresholdDb(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return -18.0f;

    return tracks[(size_t) trackIndex].compressorThresholdsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusCompressorThresholdDb(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return -18.0f;

    return sendBuses[(size_t) sendIndex].compressorThresholdsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorRatio(int trackIndex, int moduleIndex, float ratio)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorRatios[(size_t) moduleIndex].store(juce::jlimit(1.0f, 20.0f, ratio),
                                                                             std::memory_order_release);
}

void TapeEngine::setSendBusCompressorRatio(int sendIndex, int moduleIndex, float ratio)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].compressorRatios[(size_t) moduleIndex].store(juce::jlimit(1.0f, 20.0f, ratio),
                                                                                std::memory_order_release);
}

float TapeEngine::getTrackCompressorRatio(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 4.0f;

    return tracks[(size_t) trackIndex].compressorRatios[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusCompressorRatio(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 4.0f;

    return sendBuses[(size_t) sendIndex].compressorRatios[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorAttackMs(int trackIndex, int moduleIndex, float attackMs)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorAttacksMs[(size_t) moduleIndex].store(juce::jlimit(0.1f, 100.0f, attackMs),
                                                                                std::memory_order_release);
}

void TapeEngine::setSendBusCompressorAttackMs(int sendIndex, int moduleIndex, float attackMs)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].compressorAttacksMs[(size_t) moduleIndex].store(juce::jlimit(0.1f, 100.0f, attackMs),
                                                                                   std::memory_order_release);
}

float TapeEngine::getTrackCompressorAttackMs(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 10.0f;

    return tracks[(size_t) trackIndex].compressorAttacksMs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusCompressorAttackMs(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 10.0f;

    return sendBuses[(size_t) sendIndex].compressorAttacksMs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorReleaseMs(int trackIndex, int moduleIndex, float releaseMs)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorReleasesMs[(size_t) moduleIndex].store(juce::jlimit(10.0f, 1000.0f, releaseMs),
                                                                                 std::memory_order_release);
}

void TapeEngine::setSendBusCompressorReleaseMs(int sendIndex, int moduleIndex, float releaseMs)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].compressorReleasesMs[(size_t) moduleIndex].store(juce::jlimit(10.0f, 1000.0f, releaseMs),
                                                                                    std::memory_order_release);
}

float TapeEngine::getTrackCompressorReleaseMs(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 120.0f;

    return tracks[(size_t) trackIndex].compressorReleasesMs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusCompressorReleaseMs(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 120.0f;

    return sendBuses[(size_t) sendIndex].compressorReleasesMs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackCompressorMakeupGainDb(int trackIndex, int moduleIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].compressorMakeupGainsDb[(size_t) moduleIndex].store(clampDbGain(gainDb, 0.0f, 24.0f),
                                                                                    std::memory_order_release);
}

void TapeEngine::setSendBusCompressorMakeupGainDb(int sendIndex, int moduleIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].compressorMakeupGainsDb[(size_t) moduleIndex].store(clampDbGain(gainDb, 0.0f, 24.0f),
                                                                                       std::memory_order_release);
}

float TapeEngine::getTrackCompressorMakeupGainDb(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].compressorMakeupGainsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusCompressorMakeupGainDb(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return sendBuses[(size_t) sendIndex].compressorMakeupGainsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackSaturationMode(int trackIndex, int moduleIndex, SaturationMode mode)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].saturationModes[(size_t) moduleIndex].store((int) mode, std::memory_order_release);
}

void TapeEngine::setSendBusSaturationMode(int sendIndex, int moduleIndex, SaturationMode mode)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].saturationModes[(size_t) moduleIndex].store((int) mode, std::memory_order_release);
}

SaturationMode TapeEngine::getTrackSaturationMode(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return SaturationMode::light;

    return (SaturationMode) tracks[(size_t) trackIndex].saturationModes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

SaturationMode TapeEngine::getSendBusSaturationMode(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return SaturationMode::light;

    return (SaturationMode) sendBuses[(size_t) sendIndex].saturationModes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackSaturationAmount(int trackIndex, int moduleIndex, float amount)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].saturationAmounts[(size_t) moduleIndex].store(juce::jlimit(0.0f, 1.0f, amount),
                                                                              std::memory_order_release);
}

void TapeEngine::setSendBusSaturationAmount(int sendIndex, int moduleIndex, float amount)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].saturationAmounts[(size_t) moduleIndex].store(juce::jlimit(0.0f, 1.0f, amount),
                                                                                 std::memory_order_release);
}

float TapeEngine::getTrackSaturationAmount(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.5f;

    return tracks[(size_t) trackIndex].saturationAmounts[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusSaturationAmount(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.5f;

    return sendBuses[(size_t) sendIndex].saturationAmounts[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackDelayTimeMs(int trackIndex, int moduleIndex, float timeMs)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].delayTimesMs[(size_t) moduleIndex].store(clampDelayTimeMs(timeMs), std::memory_order_release);
}

void TapeEngine::setSendBusDelayTimeMs(int sendIndex, int moduleIndex, float timeMs)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].delayTimesMs[(size_t) moduleIndex].store(clampDelayTimeMs(timeMs), std::memory_order_release);
}

float TapeEngine::getTrackDelayTimeMs(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 380.0f;

    return tracks[(size_t) trackIndex].delayTimesMs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusDelayTimeMs(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 380.0f;

    return sendBuses[(size_t) sendIndex].delayTimesMs[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackDelaySyncEnabled(int trackIndex, int moduleIndex, bool shouldBeEnabled)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].delaySyncEnableds[(size_t) moduleIndex].store(shouldBeEnabled, std::memory_order_release);
}

void TapeEngine::setSendBusDelaySyncEnabled(int sendIndex, int moduleIndex, bool shouldBeEnabled)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].delaySyncEnableds[(size_t) moduleIndex].store(shouldBeEnabled, std::memory_order_release);
}

bool TapeEngine::isTrackDelaySyncEnabled(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return true;

    return tracks[(size_t) trackIndex].delaySyncEnableds[(size_t) moduleIndex].load(std::memory_order_acquire);
}

bool TapeEngine::isSendBusDelaySyncEnabled(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return true;

    return sendBuses[(size_t) sendIndex].delaySyncEnableds[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackDelaySyncIndex(int trackIndex, int moduleIndex, int index)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].delaySyncIndices[(size_t) moduleIndex].store(juce::jlimit(0, getNumDelaySyncOptions() - 1, index),
                                                                              std::memory_order_release);
}

void TapeEngine::setSendBusDelaySyncIndex(int sendIndex, int moduleIndex, int index)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].delaySyncIndices[(size_t) moduleIndex].store(juce::jlimit(0, getNumDelaySyncOptions() - 1, index),
                                                                                std::memory_order_release);
}

int TapeEngine::getTrackDelaySyncIndex(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 2;

    return juce::jlimit(0,
                        getNumDelaySyncOptions() - 1,
                        tracks[(size_t) trackIndex].delaySyncIndices[(size_t) moduleIndex].load(std::memory_order_acquire));
}

int TapeEngine::getSendBusDelaySyncIndex(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 2;

    return juce::jlimit(0,
                        getNumDelaySyncOptions() - 1,
                        sendBuses[(size_t) sendIndex].delaySyncIndices[(size_t) moduleIndex].load(std::memory_order_acquire));
}

float TapeEngine::getTrackResolvedDelayTimeMs(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 380.0f;

    return getResolvedDelayTimeMs(tracks[(size_t) trackIndex], moduleIndex);
}

float TapeEngine::getSendBusResolvedDelayTimeMs(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 380.0f;

    return getResolvedDelayTimeMs(sendBuses[(size_t) sendIndex], moduleIndex);
}

int TapeEngine::getNumDelaySyncOptions() noexcept
{
    return (int) delaySyncOptions.size();
}

juce::String TapeEngine::getDelaySyncLabel(int index)
{
    return delaySyncOptions[(size_t) juce::jlimit(0, getNumDelaySyncOptions() - 1, index)].label;
}

void TapeEngine::setTrackDelayFeedback(int trackIndex, int moduleIndex, float feedback)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].delayFeedbacks[(size_t) moduleIndex].store(juce::jlimit(0.0f, 0.95f, feedback), std::memory_order_release);
}

void TapeEngine::setSendBusDelayFeedback(int sendIndex, int moduleIndex, float feedback)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].delayFeedbacks[(size_t) moduleIndex].store(juce::jlimit(0.0f, 0.95f, feedback),
                                                                              std::memory_order_release);
}

float TapeEngine::getTrackDelayFeedback(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.35f;

    return tracks[(size_t) trackIndex].delayFeedbacks[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusDelayFeedback(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.35f;

    return sendBuses[(size_t) sendIndex].delayFeedbacks[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackDelayMix(int trackIndex, int moduleIndex, float mix)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].delayMixes[(size_t) moduleIndex].store(clampUnit(mix), std::memory_order_release);
}

void TapeEngine::setSendBusDelayMix(int sendIndex, int moduleIndex, float mix)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].delayMixes[(size_t) moduleIndex].store(clampUnit(mix), std::memory_order_release);
}

float TapeEngine::getTrackDelayMix(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.25f;

    return tracks[(size_t) trackIndex].delayMixes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusDelayMix(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.25f;

    return sendBuses[(size_t) sendIndex].delayMixes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackPhaserSyncEnabled(int trackIndex, int moduleIndex, bool shouldBeEnabled)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].phaserSyncEnableds[(size_t) moduleIndex].store(shouldBeEnabled, std::memory_order_release);
}

void TapeEngine::setSendBusPhaserSyncEnabled(int sendIndex, int moduleIndex, bool shouldBeEnabled)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].phaserSyncEnableds[(size_t) moduleIndex].store(shouldBeEnabled, std::memory_order_release);
}

bool TapeEngine::isTrackPhaserSyncEnabled(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return false;

    return tracks[(size_t) trackIndex].phaserSyncEnableds[(size_t) moduleIndex].load(std::memory_order_acquire);
}

bool TapeEngine::isSendBusPhaserSyncEnabled(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return false;

    return sendBuses[(size_t) sendIndex].phaserSyncEnableds[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackPhaserSyncIndex(int trackIndex, int moduleIndex, int index)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].phaserSyncIndices[(size_t) moduleIndex].store(juce::jlimit(0, getNumDelaySyncOptions() - 1, index),
                                                                               std::memory_order_release);
}

void TapeEngine::setSendBusPhaserSyncIndex(int sendIndex, int moduleIndex, int index)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].phaserSyncIndices[(size_t) moduleIndex].store(juce::jlimit(0, getNumDelaySyncOptions() - 1, index),
                                                                                 std::memory_order_release);
}

int TapeEngine::getTrackPhaserSyncIndex(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 2;

    return juce::jlimit(0,
                        getNumDelaySyncOptions() - 1,
                        tracks[(size_t) trackIndex].phaserSyncIndices[(size_t) moduleIndex].load(std::memory_order_acquire));
}

int TapeEngine::getSendBusPhaserSyncIndex(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 2;

    return juce::jlimit(0,
                        getNumDelaySyncOptions() - 1,
                        sendBuses[(size_t) sendIndex].phaserSyncIndices[(size_t) moduleIndex].load(std::memory_order_acquire));
}

float TapeEngine::getTrackResolvedPhaserRateHz(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.5f;

    return getResolvedPhaserRateHz(tracks[(size_t) trackIndex], moduleIndex);
}

float TapeEngine::getSendBusResolvedPhaserRateHz(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.5f;

    return getResolvedPhaserRateHz(sendBuses[(size_t) sendIndex], moduleIndex);
}

void TapeEngine::setTrackReverbSize(int trackIndex, int moduleIndex, float size)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].reverbSizes[(size_t) moduleIndex].store(clampUnit(size), std::memory_order_release);
}

void TapeEngine::setSendBusReverbSize(int sendIndex, int moduleIndex, float size)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].reverbSizes[(size_t) moduleIndex].store(clampUnit(size), std::memory_order_release);
}

float TapeEngine::getTrackReverbSize(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.45f;

    return tracks[(size_t) trackIndex].reverbSizes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusReverbSize(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.45f;

    return sendBuses[(size_t) sendIndex].reverbSizes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackReverbDamping(int trackIndex, int moduleIndex, float damping)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].reverbDampings[(size_t) moduleIndex].store(clampUnit(damping), std::memory_order_release);
}

void TapeEngine::setSendBusReverbDamping(int sendIndex, int moduleIndex, float damping)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].reverbDampings[(size_t) moduleIndex].store(clampUnit(damping), std::memory_order_release);
}

float TapeEngine::getTrackReverbDamping(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.35f;

    return tracks[(size_t) trackIndex].reverbDampings[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusReverbDamping(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.35f;

    return sendBuses[(size_t) sendIndex].reverbDampings[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackReverbMix(int trackIndex, int moduleIndex, float mix)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].reverbMixes[(size_t) moduleIndex].store(clampUnit(mix), std::memory_order_release);
}

void TapeEngine::setSendBusReverbMix(int sendIndex, int moduleIndex, float mix)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].reverbMixes[(size_t) moduleIndex].store(clampUnit(mix), std::memory_order_release);
}

float TapeEngine::getTrackReverbMix(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.25f;

    return tracks[(size_t) trackIndex].reverbMixes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusReverbMix(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.25f;

    return sendBuses[(size_t) sendIndex].reverbMixes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackPhaserRate(int trackIndex, int moduleIndex, float rate)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].phaserRates[(size_t) moduleIndex].store(clampPhaserRate(rate), std::memory_order_release);
}

void TapeEngine::setSendBusPhaserRate(int sendIndex, int moduleIndex, float rate)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].phaserRates[(size_t) moduleIndex].store(clampPhaserRate(rate), std::memory_order_release);
}

float TapeEngine::getTrackPhaserRate(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.35f;

    return tracks[(size_t) trackIndex].phaserRates[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusPhaserRate(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.35f;

    return sendBuses[(size_t) sendIndex].phaserRates[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackPhaserDepth(int trackIndex, int moduleIndex, float depth)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].phaserDepths[(size_t) moduleIndex].store(clampPhaserDepth(depth), std::memory_order_release);
}

void TapeEngine::setSendBusPhaserDepth(int sendIndex, int moduleIndex, float depth)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].phaserDepths[(size_t) moduleIndex].store(clampPhaserDepth(depth), std::memory_order_release);
}

float TapeEngine::getTrackPhaserDepth(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.8f;

    return tracks[(size_t) trackIndex].phaserDepths[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusPhaserDepth(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.8f;

    return sendBuses[(size_t) sendIndex].phaserDepths[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackPhaserCentreFrequency(int trackIndex, int moduleIndex, float centreFrequency)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].phaserCentreFrequencies[(size_t) moduleIndex].store(clampPhaserCentreFrequency(centreFrequency),
                                                                                    std::memory_order_release);
}

void TapeEngine::setSendBusPhaserCentreFrequency(int sendIndex, int moduleIndex, float centreFrequency)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].phaserCentreFrequencies[(size_t) moduleIndex].store(clampPhaserCentreFrequency(centreFrequency),
                                                                                       std::memory_order_release);
}

float TapeEngine::getTrackPhaserCentreFrequency(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 700.0f;

    return tracks[(size_t) trackIndex].phaserCentreFrequencies[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusPhaserCentreFrequency(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 700.0f;

    return sendBuses[(size_t) sendIndex].phaserCentreFrequencies[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackPhaserFeedback(int trackIndex, int moduleIndex, float feedback)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].phaserFeedbacks[(size_t) moduleIndex].store(clampPhaserFeedback(feedback), std::memory_order_release);
}

void TapeEngine::setSendBusPhaserFeedback(int sendIndex, int moduleIndex, float feedback)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].phaserFeedbacks[(size_t) moduleIndex].store(clampPhaserFeedback(feedback), std::memory_order_release);
}

float TapeEngine::getTrackPhaserFeedback(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.3f;

    return tracks[(size_t) trackIndex].phaserFeedbacks[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusPhaserFeedback(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.3f;

    return sendBuses[(size_t) sendIndex].phaserFeedbacks[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackPhaserMix(int trackIndex, int moduleIndex, float mix)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].phaserMixes[(size_t) moduleIndex].store(clampPhaserMix(mix), std::memory_order_release);
}

void TapeEngine::setSendBusPhaserMix(int sendIndex, int moduleIndex, float mix)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].phaserMixes[(size_t) moduleIndex].store(clampPhaserMix(mix), std::memory_order_release);
}

float TapeEngine::getTrackPhaserMix(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.5f;

    return tracks[(size_t) trackIndex].phaserMixes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusPhaserMix(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.5f;

    return sendBuses[(size_t) sendIndex].phaserMixes[(size_t) moduleIndex].load(std::memory_order_acquire);
}

void TapeEngine::setTrackGainModuleGainDb(int trackIndex, int moduleIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].gainModuleGainsDb[(size_t) moduleIndex].store(clampDbGain(gainDb, -24.0f, 24.0f),
                                                                              std::memory_order_release);
}

void TapeEngine::setTrackGainModulePan(int trackIndex, int moduleIndex, float pan)
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    tracks[(size_t) trackIndex].gainModulePans[(size_t) moduleIndex].store(juce::jlimit(-1.0f, 1.0f, pan), std::memory_order_release);
}

void TapeEngine::setSendBusGainModuleGainDb(int sendIndex, int moduleIndex, float gainDb)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].gainModuleGainsDb[(size_t) moduleIndex].store(clampDbGain(gainDb, -24.0f, 24.0f),
                                                                                 std::memory_order_release);
}

void TapeEngine::setSendBusGainModulePan(int sendIndex, int moduleIndex, float pan)
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return;

    sendBuses[(size_t) sendIndex].gainModulePans[(size_t) moduleIndex].store(juce::jlimit(-1.0f, 1.0f, pan), std::memory_order_release);
}

float TapeEngine::getTrackGainModuleGainDb(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].gainModuleGainsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getTrackGainModulePan(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].gainModulePans[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusGainModuleGainDb(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return sendBuses[(size_t) sendIndex].gainModuleGainsDb[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusGainModulePan(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return sendBuses[(size_t) sendIndex].gainModulePans[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getTrackModuleInputMeter(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].moduleInputMeters[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusModuleInputMeter(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return sendBuses[(size_t) sendIndex].moduleInputMeters[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getTrackModuleOutputMeter(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return tracks[(size_t) trackIndex].moduleOutputMeters[(size_t) moduleIndex].load(std::memory_order_acquire);
}

float TapeEngine::getSendBusModuleOutputMeter(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.0f;

    return sendBuses[(size_t) sendIndex].moduleOutputMeters[(size_t) moduleIndex].load(std::memory_order_acquire);
}

std::array<float, ModuleChain::spectrumAnalyzerBinCount> TapeEngine::getTrackSpectrumAnalyzerData(int trackIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(trackIndex, numTracks) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return {};

    return loadSpectrumAnalyzerData(tracks[(size_t) trackIndex].spectrumAnalyzerStates[(size_t) moduleIndex]);
}

std::array<float, ModuleChain::spectrumAnalyzerBinCount> TapeEngine::getSendBusSpectrumAnalyzerData(int sendIndex, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses) || ! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return {};

    return loadSpectrumAnalyzerData(sendBuses[(size_t) sendIndex].spectrumAnalyzerStates[(size_t) moduleIndex]);
}

float TapeEngine::getSendBusOutputMeter(int sendIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(sendIndex, numSendBuses))
        return 0.0f;

    return sendBuses[(size_t) sendIndex].outputMeter.load(std::memory_order_acquire);
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

    for (auto& sendBus : sendBuses)
    {
        for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
        {
            const auto requestedType = (ChainModuleType) sendBus.moduleTypes[(size_t) moduleIndex].load(std::memory_order_acquire);
            const auto activeType = (ChainModuleType) sendBus.activeModuleTypes[(size_t) moduleIndex];
            const auto needsReset = sendBus.moduleResetRequested[(size_t) moduleIndex].exchange(false, std::memory_order_acq_rel);

            if (! needsReset && requestedType == activeType)
                continue;

            resetModuleState(sendBus, moduleIndex, requestedType);
            sendBus.activeModuleTypes[(size_t) moduleIndex] = (int) requestedType;
        }
    }
}

void TapeEngine::resetModuleState(ModuleChain& track, int moduleIndex, ChainModuleType type) noexcept
{
    track.filterStates[(size_t) moduleIndex].reset();
    track.compressorStates[(size_t) moduleIndex].reset();
    track.delayStates[(size_t) moduleIndex].reset();
    track.reverbStates[(size_t) moduleIndex].reset();
    track.phaserStates[(size_t) moduleIndex].reset();
    track.spectrumAnalyzerStates[(size_t) moduleIndex].reset();
    track.reverbStates[(size_t) moduleIndex].prepare(sampleRate);
    track.phaserStates[(size_t) moduleIndex].prepare(sampleRate);
    track.moduleBlockInputPeaks[(size_t) moduleIndex] = 0.0f;
    track.moduleBlockOutputPeaks[(size_t) moduleIndex] = 0.0f;
    track.moduleInputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
    track.moduleOutputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);

    for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
        track.eqStates[(size_t) moduleIndex][(size_t) bandIndex].reset();

    track.activeModuleTypes[(size_t) moduleIndex] = (int) type;
}

void TapeEngine::resetModuleParameters(ModuleChain& track, int moduleIndex, ChainModuleType type) noexcept
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
    track.delayTimesMs[(size_t) moduleIndex].store(380.0f, std::memory_order_relaxed);
    track.delaySyncEnableds[(size_t) moduleIndex].store(true, std::memory_order_relaxed);
    track.delaySyncIndices[(size_t) moduleIndex].store(2, std::memory_order_relaxed);
    track.delayFeedbacks[(size_t) moduleIndex].store(0.35f, std::memory_order_relaxed);
    track.delayMixes[(size_t) moduleIndex].store(0.25f, std::memory_order_relaxed);
    track.reverbSizes[(size_t) moduleIndex].store(0.45f, std::memory_order_relaxed);
    track.reverbDampings[(size_t) moduleIndex].store(0.35f, std::memory_order_relaxed);
    track.reverbMixes[(size_t) moduleIndex].store(0.25f, std::memory_order_relaxed);
    track.phaserRates[(size_t) moduleIndex].store(0.5f, std::memory_order_relaxed);
    track.phaserSyncEnableds[(size_t) moduleIndex].store(false, std::memory_order_relaxed);
    track.phaserSyncIndices[(size_t) moduleIndex].store(2, std::memory_order_relaxed);
    track.phaserDepths[(size_t) moduleIndex].store(0.8f, std::memory_order_relaxed);
    track.phaserCentreFrequencies[(size_t) moduleIndex].store(700.0f, std::memory_order_relaxed);
    track.phaserFeedbacks[(size_t) moduleIndex].store(0.3f, std::memory_order_relaxed);
    track.phaserMixes[(size_t) moduleIndex].store(0.5f, std::memory_order_relaxed);
    track.gainModuleGainsDb[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
    track.gainModulePans[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
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

float TapeEngine::processInputSample(Track& track, int channel, float sample) const noexcept
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

float TapeEngine::processChainModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
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
        case ChainModuleType::delay:
            output = processDelayModule(track, moduleIndex, channel, sample);
            break;
        case ChainModuleType::reverb:
            output = processReverbModule(track, moduleIndex, channel, sample);
            break;
        case ChainModuleType::phaser:
            output = processPhaserModule(track, moduleIndex, channel, sample);
            break;
        case ChainModuleType::spectrumAnalyzer:
            output = processSpectrumAnalyzerModule(track, moduleIndex, channel, sample);
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

float TapeEngine::processFilterModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
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

float TapeEngine::processEqModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
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

float TapeEngine::processCompressorModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
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

float TapeEngine::processSaturationModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
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

float TapeEngine::processDelayModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
{
    auto& state = track.delayStates[(size_t) moduleIndex];

    if (state.buffer.getNumSamples() <= 0)
        return sample;

    const auto channelIndex = juce::jlimit(0, Track::numChannels - 1, channel);
    const auto delaySamples = juce::jlimit(1,
                                           juce::jmax(1, state.maxDelaySamples - 1),
                                           (int) std::round((sampleRate * getResolvedDelayTimeMs(track, moduleIndex)) * 0.001));
    const auto readPosition = (state.writePosition - delaySamples + state.maxDelaySamples) % state.maxDelaySamples;
    const auto feedback = track.delayFeedbacks[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto mix = track.delayMixes[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto delayed = state.buffer.getSample(channelIndex, readPosition);
    state.buffer.setSample(channelIndex, state.writePosition, sample + (delayed * feedback));

    if (channelIndex == Track::numChannels - 1)
        state.writePosition = (state.writePosition + 1) % state.maxDelaySamples;

    return delayed * mix + sample * (1.0f - mix);
}

float TapeEngine::getResolvedDelayTimeMs(const ModuleChain& track, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 380.0f;

    if (! track.delaySyncEnableds[(size_t) moduleIndex].load(std::memory_order_relaxed))
        return clampDelayTimeMs(track.delayTimesMs[(size_t) moduleIndex].load(std::memory_order_relaxed));

    const auto beats = delaySyncOptions[(size_t) juce::jlimit(0,
                                                              getNumDelaySyncOptions() - 1,
                                                              track.delaySyncIndices[(size_t) moduleIndex].load(std::memory_order_relaxed))].beats;
    const auto currentBpm = juce::jmax(30.0, (double) bpm.load(std::memory_order_relaxed));
    return clampDelayTimeMs((float) ((60000.0 / currentBpm) * beats));
}

float TapeEngine::getResolvedPhaserRateHz(const ModuleChain& track, int moduleIndex) const noexcept
{
    if (! juce::isPositiveAndBelow(moduleIndex, Track::maxChainModules))
        return 0.5f;

    if (! track.phaserSyncEnableds[(size_t) moduleIndex].load(std::memory_order_relaxed))
        return clampPhaserRate(track.phaserRates[(size_t) moduleIndex].load(std::memory_order_relaxed));

    const auto beats = delaySyncOptions[(size_t) juce::jlimit(0,
                                                              getNumDelaySyncOptions() - 1,
                                                              track.phaserSyncIndices[(size_t) moduleIndex].load(std::memory_order_relaxed))].beats;
    const auto currentBpm = juce::jmax(30.0, (double) bpm.load(std::memory_order_relaxed));
    const auto cycleSeconds = juce::jmax(0.001, (60.0 / currentBpm) * beats);
    return clampPhaserRate((float) (1.0 / cycleSeconds));
}

float TapeEngine::processReverbModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
{
    const auto channelIndex = juce::jlimit(0, Track::numChannels - 1, channel);
    const auto size = track.reverbSizes[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto damping = track.reverbDampings[(size_t) moduleIndex].load(std::memory_order_relaxed);
    const auto mix = track.reverbMixes[(size_t) moduleIndex].load(std::memory_order_relaxed);
    auto& state = track.reverbStates[(size_t) moduleIndex];
    state.prepare(sampleRate);
    state.updateParameters(size, damping);
    auto wet = sample;
    state.reverbs[(size_t) channelIndex].processMono(&wet, 1);
    return wet * mix + sample * (1.0f - mix);
}

float TapeEngine::processPhaserModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
{
    const auto channelIndex = juce::jlimit(0, Track::numChannels - 1, channel);
    const auto rate = getResolvedPhaserRateHz(track, moduleIndex);
    const auto depth = clampPhaserDepth(track.phaserDepths[(size_t) moduleIndex].load(std::memory_order_relaxed));
    const auto centreFrequency = clampPhaserCentreFrequency(track.phaserCentreFrequencies[(size_t) moduleIndex].load(std::memory_order_relaxed));
    const auto feedback = clampPhaserFeedback(track.phaserFeedbacks[(size_t) moduleIndex].load(std::memory_order_relaxed));
    const auto mix = clampPhaserMix(track.phaserMixes[(size_t) moduleIndex].load(std::memory_order_relaxed));
    auto& state = track.phaserStates[(size_t) moduleIndex];
    state.prepare(sampleRate);
    state.updateParameters(rate, depth, centreFrequency, feedback, mix);
    auto processed = sample;
    float* channelData[] { &processed };
    juce::dsp::AudioBlock<float> block(channelData, 1, 1);
    juce::dsp::ProcessContextReplacing<float> context(block);
    state.phasers[(size_t) channelIndex].process(context);
    return processed;
}

float TapeEngine::processSpectrumAnalyzerModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
{
    auto& state = track.spectrumAnalyzerStates[(size_t) moduleIndex];

    if (channel <= 0)
    {
        state.pendingLeftSample = sample;
        state.hasPendingLeftSample = true;
        return sample;
    }

    if (state.hasPendingLeftSample)
    {
        pushSpectrumAnalyzerSample(state, 0.5f * (state.pendingLeftSample + sample), sampleRate);
        state.hasPendingLeftSample = false;
    }

    return sample;
}

float TapeEngine::processGainModule(ModuleChain& track, int moduleIndex, int channel, float sample) const noexcept
{
    auto output = sample * juce::Decibels::decibelsToGain(track.gainModuleGainsDb[(size_t) moduleIndex].load(std::memory_order_relaxed));
    const auto pan = juce::jlimit(-1.0f, 1.0f, track.gainModulePans[(size_t) moduleIndex].load(std::memory_order_relaxed));
    const auto channelIndex = juce::jlimit(0, Track::numChannels - 1, channel);

    if (channelIndex == 0 && pan > 0.0f)
        output *= 1.0f - pan;
    else if (channelIndex == 1 && pan < 0.0f)
        output *= 1.0f + pan;

    return output;
}

void TapeEngine::resetTrackRuntimeState(Track& track) noexcept
{
    track.peakMeter.store(0.0f, std::memory_order_relaxed);
    track.clipping.store(false, std::memory_order_relaxed);
    track.mixerMeter.store(0.0f, std::memory_order_relaxed);
    track.mixerBlockPeak = 0.0f;

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        track.moduleInputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
        track.moduleOutputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
        track.moduleBlockInputPeaks[(size_t) moduleIndex] = 0.0f;
        track.moduleBlockOutputPeaks[(size_t) moduleIndex] = 0.0f;
        track.filterStates[(size_t) moduleIndex].reset();
        track.compressorStates[(size_t) moduleIndex].reset();
        track.delayStates[(size_t) moduleIndex].prepare(sampleRate);
        track.delayStates[(size_t) moduleIndex].reset();
        track.reverbStates[(size_t) moduleIndex].reset();
        track.reverbStates[(size_t) moduleIndex].prepare(sampleRate);
        track.phaserStates[(size_t) moduleIndex].prepare(sampleRate);
        track.phaserStates[(size_t) moduleIndex].reset();
        track.spectrumAnalyzerStates[(size_t) moduleIndex].reset();

        for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
            track.eqStates[(size_t) moduleIndex][(size_t) bandIndex].reset();
    }
}

void TapeEngine::resetSendBusRuntimeState(SendBus& sendBus) noexcept
{
    sendBus.outputMeter.store(0.0f, std::memory_order_relaxed);
    sendBus.outputBlockPeak = 0.0f;

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        sendBus.moduleInputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
        sendBus.moduleOutputMeters[(size_t) moduleIndex].store(0.0f, std::memory_order_relaxed);
        sendBus.moduleBlockInputPeaks[(size_t) moduleIndex] = 0.0f;
        sendBus.moduleBlockOutputPeaks[(size_t) moduleIndex] = 0.0f;
        sendBus.filterStates[(size_t) moduleIndex].reset();
        sendBus.compressorStates[(size_t) moduleIndex].reset();
        sendBus.delayStates[(size_t) moduleIndex].prepare(sampleRate);
        sendBus.delayStates[(size_t) moduleIndex].reset();
        sendBus.reverbStates[(size_t) moduleIndex].reset();
        sendBus.reverbStates[(size_t) moduleIndex].prepare(sampleRate);
        sendBus.phaserStates[(size_t) moduleIndex].prepare(sampleRate);
        sendBus.phaserStates[(size_t) moduleIndex].reset();
        sendBus.spectrumAnalyzerStates[(size_t) moduleIndex].reset();

        for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
            sendBus.eqStates[(size_t) moduleIndex][(size_t) bandIndex].reset();
    }
}
