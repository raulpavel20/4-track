#include "TrackControlChain.h"

#include "AppFonts.h"

namespace
{
juce::Colour getChainAccentColour(int trackIndex)
{
    static const std::array<juce::Colour, TapeEngine::numTracks> colours
    {
        juce::Colour::fromRGB(255, 92, 92),
        juce::Colour::fromRGB(255, 184, 77),
        juce::Colour::fromRGB(94, 233, 196),
        juce::Colour::fromRGB(94, 146, 255)
    };

    return colours[(size_t) juce::jlimit(0, TapeEngine::numTracks - 1, trackIndex)];
}

juce::String formatGainValue(float value)
{
    return juce::String(value, 2) + "x";
}

juce::String formatFilterValue(float value)
{
    if (std::abs(value) < 0.02f)
        return "BP";

    return value < 0.0f ? "LP" : "HP";
}

float smoothStep(float edge0, float edge1, float value)
{
    const auto t = juce::jlimit(0.0f, 1.0f, (value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - (2.0f * t));
}

float getFilterVisualizerResponse(float frequency, float morph)
{
    const auto lowpassResponse = 0.12f + (0.76f * (1.0f - smoothStep(0.48f, 0.78f, frequency)));
    const auto highpassResponse = 0.12f + (0.76f * smoothStep(0.22f, 0.52f, frequency));
    const auto bandRise = smoothStep(0.18f, 0.38f, frequency);
    const auto bandFall = 1.0f - smoothStep(0.62f, 0.82f, frequency);
    const auto bandpassResponse = 0.12f + (0.76f * (bandRise * bandFall));

    if (morph < 0.0f)
        return juce::jmap(morph + 1.0f, lowpassResponse, bandpassResponse);

    return juce::jmap(morph, bandpassResponse, highpassResponse);
}
}

TrackControlChain::TrackControlChain(TapeEngine& engineToUse)
    : engine(engineToUse)
{
    inputSourceBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    inputSourceBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    inputSourceBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    inputSourceBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    inputSourceBox.onChange = [this]
    {
        const auto selectedId = inputSourceBox.getSelectedId();

        if (selectedId > 0)
            engine.setTrackInputSource(selectedTrack, selectedId - 1);
    };

    gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setRange(0.0, 2.0, 0.01);
    gainSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                   juce::MathConstants<float>::pi * 2.8f,
                                   true);
    gainSlider.setDoubleClickReturnValue(true, 1.0);
    gainSlider.setColour(juce::Slider::rotarySliderFillColourId, getChainAccentColour(selectedTrack));
    gainSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha(0.18f));
    gainSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    gainSlider.setValue(engine.getTrackInputGain(selectedTrack), juce::dontSendNotification);
    gainSlider.onValueChange = [this]
    {
        engine.setTrackInputGain(selectedTrack, (float) gainSlider.getValue());
        repaint();
    };

    filterSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    filterSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    filterSlider.setRange(-1.0, 1.0, 0.01);
    filterSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                     juce::MathConstants<float>::pi * 2.8f,
                                     true);
    filterSlider.setDoubleClickReturnValue(true, 0.0);
    filterSlider.setColour(juce::Slider::rotarySliderFillColourId, getChainAccentColour(selectedTrack));
    filterSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha(0.18f));
    filterSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    filterSlider.setValue(engine.getTrackFilterMorph(selectedTrack), juce::dontSendNotification);
    filterSlider.onValueChange = [this]
    {
        engine.setTrackFilterMorph(selectedTrack, (float) filterSlider.getValue());
        repaint();
    };

    addAndMakeVisible(inputSourceBox);
    addAndMakeVisible(gainSlider);
    addAndMakeVisible(filterSlider);
}

void TrackControlChain::paint(juce::Graphics& g)
{
    const auto frameBounds = getFrameBounds();
    const auto bounds = frameBounds.toFloat();
    const auto accent = getChainAccentColour(selectedTrack);
    const auto inputModule = getInputModuleBounds();
    const auto filterModule = getFilterModuleBounds();

    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(bounds, 18.0f, 1.0f);

    g.setColour(accent);
    g.drawRoundedRectangle(inputModule.toFloat(), 14.0f, 2.0f);
    g.drawRoundedRectangle(filterModule.toFloat(), 14.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(16.0f));
    g.drawText("Input",
               inputModule.withTrimmedTop(10).removeFromTop(20).reduced(14, 0),
               juce::Justification::centredLeft,
               false);
    g.drawText("Filter",
               filterModule.withTrimmedTop(10).removeFromTop(20).reduced(14, 0),
               juce::Justification::centredLeft,
               false);

    const auto gainBounds = gainSlider.getBounds();
    const auto filterBounds = filterSlider.getBounds();
    g.setFont(AppFonts::getFont(13.0f));
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.drawText(formatGainValue((float) gainSlider.getValue()),
               juce::Rectangle<int>(inputModule.getX() + 16, gainBounds.getBottom() - 6, inputModule.getWidth() - 32, 16),
               juce::Justification::centred,
               false);
    g.drawText(formatFilterValue((float) filterSlider.getValue()),
               juce::Rectangle<int>(filterModule.getX() + 16, filterBounds.getBottom() - 6, filterModule.getWidth() - 32, 16),
               juce::Justification::centred,
               false);

    auto visualizerBounds = filterModule.reduced(14);
    visualizerBounds.removeFromTop(24);
    visualizerBounds.setHeight((int) std::round((float) filterModule.getHeight() * 0.38f));
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(visualizerBounds.toFloat(), 10.0f, 1.0f);

    juce::Path responsePath;
    const auto morph = engine.getTrackFilterMorph(selectedTrack);

    for (int x = 0; x < visualizerBounds.getWidth(); ++x)
    {
        const auto t = visualizerBounds.getWidth() > 1 ? (float) x / (float) (visualizerBounds.getWidth() - 1) : 0.0f;
        const auto response = getFilterVisualizerResponse(t, morph);

        const auto point = juce::Point<float>((float) visualizerBounds.getX() + (float) x,
                                              (float) visualizerBounds.getBottom() - ((float) visualizerBounds.getHeight() * response));

        if (x == 0)
            responsePath.startNewSubPath(point);
        else
            responsePath.lineTo(point);
    }

    g.setColour(accent);
    g.strokePath(responsePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void TrackControlChain::resized()
{
    auto inputModule = getInputModuleBounds().reduced(12);
    auto filterModule = getFilterModuleBounds().reduced(12);
    constexpr int knobSize = 64;

    inputModule.removeFromTop(20);
    inputSourceBox.setBounds(inputModule.removeFromTop(28));
    inputModule.removeFromTop(6);
    const auto gainKnobSize = (int) std::round(knobSize * 1.2f);
    gainSlider.setBounds(juce::Rectangle<int>(inputModule.getCentreX() - (gainKnobSize / 2),
                                              inputModule.getY() + 10,
                                              gainKnobSize,
                                              gainKnobSize));

    filterModule.removeFromTop(20);
    const auto visualizerHeight = (int) std::round((float) filterModule.getHeight() * 0.34f);
    filterModule.removeFromTop(visualizerHeight + 10);
    filterSlider.setBounds(juce::Rectangle<int>(filterModule.getCentreX() - (knobSize / 2),
                                                filterModule.getBottom() - knobSize - 10,
                                                knobSize,
                                                knobSize));
}

void TrackControlChain::setInputOptions(const juce::StringArray& options)
{
    inputSourceBox.clear(juce::dontSendNotification);

    for (int index = 0; index < options.size(); ++index)
        inputSourceBox.addItem(options[index], index + 1);

    inputSourceBox.setEnabled(options.isEmpty() == false);

    if (options.isEmpty())
        return;

    const auto selectedSource = juce::jlimit(0, options.size() - 1, engine.getTrackInputSource(selectedTrack));
    inputSourceBox.setSelectedId(selectedSource + 1, juce::dontSendNotification);
}

void TrackControlChain::setSelectedTrack(int trackIndex)
{
    selectedTrack = juce::jlimit(0, TapeEngine::numTracks - 1, trackIndex);
    const auto accent = getChainAccentColour(selectedTrack);
    gainSlider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    filterSlider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    gainSlider.setValue(engine.getTrackInputGain(selectedTrack), juce::dontSendNotification);
    filterSlider.setValue(engine.getTrackFilterMorph(selectedTrack), juce::dontSendNotification);
    const auto selectedSource = juce::jmax(0, engine.getTrackInputSource(selectedTrack));

    if (inputSourceBox.getNumItems() > 0)
        inputSourceBox.setSelectedId(juce::jlimit(1, inputSourceBox.getNumItems(), selectedSource + 1), juce::dontSendNotification);

    repaint();
}

int TrackControlChain::getSelectedTrack() const noexcept
{
    return selectedTrack;
}

juce::Rectangle<int> TrackControlChain::getFrameBounds() const
{
    return getLocalBounds().reduced(6);
}

juce::Rectangle<int> TrackControlChain::getContentBounds() const
{
    return getFrameBounds().reduced(8);
}

juce::Rectangle<int> TrackControlChain::getInputModuleBounds() const
{
    auto content = getContentBounds();
    const auto availableWidth = content.getWidth();
    const auto inputWidth = juce::jmin(180, juce::jmax(146, availableWidth / 3));
    return juce::Rectangle<int>(content.getX(), content.getY(), inputWidth, content.getHeight());
}

juce::Rectangle<int> TrackControlChain::getFilterModuleBounds() const
{
    auto content = getContentBounds();
    const auto gap = 10;
    const auto availableWidth = content.getWidth();
    const auto inputWidth = juce::jmin(180, juce::jmax(146, availableWidth / 3));
    const auto remainingWidth = juce::jmax(172, availableWidth - inputWidth - gap);
    const auto filterWidth = juce::jmin(212, juce::jmax(182, juce::jmin(remainingWidth, availableWidth / 2)));
    return juce::Rectangle<int>(content.getX() + inputWidth + gap, content.getY(), filterWidth, content.getHeight());
}
