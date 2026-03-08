#include "../../TapeView.h"

#include "../../AppFonts.h"
#include "../../AppSettings.h"

#include <cmath>

namespace
{
juce::Colour getTrackColour(int index)
{
    return AppSettings::getInstance().getTrackColour(juce::jlimit(0, TapeEngine::numTracks - 1, index));
}

juce::Path createReelPath(juce::Point<float> centre, float radius, float phase)
{
    juce::Path path;
    path.addEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    path.addEllipse(centre.x - (radius * 0.18f), centre.y - (radius * 0.18f), radius * 0.36f, radius * 0.36f);

    for (int spokeIndex = 0; spokeIndex < 3; ++spokeIndex)
    {
        const auto angle = phase + (juce::MathConstants<float>::twoPi * (float) spokeIndex / 3.0f);
        const auto inner = juce::Point<float>(centre.x + std::cos(angle) * (radius * 0.22f),
                                              centre.y + std::sin(angle) * (radius * 0.22f));
        const auto outer = juce::Point<float>(centre.x + std::cos(angle) * (radius * 0.78f),
                                              centre.y + std::sin(angle) * (radius * 0.78f));
        path.startNewSubPath(inner);
        path.lineTo(outer);
    }

    return path;
}

juce::Path createPlayIcon(juce::Rectangle<float> bounds)
{
    juce::Path path;
    path.startNewSubPath(bounds.getX(), bounds.getY());
    path.lineTo(bounds.getRight(), bounds.getCentreY());
    path.lineTo(bounds.getX(), bounds.getBottom());
    path.closeSubPath();
    return path;
}

juce::Path createRewindIcon(juce::Rectangle<float> bounds)
{
    juce::Path path;
    const auto halfWidth = bounds.getWidth() * 0.5f;

    path.startNewSubPath(bounds.getX() + halfWidth, bounds.getY());
    path.lineTo(bounds.getX(), bounds.getCentreY());
    path.lineTo(bounds.getX() + halfWidth, bounds.getBottom());
    path.closeSubPath();

    path.startNewSubPath(bounds.getRight(), bounds.getY());
    path.lineTo(bounds.getX() + halfWidth, bounds.getCentreY());
    path.lineTo(bounds.getRight(), bounds.getBottom());
    path.closeSubPath();

    return path;
}

juce::Path createReversePreviewIcon(juce::Rectangle<float> bounds)
{
    juce::Path path;
    path.startNewSubPath(bounds.getRight(), bounds.getY());
    path.lineTo(bounds.getX(), bounds.getCentreY());
    path.lineTo(bounds.getRight(), bounds.getBottom());
    path.closeSubPath();
    return path;
}

juce::Path createMetronomeIcon(juce::Rectangle<float> bounds)
{
    juce::Path path;
    const auto left = bounds.getX() + (bounds.getWidth() * 0.22f);
    const auto right = bounds.getRight() - (bounds.getWidth() * 0.22f);
    const auto top = bounds.getY() + (bounds.getHeight() * 0.2f);
    const auto upperLeft = bounds.getX() + (bounds.getWidth() * 0.38f);
    const auto upperRight = bounds.getRight() - (bounds.getWidth() * 0.38f);
    const auto bottom = bounds.getBottom() - (bounds.getHeight() * 0.14f);
    path.startNewSubPath(left, bottom);
    path.lineTo(right, bottom);
    path.lineTo(upperRight, top + 4.0f);
    path.lineTo(upperLeft, top + 4.0f);
    path.closeSubPath();
    path.addEllipse(bounds.getCentreX() - 2.0f, top - 4.0f, 4.0f, 4.0f);
    path.startNewSubPath(upperLeft + 2.0f, top + 1.0f);
    path.lineTo(upperRight - 2.0f, top + 1.0f);
    return path;
}

juce::Path createLoopIcon(juce::Rectangle<float> bounds)
{
    juce::Path path;
    const auto circleDiameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.52f;
    const auto circleX = bounds.getCentreX() - (circleDiameter * 0.5f);
    const auto circleY = bounds.getY() + (bounds.getHeight() * 0.14f);
    path.addEllipse(circleX, circleY, circleDiameter, circleDiameter);
    path.startNewSubPath(bounds.getCentreX(), circleY + circleDiameter + 2.0f);
    path.lineTo(bounds.getCentreX(), bounds.getBottom() - 2.0f);
    return path;
}

juce::Path createEraseIcon(juce::Rectangle<float> bounds)
{
    juce::Path path;
    path.startNewSubPath(bounds.getX(), bounds.getY());
    path.lineTo(bounds.getRight(), bounds.getBottom());
    path.startNewSubPath(bounds.getRight(), bounds.getY());
    path.lineTo(bounds.getX(), bounds.getBottom());
    return path;
}

juce::Path createReplaceIcon(juce::Rectangle<float> bounds)
{
    juce::Path path;
    path.addRectangle(bounds);
    return path;
}

juce::Path createTapePath(juce::Point<float> leftReelCentre,
                          juce::Point<float> rightReelCentre,
                          float reelRadius,
                          float playheadX,
                          float topY,
                          float bottomY)
{
    juce::Path path;
    const auto YOffset = 50.0f;
    const auto leftStart = juce::Point<float>(leftReelCentre.x + (reelRadius * 0.82f),
                                              leftReelCentre.y + (reelRadius * 0.06f) + YOffset);
    const auto rightStart = juce::Point<float>(rightReelCentre.x - (reelRadius * 0.82f),
                                               rightReelCentre.y + (reelRadius * 0.06f) + YOffset);
    const auto shoulderOffset = 36.0f;
    const auto neckHalfWidth = 7.0f;

    path.startNewSubPath(leftStart);
    path.lineTo(playheadX - shoulderOffset, topY);
    path.lineTo(playheadX - neckHalfWidth, topY);
    path.startNewSubPath(playheadX + neckHalfWidth, topY);
    path.lineTo(playheadX + shoulderOffset, topY);
    path.lineTo(rightStart);
    path.startNewSubPath(playheadX, topY + 1.0f);
    path.lineTo(playheadX, bottomY);
    return path;
}
}

void TapeView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    const auto playheadSample = displayedPlayhead;
    const auto reelSectionBounds = getReelSectionBounds();
    const auto trackSectionBounds = getTrackSectionBounds();
    const auto accent = getTrackColour(selectedTrack);
    const auto motionDelta = std::abs(playheadSample - lastDisplayedPlayhead);
    const auto isMoving = engine.isPlaying() || engine.isRewinding() || engine.isReversePlaying() || isScrubbing || motionDelta > 1.0;

    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawRoundedRectangle(getTapeAreaBounds().toFloat(), 18.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(60.0f));

    const auto seconds = playheadSample / juce::jmax(1.0, engine.getSampleRate());
    const auto totalSeconds = (int) seconds;
    const auto minutesText = juce::String(totalSeconds / 60).paddedLeft('0', 1);
    const auto secondsText = juce::String(totalSeconds % 60).paddedLeft('0', 2);
    const auto centisecondsText = juce::String((int) std::fmod(seconds * 100.0, 100.0)).paddedLeft('0', 2);
    auto timeBounds = juce::Rectangle<int>(reelSectionBounds.getX() + 18,
                                           reelSectionBounds.getY() + 14,
                                           250,
                                           52);
    g.drawText(minutesText + ":" + secondsText + ":" + centisecondsText,
               timeBounds,
               juce::Justification::centredLeft,
               false);

    auto bpmTextBounds = getBpmEditorBounds().translated(getBpmEditorBounds().getWidth() + 10, 0);
    bpmTextBounds.setWidth(44);
    g.setFont(AppFonts::getFont(20.0f));
    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.drawText("BPM", bpmTextBounds, juce::Justification::centredLeft, false);

    auto signatureBounds = getBeatsPerBarEditorBounds();
    auto slashBounds = signatureBounds.translated(signatureBounds.getWidth() + 10, 0);
    slashBounds.setWidth(36);
    g.drawText("/4", slashBounds, juce::Justification::centredLeft, false);

    const auto leftReelCentre = juce::Point<float>((float) reelSectionBounds.getCentreX() - 120.0f,
                                                   (float) reelSectionBounds.getCentreY() + 6.0f);
    const auto rightReelCentre = juce::Point<float>((float) reelSectionBounds.getCentreX() + 120.0f,
                                                    (float) reelSectionBounds.getCentreY() + 6.0f);
    const auto reelRadius = 78.0f;
    const auto playheadX = (float) trackSectionBounds.getCentreX();

    g.setColour(accent.withAlpha(0.92f));
    g.strokePath(createReelPath(leftReelCentre, reelRadius, reelPhase), juce::PathStrokeType(2.4f));
    g.strokePath(createReelPath(rightReelCentre, reelRadius, reelPhase), juce::PathStrokeType(2.4f));

    const auto tapeGuideTop = (float) reelSectionBounds.getCentreY() + 76.0f;
    const auto tapeGuideBottom = (float) trackSectionBounds.getY() + 6.0f;
    g.setColour(juce::Colours::white.withAlpha(0.68f));
    g.strokePath(createTapePath(leftReelCentre,
                                rightReelCentre,
                                reelRadius,
                                playheadX,
                                tapeGuideTop,
                                tapeGuideBottom),
                 juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colours::white);
    g.drawLine(playheadX - 5.0f, tapeGuideTop, playheadX + 5.0f, tapeGuideTop, 1.5f);
    g.fillRoundedRectangle(juce::Rectangle<float>(playheadX - 7.0f, tapeGuideTop + 6.0f, 14.0f, 11.0f), 3.5f);

    auto statusCircle = juce::Rectangle<float>((float) reelSectionBounds.getCentreX() - 10.0f,
                                               (float) reelSectionBounds.getCentreY() - 4.0f,
                                               20.0f,
                                               20.0f);
    g.setColour(isMoving ? accent : juce::Colours::white.withAlpha(0.3f));
    g.fillEllipse(statusCircle);

    const auto rewindButtonBounds = getRewindButtonBounds().toFloat();
    const auto reversePreviewButtonBounds = getReversePreviewButtonBounds().toFloat();
    const auto playButtonBounds = getPlayStopButtonBounds().toFloat();
    const auto metronomeButtonBounds = getMetronomeButtonBounds().toFloat();
    const auto loopButtonBounds = getLoopButtonBounds().toFloat();
    const auto loopButtonEnabled = engine.canEditLoopMarkers();
    const auto drawRoundedButton = [&] (juce::Rectangle<float> bounds, bool active, juce::Colour activeColour)
    {
        g.setColour(active ? activeColour : juce::Colours::black);
        g.fillRoundedRectangle(bounds, 12.0f);
        g.setColour(active ? activeColour : juce::Colours::white.withAlpha(0.22f));
        g.drawRoundedRectangle(bounds, 12.0f, 1.2f);
    };

    drawRoundedButton(reversePreviewButtonBounds, reversePreviewPressed || engine.isReversePlaying(), accent);
    drawRoundedButton(rewindButtonBounds, engine.isRewinding(), accent);
    drawRoundedButton(playButtonBounds, engine.isPlaying() || engine.isCountInActive(), accent);
    const auto metronomeActive = engine.isMetronomeEnabled();
    const auto metronomeColour = accent.brighter((float) (metronomeBlinkLevel * 0.18));
    drawRoundedButton(metronomeButtonBounds, metronomeActive, metronomeColour);
    g.saveState();
    g.setOpacity(loopButtonEnabled ? 1.0f : 0.4f);
    drawRoundedButton(loopButtonBounds, engine.hasLoopMarkerNearPlayhead(), accent);
    g.restoreState();

    const auto buttonIconColour = [] (bool active)
    {
        return active ? juce::Colours::black : juce::Colours::white;
    };
    g.setColour(buttonIconColour(reversePreviewPressed || engine.isReversePlaying()));
    g.fillPath(createReversePreviewIcon(reversePreviewButtonBounds.reduced(20.0f)));
    g.setColour(buttonIconColour(engine.isRewinding()));
    g.fillPath(createRewindIcon(rewindButtonBounds.reduced(18.0f)));
    g.setColour(buttonIconColour(engine.isPlaying() || engine.isCountInActive()));
    g.fillPath(createPlayIcon(playButtonBounds.reduced(22.0f, 18.0f)));
    g.setColour(buttonIconColour(metronomeActive));
    g.strokePath(createMetronomeIcon(metronomeButtonBounds.reduced(17.0f, 15.0f)),
                 juce::PathStrokeType(1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(buttonIconColour(engine.hasLoopMarkerNearPlayhead()).withAlpha(loopButtonEnabled ? 1.0f : 0.4f));
    g.strokePath(createLoopIcon(loopButtonBounds.reduced(18.0f, 15.0f)),
                 juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        const auto laneBounds = getLaneBounds(trackIndex);
        const auto waveformBounds = getWaveformBounds(trackIndex);
        const auto meterBounds = getMeterBounds(trackIndex);
        const auto colour = getTrackColour(trackIndex);
        const auto isSelected = trackIndex == selectedTrack;
        const auto peak = juce::jlimit(0.0f, 1.0f, engine.getTrackPeakMeter(trackIndex));
        const auto clip = engine.getTrackClipping(trackIndex);

        g.setColour(isSelected ? colour : juce::Colours::white.withAlpha(0.1f));
        g.drawRoundedRectangle(laneBounds.toFloat(), 12.0f, isSelected ? 1.25f : 1.0f);

        for (int buttonIndex = 0; buttonIndex < 3; ++buttonIndex)
        {
            const auto buttonBounds = getModeButtonBounds(trackIndex, buttonIndex).toFloat();
            const auto recordMode = buttonIndex == 0 ? TrackRecordMode::overdub
                                                     : buttonIndex == 1 ? TrackRecordMode::replace
                                                                        : TrackRecordMode::erase;
            const auto active = engine.getTrackRecordMode(trackIndex) == recordMode
                                || engine.getPendingTrackRecordMode(trackIndex) == recordMode;
            const auto pending = engine.getPendingTrackRecordMode(trackIndex) == recordMode
                                 && engine.getTrackRecordMode(trackIndex) != recordMode;
            const auto buttonBaseColour = colour.withAlpha(pending ? (0.35f + (0.4f * (float) std::abs(std::sin(lastTimerSeconds * 7.0))))
                                                                   : 1.0f);

            g.setColour(active ? buttonBaseColour : juce::Colours::black);
            g.fillRoundedRectangle(buttonBounds, 9.0f);
            g.setColour(active ? buttonBaseColour : juce::Colours::white.withAlpha(0.2f));
            g.drawRoundedRectangle(buttonBounds, 9.0f, 1.0f);

            auto iconBounds = buttonBounds.reduced(8.0f);
            g.setColour(active ? juce::Colours::black : juce::Colours::white);

            if (buttonIndex == 0)
                g.fillEllipse(iconBounds);
            else if (buttonIndex == 1)
                g.fillPath(createReplaceIcon(iconBounds));
            else
                g.strokePath(createEraseIcon(iconBounds),
                             juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        for (int buttonIndex = 0; buttonIndex < 2; ++buttonIndex)
        {
            const auto buttonBounds = getUtilityButtonBounds(trackIndex, buttonIndex).toFloat();
            const auto active = buttonIndex == 0 ? engine.isTrackSolo(trackIndex)
                                                 : engine.isTrackMuted(trackIndex);

            g.setColour(active ? colour : juce::Colours::black);
            g.fillRoundedRectangle(buttonBounds, 9.0f);
            g.setColour(active ? colour : juce::Colours::white.withAlpha(0.2f));
            g.drawRoundedRectangle(buttonBounds, 9.0f, 1.0f);
            g.setColour(active ? juce::Colours::black : juce::Colours::white);
            g.setFont(AppFonts::getFont(12.0f));
            g.drawText(buttonIndex == 0 ? "S" : "M", buttonBounds, juce::Justification::centred, false);
        }

        const auto visibleSamples = getVisibleSamples();
        const auto samplesPerPixel = visibleSamples / juce::jmax(1, waveformBounds.getWidth());
        const auto centreSample = playheadSample;
        const auto playheadRatio = ((double) playheadX - (double) waveformBounds.getX()) / (double) juce::jmax(1, waveformBounds.getWidth());
        const auto leftSample = centreSample - (visibleSamples * playheadRatio);
        const auto centreY = (float) waveformBounds.getCentreY();
        const auto halfHeight = (float) waveformBounds.getHeight() * 0.5f;
        const auto lineColour = clip ? juce::Colours::red : colour;
        const auto waveformColour = lineColour.withAlpha(isSelected ? 0.95f : 0.72f);
        juce::Path waveformPath;

        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawLine((float) waveformBounds.getX(),
                   centreY,
                   (float) waveformBounds.getRight(),
                   centreY,
                   1.0f);

        for (int pixel = 0; pixel < waveformBounds.getWidth(); ++pixel)
        {
            const auto rangeStart = leftSample + ((double) pixel * samplesPerPixel);
            const auto rangeEnd = leftSample + ((double) (pixel + 1) * samplesPerPixel);
            const auto extents = getWaveformExtents(trackIndex,
                                                    (int) std::floor(rangeStart),
                                                    (int) std::ceil(rangeEnd));

            if (extents.isEmpty())
                continue;

            const auto x = (float) waveformBounds.getX() + (float) pixel + 0.5f;
            const auto y1 = centreY - (halfHeight * extents.getEnd());
            const auto y2 = centreY - (halfHeight * extents.getStart());
            waveformPath.startNewSubPath(x, y1);
            waveformPath.lineTo(x, y2);
        }

        g.setColour(waveformColour);
        g.strokePath(waveformPath,
                     juce::PathStrokeType(1.1f,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));

        const auto currentBpm = juce::jmax(30.0, (double) engine.getBpm());
        const auto samplesPerBeat = (engine.getSampleRate() * 60.0) / currentBpm;
        const auto firstBeatIndex = (int64_t) std::floor(leftSample / samplesPerBeat) - 1;
        const auto lastBeatIndex = (int64_t) std::ceil((leftSample + visibleSamples) / samplesPerBeat) + 1;

        for (int64_t beatIndex = firstBeatIndex; beatIndex <= lastBeatIndex; ++beatIndex)
        {
            if (beatIndex < 0)
                continue;

            const auto beatSample = (double) beatIndex * samplesPerBeat;
            const auto beatX = (float) waveformBounds.getX() + (float) ((beatSample - leftSample) / samplesPerPixel);

            if (beatX < (float) waveformBounds.getX() - 1.0f || beatX > (float) waveformBounds.getRight() + 1.0f)
                continue;

            const auto isBarStart = beatIndex % juce::jmax(1, engine.getBeatsPerBar()) == 0;
            g.setColour(juce::Colours::white.withAlpha(isBarStart ? 0.26f : 0.14f));
            g.drawLine(beatX,
                       (float) waveformBounds.getY(),
                       beatX,
                       (float) waveformBounds.getBottom(),
                       isBarStart ? 1.2f : 0.8f);

            if (engine.hasLoopMarkerAtBeat(beatIndex))
            {
                const auto markerRadius = isBarStart ? 4.5f : 4.0f;
                g.setColour(juce::Colours::white);
                g.fillEllipse(beatX - markerRadius,
                              (float) waveformBounds.getY() - 10.0f - markerRadius,
                              markerRadius * 2.0f,
                              markerRadius * 2.0f);
            }
        }

        const auto meterFillHeight = juce::roundToInt((float) meterBounds.getHeight() * peak);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.fillRoundedRectangle(meterBounds.toFloat(), 4.0f);
        g.setColour(lineColour);
        g.fillRoundedRectangle(juce::Rectangle<float>((float) meterBounds.getX(),
                                                     (float) meterBounds.getBottom() - (float) meterFillHeight,
                                                     (float) meterBounds.getWidth(),
                                                     (float) meterFillHeight),
                              4.0f);
    }

    g.setColour(accent);
    g.drawLine(playheadX,
               (float) trackSectionBounds.getY() + 8.0f,
               playheadX,
               (float) trackSectionBounds.getBottom() - 8.0f,
               1.8f);
}
