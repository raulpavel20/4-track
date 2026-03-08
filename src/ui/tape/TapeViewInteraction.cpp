#include "../../TapeView.h"

#include <cmath>

void TapeView::mouseDown(const juce::MouseEvent& event)
{
    if (getReversePreviewButtonBounds().contains(event.position.toInt()))
    {
        reversePreviewPressed = true;
        engine.startReversePlayback();
        repaint();
        return;
    }

    if (getPlayStopButtonBounds().contains(event.position.toInt()))
    {
        if (engine.isPlaying() || engine.isCountInActive())
            engine.stop();
        else
            engine.play();

        repaint();
        return;
    }

    if (getRewindButtonBounds().contains(event.position.toInt()))
    {
        engine.rewind();
        repaint();
        return;
    }

    if (getMetronomeButtonBounds().contains(event.position.toInt()))
    {
        engine.setMetronomeEnabled(! engine.isMetronomeEnabled());
        repaint();
        return;
    }

    if (getLoopButtonBounds().contains(event.position.toInt()))
    {
        if (engine.canEditLoopMarkers())
            engine.toggleLoopMarkerAtNearestBeat();

        repaint();
        return;
    }

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        if (getLaneBounds(trackIndex).contains(event.position.toInt()))
        {
            setSelectedTrack(trackIndex);
            break;
        }
    }

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        for (int buttonIndex = 0; buttonIndex < 3; ++buttonIndex)
        {
            if (! getModeButtonBounds(trackIndex, buttonIndex).contains(event.position.toInt()))
                continue;

            const auto modeForButton = buttonIndex == 0 ? TrackRecordMode::overdub
                                                        : buttonIndex == 1 ? TrackRecordMode::replace
                                                                           : TrackRecordMode::erase;
            const auto currentMode = engine.getTrackRecordMode(trackIndex);
            const auto pendingMode = engine.getPendingTrackRecordMode(trackIndex);
            const auto nextMode = pendingMode == modeForButton ? TrackRecordMode::off
                                                               : pendingMode != TrackRecordMode::off ? modeForButton
                                                                                                     : currentMode == modeForButton ? TrackRecordMode::off
                                                                                                                                   : modeForButton;
            engine.requestTrackRecordMode(trackIndex, nextMode);
            repaint();
            return;
        }
    }

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        for (int buttonIndex = 0; buttonIndex < 2; ++buttonIndex)
        {
            if (! getUtilityButtonBounds(trackIndex, buttonIndex).contains(event.position.toInt()))
                continue;

            if (buttonIndex == 0)
                engine.setTrackSolo(trackIndex, ! engine.isTrackSolo(trackIndex));
            else
                engine.setTrackMuted(trackIndex, ! engine.isTrackMuted(trackIndex));

            repaint();
            return;
        }
    }

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        if (! getWaveformBounds(trackIndex).expanded(0, 16).contains(event.position.toInt()))
            continue;

        isScrubbing = true;
        scrubStartX = event.position.x;
        scrubStartPlayhead = engine.getPlayheadSample();
        scrubTo(event.position);
        return;
    }
}

void TapeView::mouseDrag(const juce::MouseEvent& event)
{
    if (isScrubbing)
        scrubTo(event.position);
}

void TapeView::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (reversePreviewPressed)
    {
        reversePreviewPressed = false;
        engine.stopReversePlayback();
    }

    isScrubbing = false;
}

void TapeView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    auto isOverTrack = false;

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        if (getLaneBounds(trackIndex).contains(event.position.toInt()))
        {
            isOverTrack = true;
            break;
        }
    }

    if (! isOverTrack)
    {
        juce::Component::mouseWheelMove(event, wheel);
        return;
    }

    const auto wheelDelta = std::abs(wheel.deltaY) >= std::abs(wheel.deltaX) ? wheel.deltaY
                                                                              : wheel.deltaX;

    if (std::abs(wheelDelta) < 0.0001f)
        return;

    if (event.mods.isShiftDown())
    {
        zoomSlider.setValue(juce::jlimit(0.0, 1.0, zoomSlider.getValue() + (double) wheelDelta * 0.12),
                            juce::sendNotificationSync);
        repaint();
        return;
    }

    const auto scrubAmount = getVisibleSamples() * 0.12;
    engine.setPlayheadSample(engine.getPlayheadSample() - ((double) wheelDelta * scrubAmount));
    repaint();
}

void TapeView::scrubTo(juce::Point<float> position)
{
    if (! isScrubbing || ! getTrackSectionBounds().contains(position.toInt()))
        return;

    const auto pixelsPerSample = (double) getTrackSectionBounds().getWidth() / getVisibleSamples();
    const auto scrubPixelsPerSample = pixelsPerSample * 4.0;
    const auto sampleOffset = ((double) scrubStartX - (double) position.x) / scrubPixelsPerSample;
    engine.setPlayheadSample(scrubStartPlayhead + sampleOffset);
}
