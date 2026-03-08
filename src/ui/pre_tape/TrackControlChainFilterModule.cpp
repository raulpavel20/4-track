#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

#include <cmath>

namespace
{
constexpr int moduleInnerPadding = 10;
constexpr int filterKnobSize = 72;

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

void TrackControlChain::layoutFilterModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(18);
    const auto visualizerHeight = (int) std::round((float) content.getHeight() * 0.33f);
    content.removeFromTop(visualizerHeight + 10);
    filterSliders[(size_t) slot].setBounds(content.getCentreX() - (filterKnobSize / 2),
                                           content.getBottom() - filterKnobSize - 8,
                                           filterKnobSize,
                                           filterKnobSize);
}

void TrackControlChain::paintFilterModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    g.setFont(AppFonts::getFont(13.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));
    g.drawText(formatFilterValue((float) filterSliders[(size_t) slot].getValue()),
               juce::Rectangle<int>(bounds.getX() + 12,
                                    filterSliders[(size_t) slot].getBounds().getBottom() - 8,
                                    bounds.getWidth() - 24,
                                    16),
               juce::Justification::centred,
               false);

    auto visualizerBounds = bounds.reduced(moduleInnerPadding);
    visualizerBounds.removeFromTop(24);
    visualizerBounds.setHeight((int) std::round((float) bounds.getHeight() * 0.33f));
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(visualizerBounds.toFloat(), 10.0f, 1.0f);

    juce::Path responsePath;
    const auto morph = getFilterMorph(slot);

    for (int x = 0; x < visualizerBounds.getWidth(); ++x)
    {
        const auto t = visualizerBounds.getWidth() > 1 ? (float) x / (float) (visualizerBounds.getWidth() - 1) : 0.0f;
        const auto response = getFilterVisualizerResponse(t, morph);
        const auto point = juce::Point<float>((float) visualizerBounds.getX() + (float) x,
                                              (float) visualizerBounds.getBottom()
                                                  - ((float) visualizerBounds.getHeight() * response));

        if (x == 0)
            responsePath.startNewSubPath(point);
        else
            responsePath.lineTo(point);
    }

    g.setColour(accent);
    g.strokePath(responsePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
