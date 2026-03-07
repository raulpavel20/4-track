#include "TransportControls.h"

#include "AppFonts.h"

TransportControls::TransportControls()
{
    addAndMakeVisible(rewindButton);
    addAndMakeVisible(playStopButton);

    rewindButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    rewindButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::white.withAlpha(0.12f));
    rewindButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    rewindButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    playStopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    playStopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(255, 92, 92));
    playStopButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    playStopButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    rewindButton.onClick = [this]
    {
        if (onRewind != nullptr)
            onRewind();
    };

    playStopButton.onClick = [this]
    {
        if (onPlayStop != nullptr)
            onPlayStop();
    };
}

void TransportControls::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white.withAlpha(0.18f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 14.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(15.0f));
    g.drawText("TRANSPORT", getLocalBounds().reduced(16, 0), juce::Justification::centredLeft, false);
}

void TransportControls::resized()
{
    auto bounds = getLocalBounds().reduced(14, 10);
    bounds.removeFromLeft(130);
    rewindButton.setBounds(bounds.removeFromLeft(120));
    bounds.removeFromLeft(10);
    playStopButton.setBounds(bounds.removeFromLeft(120));
}

void TransportControls::setPlaying(bool shouldBePlaying)
{
    playStopButton.setButtonText(shouldBePlaying ? "Stop" : "Play");
}
