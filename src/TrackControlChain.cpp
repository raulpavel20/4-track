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
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 20);
    gainSlider.setRange(0.0, 2.0, 0.01);
    gainSlider.setColour(juce::Slider::rotarySliderFillColourId, getChainAccentColour(selectedTrack));
    gainSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    gainSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    gainSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::white.withAlpha(0.18f));
    gainSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black);
    gainSlider.setValue(engine.getTrackInputGain(selectedTrack), juce::dontSendNotification);
    gainSlider.onValueChange = [this]
    {
        engine.setTrackInputGain(selectedTrack, (float) gainSlider.getValue());
    };

    lowpassButton.setButtonText("Lowpass");
    lowpassButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    lowpassButton.setColour(juce::TextButton::buttonOnColourId, getChainAccentColour(selectedTrack));
    lowpassButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    lowpassButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    lowpassButton.setToggleState(engine.isTrackLowpassEnabled(selectedTrack), juce::dontSendNotification);
    lowpassButton.onClick = [this]
    {
        engine.setTrackLowpassEnabled(selectedTrack, lowpassButton.getToggleState());
    };

    addAndMakeVisible(inputSourceBox);
    addAndMakeVisible(gainSlider);
    addAndMakeVisible(lowpassButton);
}

void TrackControlChain::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto accent = getChainAccentColour(selectedTrack);

    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 14.0f, 1.0f);

    auto content = getLocalBounds().reduced(18);
    auto header = content.removeFromTop(30);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(16.0f));
    g.drawText("CHAIN", header.removeFromLeft(90), juce::Justification::centredLeft, false);
    g.drawText("TRACK " + juce::String(selectedTrack + 1), header.removeFromLeft(110), juce::Justification::centredLeft, false);

    auto modules = content.removeFromTop(150);
    auto inputModule = modules.removeFromLeft(230);
    modules.removeFromLeft(18);
    auto filterModule = modules.removeFromLeft(230);

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.fillRoundedRectangle(inputModule.toFloat(), 14.0f);
    g.fillRoundedRectangle(filterModule.toFloat(), 14.0f);

    g.setColour(accent);
    g.drawRoundedRectangle(inputModule.toFloat(), 14.0f, 2.0f);
    g.drawRoundedRectangle(filterModule.toFloat(), 14.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(15.0f));
    g.drawText("Input", inputModule.removeFromTop(28).reduced(14, 0), juce::Justification::centredLeft, false);
    g.drawText("Filter", filterModule.removeFromTop(28).reduced(14, 0), juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.setFont(AppFonts::getFont(12.0f));
    g.drawText("Source", inputModule.removeFromTop(18).reduced(14, 0), juce::Justification::centredLeft, false);
    g.drawText("Gain", inputModule.removeFromTop(18).reduced(14, 0), juce::Justification::centredLeft, false);
    g.drawText("Shape", filterModule.removeFromTop(18).reduced(14, 0), juce::Justification::centredLeft, false);
}

void TrackControlChain::resized()
{
    auto content = getLocalBounds().reduced(18);
    content.removeFromTop(30);

    auto modules = content.removeFromTop(150);
    auto inputModule = modules.removeFromLeft(230).reduced(14);
    modules.removeFromLeft(18);
    auto filterModule = modules.removeFromLeft(230).reduced(14);

    inputModule.removeFromTop(46);
    inputSourceBox.setBounds(inputModule.removeFromTop(28));
    inputModule.removeFromTop(8);
    gainSlider.setBounds(inputModule.removeFromLeft(150));

    filterModule.removeFromTop(52);
    lowpassButton.setBounds(filterModule.removeFromTop(28));
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
    lowpassButton.setColour(juce::TextButton::buttonOnColourId, accent);
    gainSlider.setValue(engine.getTrackInputGain(selectedTrack), juce::dontSendNotification);
    lowpassButton.setToggleState(engine.isTrackLowpassEnabled(selectedTrack), juce::dontSendNotification);
    const auto selectedSource = juce::jmax(0, engine.getTrackInputSource(selectedTrack));

    if (inputSourceBox.getNumItems() > 0)
        inputSourceBox.setSelectedId(juce::jlimit(1, inputSourceBox.getNumItems(), selectedSource + 1), juce::dontSendNotification);

    repaint();
}

int TrackControlChain::getSelectedTrack() const noexcept
{
    return selectedTrack;
}
