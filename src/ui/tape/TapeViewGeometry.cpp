#include "../../TapeView.h"

#include <cmath>

juce::Rectangle<int> TapeView::getLaneBounds(int trackIndex) const
{
    auto bounds = getTrackSectionBounds().reduced(0, 8);
    bounds.setRight(getZoomSliderBounds().getX() - 20);
    const auto laneHeight = bounds.getHeight() / TapeEngine::numTracks;
    auto laneBounds = juce::Rectangle<int>(bounds.getX(),
                                           bounds.getY() + (laneHeight * trackIndex),
                                           bounds.getWidth(),
                                           laneHeight);
    return laneBounds.reduced(0, 5);
}

juce::Rectangle<int> TapeView::getTapeAreaBounds() const
{
    return getLocalBounds().reduced(6);
}

juce::Rectangle<int> TapeView::getTrackSectionBounds() const
{
    auto bounds = getTapeAreaBounds().reduced(18);
    bounds.removeFromTop(198);
    return bounds;
}

juce::Rectangle<int> TapeView::getReelSectionBounds() const
{
    auto bounds = getTapeAreaBounds().reduced(18);
    return bounds.removeFromTop(186);
}

juce::Rectangle<int> TapeView::getPlayStopButtonBounds() const
{
    auto reelBounds = getReelSectionBounds();
    return juce::Rectangle<int>(reelBounds.getRight() - 18 - 64, reelBounds.getY() + 14, 64, 64);
}

juce::Rectangle<int> TapeView::getRewindButtonBounds() const
{
    auto playBounds = getPlayStopButtonBounds();
    return playBounds.translated(-76, 0);
}

juce::Rectangle<int> TapeView::getReversePreviewButtonBounds() const
{
    auto rewindBounds = getRewindButtonBounds();
    return rewindBounds.translated(-76, 0);
}

juce::Rectangle<int> TapeView::getMetronomeButtonBounds() const
{
    auto playBounds = getPlayStopButtonBounds();
    return juce::Rectangle<int>(playBounds.getX(), playBounds.getBottom() + 14, 64, 64);
}

juce::Rectangle<int> TapeView::getLoopButtonBounds() const
{
    auto metronomeBounds = getMetronomeButtonBounds();
    return metronomeBounds.translated(-76, 0);
}

juce::Rectangle<int> TapeView::getBpmEditorBounds() const
{
    auto reelBounds = getReelSectionBounds();
    return juce::Rectangle<int>(reelBounds.getX() + 22, reelBounds.getY() + 78, 64, 34);
}

juce::Rectangle<int> TapeView::getBeatsPerBarEditorBounds() const
{
    auto bpmBounds = getBpmEditorBounds();
    return bpmBounds.translated(0, bpmBounds.getHeight() + 10).withWidth(40);
}

juce::Rectangle<int> TapeView::getControlClusterBounds(int trackIndex) const
{
    auto laneBounds = getLaneBounds(trackIndex).reduced(10, 6);
    laneBounds.removeFromLeft(8);
    return juce::Rectangle<int>(laneBounds.getX(), laneBounds.getCentreY() - 29, 92, 58);
}

juce::Rectangle<int> TapeView::getModeButtonBounds(int trackIndex, int buttonIndex) const
{
    auto bounds = getControlClusterBounds(trackIndex);
    const auto buttonSize = 26;
    const auto gap = 6;
    return juce::Rectangle<int>(bounds.getX() + (buttonIndex * (buttonSize + gap)),
                                bounds.getY(),
                                buttonSize,
                                buttonSize);
}

juce::Rectangle<int> TapeView::getUtilityButtonBounds(int trackIndex, int buttonIndex) const
{
    auto bounds = getControlClusterBounds(trackIndex);
    const auto buttonSize = 26;
    const auto gap = 6;
    const auto rowWidth = (buttonSize * 2) + gap;
    const auto startX = bounds.getX() + ((bounds.getWidth() - rowWidth) / 2);
    return juce::Rectangle<int>(startX + (buttonIndex * (buttonSize + gap)),
                                bounds.getBottom() - buttonSize,
                                buttonSize,
                                buttonSize);
}

juce::Rectangle<int> TapeView::getZoomSliderBounds() const
{
    auto bounds = getTrackSectionBounds().reduced(0, 18);
    return juce::Rectangle<int>(bounds.getRight() - 10, bounds.getY(), 20, bounds.getHeight());
}

juce::Rectangle<int> TapeView::getWaveformBounds(int trackIndex) const
{
    auto laneBounds = getLaneBounds(trackIndex).reduced(12, 10);
    const auto controlBounds = getControlClusterBounds(trackIndex);
    const auto waveformLeft = controlBounds.getRight() + 12;
    const auto meterX = getZoomSliderBounds().getX() - 12;
    const auto maxWaveformRight = meterX - 8;
    laneBounds.setX(waveformLeft);
    laneBounds.setRight(maxWaveformRight);
    return laneBounds;
}

juce::Rectangle<int> TapeView::getMeterBounds(int trackIndex) const
{
    auto waveformBounds = getWaveformBounds(trackIndex);
    return juce::Rectangle<int>(getZoomSliderBounds().getX() - 14,
                                waveformBounds.getY(),
                                12,
                                waveformBounds.getHeight());
}

double TapeView::getVisibleSamples() const
{
    const auto currentBpm = juce::jmax(30.0, (double) engine.getBpm());
    const auto samplesPerBeat = (engine.getSampleRate() * 60.0) / currentBpm;
    constexpr double minVisibleBeats = 4.0;
    constexpr double maxVisibleBeats = 64.0;
    const auto zoomValue = juce::jlimit(0.0, 1.0, zoomSlider.getValue());
    const auto visibleBeats = minVisibleBeats * std::pow(maxVisibleBeats / minVisibleBeats, 1.0 - zoomValue);
    return juce::jmax(1.0, visibleBeats * samplesPerBeat);
}

juce::Range<float> TapeView::getWaveformExtents(int trackIndex, int startSample, int endSample) const
{
    const auto recordedLength = engine.getTrackRecordedLength(trackIndex);

    if (recordedLength <= 0)
        return {};

    if (endSample <= 0 || startSample >= recordedLength)
        return {};

    const auto clampedStart = juce::jlimit(0, recordedLength, startSample);
    const auto clampedEnd = juce::jlimit(0, recordedLength, endSample);

    if (clampedEnd <= clampedStart)
        return {};

    const auto range = clampedEnd - clampedStart;
    const auto step = juce::jmax(1, range / 32);
    auto minSample = 1.0f;
    auto maxSample = -1.0f;

    for (int sample = clampedStart; sample < clampedEnd; sample += step)
    {
        const auto left = juce::jlimit(-1.0f, 1.0f, engine.getTrackSample(trackIndex, 0, sample));
        const auto right = juce::jlimit(-1.0f, 1.0f, engine.getTrackSample(trackIndex, 1, sample));
        minSample = juce::jmin(minSample, juce::jmin(left, right));
        maxSample = juce::jmax(maxSample, juce::jmax(left, right));
    }

    const auto finalSample = clampedEnd - 1;
    const auto finalLeft = juce::jlimit(-1.0f, 1.0f, engine.getTrackSample(trackIndex, 0, finalSample));
    const auto finalRight = juce::jlimit(-1.0f, 1.0f, engine.getTrackSample(trackIndex, 1, finalSample));
    minSample = juce::jmin(minSample, juce::jmin(finalLeft, finalRight));
    maxSample = juce::jmax(maxSample, juce::jmax(finalLeft, finalRight));

    if (maxSample < minSample)
        return {};

    return { minSample, maxSample };
}
