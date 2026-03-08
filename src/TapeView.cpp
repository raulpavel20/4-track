#include "TapeView.h"

#include "AppSettings.h"
#include "AppFonts.h"

#include <cmath>

namespace
{
juce::Colour getTrackColour(int index)
{
    return AppSettings::getInstance().getTrackColour(juce::jlimit(0, TapeEngine::numTracks - 1, index));
}
}

TapeView::TapeView(TapeEngine& engineToUse)
    : engine(engineToUse)
{
    AppSettings::getInstance().addChangeListener(this);
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
    zoomSlider.setValue(0.50, juce::dontSendNotification);
    zoomSlider.setDoubleClickReturnValue(true, 0.50);
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

TapeView::~TapeView()
{
    AppSettings::getInstance().removeChangeListener(this);
}

void TapeView::resized()
{
    bpmEditor.setBounds(getBpmEditorBounds());
    beatsPerBarEditor.setBounds(getBeatsPerBarEditorBounds());
    zoomSlider.setBounds(getZoomSliderBounds());
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

void TapeView::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &AppSettings::getInstance())
        return;

    zoomSlider.setColour(juce::Slider::trackColourId, getTrackColour(selectedTrack));
    repaint();
}
