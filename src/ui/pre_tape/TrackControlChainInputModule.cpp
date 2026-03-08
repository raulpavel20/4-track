#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int moduleVerticalInset = 10;
constexpr int moduleInnerPadding = 10;
constexpr int inputKnobSize = 80;

juce::String formatInputGainValue(float value)
{
    return juce::String(value, 2) + "x";
}
}

void TrackControlChain::layoutInputModule(juce::Rectangle<int> inputBounds)
{
    auto inputLayout = inputBounds.reduced(moduleInnerPadding, moduleVerticalInset);
    inputLayout.removeFromTop(26);
    inputSourceBox.setBounds(inputLayout.removeFromTop(28));
    inputLayout.removeFromTop(6);
    inputGainSlider.setBounds(inputLayout.getCentreX() - (inputKnobSize / 2),
                              inputLayout.getY() + 16,
                              inputKnobSize,
                              inputKnobSize);
}

void TrackControlChain::paintInputModule(juce::Graphics& g, juce::Colour accent, juce::Rectangle<int> inputBounds)
{
    g.setColour(accent);
    g.drawRoundedRectangle(inputBounds.toFloat(), 14.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(16.0f));
    g.drawText("Input",
               inputBounds.withTrimmedTop(10).removeFromTop(18).reduced(12, 0),
               juce::Justification::centredLeft,
               false);

    g.setFont(AppFonts::getFont(13.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));
    g.drawText(formatInputGainValue((float) inputGainSlider.getValue()),
               juce::Rectangle<int>(inputBounds.getX() + 12,
                                    inputGainSlider.getBounds().getBottom() - 8,
                                    inputBounds.getWidth() - 24,
                                    16),
               juce::Justification::centred,
               false);
}
