#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

TrackControlChain::CloseButton::CloseButton()
    : juce::Button({})
{
}

void TrackControlChain::CloseButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat();
    auto background = juce::Colours::black;

    if (isButtonDown)
        background = juce::Colours::white.withAlpha(0.18f);
    else if (isMouseOverButton)
        background = juce::Colours::white.withAlpha(0.08f);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colours::white.withAlpha(0.26f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto iconBounds = bounds.reduced(5.0f);
    juce::Path cross;
    cross.startNewSubPath(iconBounds.getTopLeft());
    cross.lineTo(iconBounds.getBottomRight());
    cross.startNewSubPath(iconBounds.getTopRight());
    cross.lineTo(iconBounds.getBottomLeft());

    g.setColour(juce::Colours::white);
    g.strokePath(cross, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

TrackControlChain::BypassButton::BypassButton()
    : juce::Button({})
{
    setClickingTogglesState(true);
}

void TrackControlChain::BypassButton::setAccentColour(juce::Colour newAccentColour)
{
    accentColour = newAccentColour;
    repaint();
}

void TrackControlChain::BypassButton::paintButton(juce::Graphics& g, bool, bool)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto bypassed = getToggleState();
    const auto ellipse = bounds.reduced(0.5f);

    if (bypassed)
    {
        g.setColour(juce::Colours::black);
        g.fillEllipse(ellipse);
        g.setColour(accentColour);
        g.drawEllipse(ellipse, 1.0f);
    }
    else
    {
        g.setColour(accentColour);
        g.fillEllipse(ellipse);
    }
}

TrackControlChain::SaturationModeButton::SaturationModeButton()
    : juce::Button({})
{
}

void TrackControlChain::SaturationModeButton::setAccentColour(juce::Colour newAccentColour)
{
    accentColour = newAccentColour;
    repaint();
}

void TrackControlChain::SaturationModeButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    const auto isOn = getToggleState();
    auto background = isOn ? accentColour : juce::Colours::black;

    if (isButtonDown)
        background = background.brighter(0.08f);
    else if (isMouseOverButton)
        background = background.brighter(0.04f);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(isOn ? juce::Colours::black : juce::Colours::white.withAlpha(0.24f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
    g.setColour(isOn ? juce::Colours::black : juce::Colours::white);
    g.setFont(isOn ? AppFonts::getFont(12.0f).boldened() : AppFonts::getFont(12.0f));
    g.drawText("Heavy", getLocalBounds(), juce::Justification::centred, false);
}

TrackControlChain::ContentComponent::ContentComponent(TrackControlChain& ownerToUse)
    : owner(ownerToUse)
{
}

void TrackControlChain::ContentComponent::paint(juce::Graphics& g)
{
    owner.paintContent(g);
}
