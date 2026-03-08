#include "TapeView.h"

#include "AppFonts.h"

#include <cmath>

namespace
{
juce::Colour getTrackColour(int index)
{
    static const std::array<juce::Colour, TapeEngine::numTracks> colours
    {
        juce::Colour::fromRGB(255, 92, 92),
        juce::Colour::fromRGB(255, 184, 77),
        juce::Colour::fromRGB(94, 233, 196),
        juce::Colour::fromRGB(94, 146, 255)
    };

    return colours[(size_t) index];
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
}

TapeView::TapeView(TapeEngine& engineToUse)
    : engine(engineToUse)
{
    displayedPlayhead = engine.getDisplayPlayheadSample();
    lastDisplayedPlayhead = displayedPlayhead;
    lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    bpmEditor.setEditable(true, true, false);
    bpmEditor.setJustificationType(juce::Justification::centred);
    bpmEditor.setFont(AppFonts::getFont(22.0f));
    bpmEditor.setText(juce::String((int) std::round(engine.getBpm())), juce::dontSendNotification);
    bpmEditor.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmEditor.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    bpmEditor.setColour(juce::Label::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    bpmEditor.setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
    bpmEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    bpmEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    bpmEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.22f));
    bpmEditor.onTextChange = [this]
    {
        const auto numericText = bpmEditor.getText().retainCharacters("0123456789");

        if (numericText.isNotEmpty())
            engine.setBpm(numericText.getFloatValue());
    };
    bpmEditor.onEditorShow = [this]
    {
        if (auto* editor = bpmEditor.getCurrentTextEditor())
            editor->setInputRestrictions(3, "0123456789");
    };
    addAndMakeVisible(bpmEditor);
    beatsPerBarEditor.setEditable(true, true, false);
    beatsPerBarEditor.setJustificationType(juce::Justification::centred);
    beatsPerBarEditor.setFont(AppFonts::getFont(22.0f));
    beatsPerBarEditor.setText(juce::String(engine.getBeatsPerBar()), juce::dontSendNotification);
    beatsPerBarEditor.setColour(juce::Label::textColourId, juce::Colours::white);
    beatsPerBarEditor.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    beatsPerBarEditor.setColour(juce::Label::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    beatsPerBarEditor.setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
    beatsPerBarEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    beatsPerBarEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    beatsPerBarEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.22f));
    beatsPerBarEditor.onTextChange = [this]
    {
        const auto numericText = beatsPerBarEditor.getText().retainCharacters("0123456789");

        if (numericText.isNotEmpty())
            engine.setBeatsPerBar(numericText.getIntValue());
    };
    beatsPerBarEditor.onEditorShow = [this]
    {
        if (auto* editor = beatsPerBarEditor.getCurrentTextEditor())
            editor->setInputRestrictions(2, "0123456789");
    };
    addAndMakeVisible(beatsPerBarEditor);
    zoomSlider.setSliderStyle(juce::Slider::LinearVertical);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange(0.0, 1.0, 0.001);
    zoomSlider.setValue(0.55, juce::dontSendNotification);
    zoomSlider.setDoubleClickReturnValue(true, 0.55);
    zoomSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::white.withAlpha(0.1f));
    zoomSlider.setColour(juce::Slider::trackColourId, getTrackColour(selectedTrack));
    zoomSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    zoomSlider.onValueChange = [this]
    {
        repaint();
    };
    addAndMakeVisible(zoomSlider);

    startTimerHz(30);
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

    g.setColour(accent.withAlpha(0.92f));
    g.strokePath(createReelPath(leftReelCentre, reelRadius, reelPhase), juce::PathStrokeType(2.4f));
    g.strokePath(createReelPath(rightReelCentre, reelRadius, reelPhase), juce::PathStrokeType(2.4f));

    auto statusCircle = juce::Rectangle<float>((float) reelSectionBounds.getCentreX() - 10.0f,
                                               (float) reelSectionBounds.getCentreY() - 4.0f,
                                               20.0f,
                                               20.0f);
    g.setColour(isMoving ? accent : juce::Colours::white.withAlpha(0.3f));
    g.fillEllipse(statusCircle);

    const auto rewindButtonBounds = getRewindButtonBounds().toFloat();
    const auto reversePreviewButtonBounds = getReversePreviewButtonBounds().toFloat();
    const auto playStopButtonBounds = getPlayStopButtonBounds().toFloat();
    const auto metronomeButtonBounds = getMetronomeButtonBounds().toFloat();
    const auto loopButtonBounds = getLoopButtonBounds().toFloat();
    const auto rewindActive = engine.isRewinding();
    const auto reversePreviewActive = engine.isReversePlaying();
    const auto countInActive = engine.isCountInActive();
    const auto transportActive = engine.isPlaying() || countInActive;
    const auto metronomeActive = engine.isMetronomeEnabled();
    const auto loopEditable = engine.canEditLoopMarkers();
    const auto loopActive = engine.hasLoopMarkerNearPlayhead();
    const auto metronomeFill = countInActive && metronomeActive ? juce::Colours::black.interpolatedWith(accent, (float) metronomeBlinkLevel)
                                                                : (metronomeActive ? accent : juce::Colours::black);
    const auto metronomeIconColour = countInActive && metronomeActive ? juce::Colours::white.interpolatedWith(juce::Colours::black, (float) metronomeBlinkLevel)
                                                                      : (metronomeActive ? juce::Colours::black : juce::Colours::white);

    g.setColour(reversePreviewActive ? accent : juce::Colours::black);
    g.fillRoundedRectangle(reversePreviewButtonBounds, 9.0f);
    g.setColour(rewindActive ? accent : juce::Colours::black);
    g.fillRoundedRectangle(rewindButtonBounds, 9.0f);
    g.setColour(transportActive ? accent : juce::Colours::black);
    g.fillRoundedRectangle(playStopButtonBounds, 9.0f);
    g.setColour(metronomeFill);
    g.fillRoundedRectangle(metronomeButtonBounds, 9.0f);
    g.setColour(loopActive && loopEditable ? accent : juce::Colours::black);
    g.fillRoundedRectangle(loopButtonBounds, 9.0f);

    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(reversePreviewButtonBounds, 9.0f, 1.0f);
    g.drawRoundedRectangle(rewindButtonBounds, 9.0f, 1.0f);
    g.drawRoundedRectangle(playStopButtonBounds, 9.0f, 1.0f);
    g.drawRoundedRectangle(metronomeButtonBounds, 9.0f, 1.0f);
    g.drawRoundedRectangle(loopButtonBounds, 9.0f, 1.0f);

    g.setColour(reversePreviewActive ? juce::Colours::black : juce::Colours::white);
    g.fillPath(createReversePreviewIcon(reversePreviewButtonBounds.reduced(19.0f, 15.0f)));
    g.setColour(rewindActive ? juce::Colours::black : juce::Colours::white);
    g.fillPath(createRewindIcon(rewindButtonBounds.reduced(16.0f, 14.0f)));
    g.setColour(transportActive ? juce::Colours::black : juce::Colours::white);
    g.fillPath(createPlayIcon(playStopButtonBounds.reduced(18.0f, 15.0f)));
    g.setColour(metronomeIconColour);
    g.strokePath(createMetronomeIcon(metronomeButtonBounds.reduced(18.0f, 16.0f)),
                 juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(loopEditable ? (loopActive ? juce::Colours::black : juce::Colours::white)
                             : juce::Colours::white.withAlpha(0.28f));
    g.strokePath(createLoopIcon(loopButtonBounds.reduced(16.0f, 16.0f)),
                 juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path tapePath;
    tapePath.startNewSubPath(leftReelCentre.x + reelRadius * 0.86f, leftReelCentre.y + reelRadius * 0.62f);
    tapePath.lineTo((float) reelSectionBounds.getCentreX() - 18.0f, (float) trackSectionBounds.getY() - 16.0f);
    tapePath.lineTo((float) reelSectionBounds.getCentreX() + 18.0f, (float) trackSectionBounds.getY() - 16.0f);
    tapePath.lineTo(rightReelCentre.x - reelRadius * 0.86f, rightReelCentre.y + reelRadius * 0.62f);

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.strokePath(tapePath, juce::PathStrokeType(2.0f));

    const auto fixedPlayheadX = (float) trackSectionBounds.getCentreX();
    g.setColour(juce::Colours::white);
    g.drawLine(fixedPlayheadX,
               (float) reelSectionBounds.getBottom() - 10.0f,
               fixedPlayheadX,
               (float) trackSectionBounds.getBottom(),
               2.0f);

    auto playheadMarker = juce::Rectangle<float>(fixedPlayheadX - 7.0f,
                                                 (float) trackSectionBounds.getY() - 12.0f,
                                                 14.0f,
                                                 10.0f);
    g.fillRoundedRectangle(playheadMarker, 3.0f);

    const auto visibleSamples = getVisibleSamples();
    const auto samplesPerPixel = visibleSamples / juce::jmax(1, trackSectionBounds.getWidth());
    const auto currentBpm = juce::jmax(30.0, (double) engine.getBpm());
    const auto samplesPerBeat = (engine.getSampleRate() * 60.0) / currentBpm;
    const auto currentBeatsPerBar = juce::jmax(1, engine.getBeatsPerBar());
    const auto blinkOn = ((juce::Time::getMillisecondCounter() / 220) % 2) == 0;

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        const auto waveformBounds = getWaveformBounds(trackIndex);
        const auto meterBounds = getMeterBounds(trackIndex);
        const auto colour = getTrackColour(trackIndex);
        const auto peakMeter = juce::jlimit(0.0f, 1.2f, engine.getTrackPeakMeter(trackIndex));
        const auto isClipping = engine.getTrackClipping(trackIndex);
        const auto isSelected = trackIndex == selectedTrack;
        const auto isSolo = engine.isTrackSolo(trackIndex);
        const auto isMuted = engine.isTrackMuted(trackIndex);

        g.setColour(isSelected ? colour : juce::Colours::white.withAlpha(0.15f));
        g.drawRoundedRectangle(waveformBounds.toFloat(), 6.0f, isSelected ? 1.6f : 1.0f);

        g.setColour(isSelected ? colour.withAlpha(0.95f) : juce::Colours::white.withAlpha(0.24f));
        g.drawLine((float) waveformBounds.getX(),
                   (float) waveformBounds.getCentreY(),
                   (float) waveformBounds.getRight(),
                   (float) waveformBounds.getCentreY(),
                   1.0f);

        g.setColour(isSelected ? colour.withAlpha(0.92f) : colour.withAlpha(0.55f));
        const auto leftSample = playheadSample - (((double) fixedPlayheadX - (double) waveformBounds.getX()) * samplesPerPixel);
        const auto rightSample = leftSample + ((double) waveformBounds.getWidth() * samplesPerPixel);

        for (int x = 0; x < waveformBounds.getWidth(); ++x)
        {
            const auto drawX = waveformBounds.getX() + x;
            const auto bucketStart = (int) std::floor(leftSample + ((double) x * samplesPerPixel));
            const auto bucketEnd = (int) std::ceil(leftSample + ((double) (x + 1) * samplesPerPixel));
            const auto extents = getWaveformExtents(trackIndex, bucketStart, bucketEnd);

            if (std::abs(extents.getStart()) <= 0.0001f && std::abs(extents.getEnd()) <= 0.0001f)
                continue;

            const auto halfHeight = (float) waveformBounds.getHeight() * 0.42f;
            const auto top = (float) waveformBounds.getCentreY() - (extents.getEnd() * halfHeight);
            const auto bottom = (float) waveformBounds.getCentreY() - (extents.getStart() * halfHeight);
            g.drawVerticalLine(drawX,
                               juce::jlimit((float) waveformBounds.getY() + 1.0f, (float) waveformBounds.getBottom() - 1.0f, top),
                               juce::jlimit((float) waveformBounds.getY() + 1.0f, (float) waveformBounds.getBottom() - 1.0f, bottom));
        }

        const auto firstBeatIndex = juce::jmax<int64_t>(0, (int64_t) std::floor(leftSample / samplesPerBeat));
        const auto lastBeatIndex = juce::jmax<int64_t>(firstBeatIndex, (int64_t) std::ceil(rightSample / samplesPerBeat));

        for (int64_t beatIndex = firstBeatIndex; beatIndex <= lastBeatIndex; ++beatIndex)
        {
            const auto beatSample = (double) beatIndex * samplesPerBeat;
            const auto drawX = (int) std::round((double) waveformBounds.getX()
                                                + ((beatSample - leftSample) / samplesPerPixel));

            if (drawX < waveformBounds.getX() || drawX > waveformBounds.getRight())
                continue;

            const auto isBarLine = beatIndex % currentBeatsPerBar == 0;
            g.setColour(juce::Colours::white.withAlpha(isBarLine ? 0.5f : 0.15f));
            g.drawVerticalLine(drawX,
                               (float) waveformBounds.getY() + 4.0f,
                               (float) waveformBounds.getBottom() - 4.0f);

            if (engine.hasLoopMarkerAtBeat(beatIndex))
            {
                auto markerBounds = juce::Rectangle<float>((float) drawX - 4.0f,
                                                           (float) waveformBounds.getY() + 7.0f,
                                                           8.0f,
                                                           8.0f);
                g.setColour(juce::Colours::black);
                g.fillEllipse(markerBounds);
                g.setColour(isSelected ? colour : juce::Colours::white.withAlpha(0.75f));
                g.drawEllipse(markerBounds, 1.2f);
            }
        }

        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(meterBounds.toFloat(), 4.0f, 1.0f);

        auto filledMeterBounds = meterBounds.reduced(2);
        filledMeterBounds.removeFromBottom(juce::roundToInt((1.0f - juce::jlimit(0.0f, 1.0f, peakMeter)) * filledMeterBounds.getHeight()));
        g.setColour(isClipping ? juce::Colours::red : colour);
        g.fillRoundedRectangle(filledMeterBounds.toFloat(), 3.0f);

        const auto recordMode = engine.getTrackRecordMode(trackIndex);
        const auto pendingRecordMode = engine.getPendingTrackRecordMode(trackIndex);

        for (int buttonIndex = 0; buttonIndex < 3; ++buttonIndex)
        {
            const auto buttonBounds = getModeButtonBounds(trackIndex, buttonIndex).toFloat();
            const auto modeForButton = buttonIndex == 0 ? TrackRecordMode::overdub
                                                        : buttonIndex == 1 ? TrackRecordMode::replace
                                                                           : TrackRecordMode::erase;
            const auto isActive = recordMode == modeForButton;
            const auto isPending = pendingRecordMode == modeForButton;
            const auto showActiveFill = isPending ? blinkOn : isActive;

            g.setColour(showActiveFill ? colour : juce::Colours::black);
            g.fillRoundedRectangle(buttonBounds, 6.0f);
            g.setColour((isActive || isPending) ? colour : juce::Colours::white.withAlpha(0.35f));
            g.drawRoundedRectangle(buttonBounds, 6.0f, 1.2f);

            auto iconBounds = buttonBounds.reduced(8.0f);

            if (modeForButton == TrackRecordMode::overdub)
            {
                g.setColour(showActiveFill ? juce::Colours::black : juce::Colours::white);
                g.fillEllipse(iconBounds.reduced(2.0f));
            }
            else if (modeForButton == TrackRecordMode::replace)
            {
                g.setColour(showActiveFill ? juce::Colours::black : juce::Colours::white);
                g.strokePath(createReplaceIcon(iconBounds.reduced(2.0f)), juce::PathStrokeType(2.0f));
            }
            else
            {
                g.setColour(showActiveFill ? juce::Colours::black : juce::Colours::white);
                g.strokePath(createEraseIcon(iconBounds.reduced(2.0f)), juce::PathStrokeType(2.0f));
            }
        }

        for (int buttonIndex = 0; buttonIndex < 2; ++buttonIndex)
        {
            const auto buttonBounds = getUtilityButtonBounds(trackIndex, buttonIndex).toFloat();
            const auto isActive = buttonIndex == 0 ? isSolo : isMuted;

            g.setColour(isActive ? colour : juce::Colours::black);
            g.fillRoundedRectangle(buttonBounds, 6.0f);
            g.setColour(isActive ? colour : juce::Colours::white.withAlpha(0.35f));
            g.drawRoundedRectangle(buttonBounds, 6.0f, 1.2f);
            g.setColour(isActive ? juce::Colours::black : juce::Colours::white);
            g.setFont(AppFonts::getFont(14.0f));
            g.drawText(buttonIndex == 0 ? "S" : "M",
                       buttonBounds.toNearestInt(),
                       juce::Justification::centred,
                       false);
        }

    }
}

void TapeView::resized()
{
    bpmEditor.setBounds(getBpmEditorBounds());
    beatsPerBarEditor.setBounds(getBeatsPerBarEditorBounds());
    zoomSlider.setBounds(getZoomSliderBounds());
}

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

void TapeView::setSelectedTrack(int trackIndex)
{
    const auto newTrack = juce::jlimit(0, TapeEngine::numTracks - 1, trackIndex);

    if (selectedTrack == newTrack)
        return;

    selectedTrack = newTrack;
    zoomSlider.setColour(juce::Slider::trackColourId, getTrackColour(selectedTrack));

    if (onSelectedTrackChanged != nullptr)
        onSelectedTrackChanged(selectedTrack);

    repaint();
}

int TapeView::getSelectedTrack() const noexcept
{
    return selectedTrack;
}

juce::Rectangle<int> TapeView::getLaneBounds(int trackIndex) const
{
    auto bounds = getTrackSectionBounds().reduced(0, 8);
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
    return juce::Rectangle<int>(bounds.getRight() - 20, bounds.getY(), 20, bounds.getHeight());
}

juce::Rectangle<int> TapeView::getWaveformBounds(int trackIndex) const
{
    auto laneBounds = getLaneBounds(trackIndex).reduced(12, 10);
    const auto controlBounds = getControlClusterBounds(trackIndex);
    laneBounds.setX(controlBounds.getRight() + 12);
    const auto meterX = getZoomSliderBounds().getX() - 22;
    laneBounds.setRight(meterX - 10);
    return laneBounds;
}

juce::Rectangle<int> TapeView::getMeterBounds(int trackIndex) const
{
    auto waveformBounds = getWaveformBounds(trackIndex);
    return juce::Rectangle<int>(getZoomSliderBounds().getX() - 22,
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

void TapeView::scrubTo(juce::Point<float> position)
{
    if (! isScrubbing || ! getTrackSectionBounds().contains(position.toInt()))
        return;

    const auto pixelsPerSample = (double) getTrackSectionBounds().getWidth() / getVisibleSamples();
    const auto scrubPixelsPerSample = pixelsPerSample * 4.0;
    const auto sampleOffset = ((double) scrubStartX - (double) position.x) / scrubPixelsPerSample;
    engine.setPlayheadSample(scrubStartPlayhead + sampleOffset);
}

void TapeView::timerCallback()
{
    engine.servicePendingAllocations();

    if (! bpmEditor.isBeingEdited())
        bpmEditor.setText(juce::String((int) std::round(engine.getBpm())), juce::dontSendNotification);

    if (! beatsPerBarEditor.isBeingEdited())
        beatsPerBarEditor.setText(juce::String(engine.getBeatsPerBar()), juce::dontSendNotification);

    const auto currentPlayhead = engine.getDisplayPlayheadSample();
    const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto elapsedSeconds = juce::jlimit(0.0, 0.1, nowSeconds - lastTimerSeconds);
    const auto sampleRate = juce::jmax(1.0, engine.getSampleRate());

    if (isScrubbing)
    {
        displayedPlayhead = currentPlayhead;
    }
    else if (engine.isPlaying())
    {
        const auto predictedPlayhead = currentPlayhead + (elapsedSeconds * sampleRate);
        const auto loopJumpThreshold = sampleRate * 0.05;

        if (predictedPlayhead < displayedPlayhead - loopJumpThreshold)
            displayedPlayhead = predictedPlayhead;
        else
            displayedPlayhead = juce::jmax(displayedPlayhead, predictedPlayhead);
    }
    else if (engine.isRewinding())
    {
        displayedPlayhead = juce::jmax(0.0,
                                       juce::jmin(displayedPlayhead,
                                                  currentPlayhead - (elapsedSeconds * sampleRate * TapeEngine::rewindSpeedMultiplier)));
    }
    else if (engine.isReversePlaying())
    {
        displayedPlayhead = juce::jmax(0.0,
                                       juce::jmin(displayedPlayhead,
                                                  currentPlayhead - (elapsedSeconds * sampleRate)));
    }
    else
    {
        displayedPlayhead = currentPlayhead;
    }

    const auto metronomePulseRevision = engine.getMetronomePulseRevision();

    if (metronomePulseRevision != lastMetronomePulseRevision)
    {
        lastMetronomePulseRevision = metronomePulseRevision;
        metronomeBlinkLevel = 1.0;
    }
    else
    {
        metronomeBlinkLevel = juce::jmax(0.0, metronomeBlinkLevel - (elapsedSeconds / 0.14));
    }

    lastTimerSeconds = nowSeconds;
    const auto signedPlayheadDelta = displayedPlayhead - lastDisplayedPlayhead;
    reelPhase += (float) juce::jlimit(-0.45, 0.45, signedPlayheadDelta * 0.00018);

    if (reelPhase > juce::MathConstants<float>::twoPi)
        reelPhase -= juce::MathConstants<float>::twoPi;
    else if (reelPhase < 0.0f)
        reelPhase += juce::MathConstants<float>::twoPi;

    lastDisplayedPlayhead = displayedPlayhead;
    repaint();
}
