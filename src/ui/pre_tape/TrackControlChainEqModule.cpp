#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int eqGainSliderHeight = 80;
constexpr int eqGainSliderWidth = 18;
constexpr int eqQKnobSize = 38;
constexpr int eqFrequencyEditorHeight = 20;
constexpr int eqFrequencyEditorWidth = 38;

juce::String formatDbValue(float value)
{
    return juce::String(value, 1) + " dB";
}

void drawVerticalSliderTrack(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto x = (float) bounds.getCentreX();
    const auto top = (float) bounds.getY() + 4.0f;
    const auto bottom = (float) bounds.getBottom() - 4.0f;
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawLine(x, top, x, bottom, 2.0f);
}
}

void TrackControlChain::layoutEqModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(16);
    const auto columnWidth = content.getWidth() / Track::maxEqBands;

    for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
    {
        auto column = content.removeFromLeft(columnWidth);
        auto sliderBounds = juce::Rectangle<int>(column.getCentreX() - (eqGainSliderWidth / 2),
                                                 column.getY() + 18,
                                                 eqGainSliderWidth,
                                                 eqGainSliderHeight);
        eqGainSliders[(size_t) slot][(size_t) bandIndex].setBounds(sliderBounds);

        auto qBounds = juce::Rectangle<int>(column.getCentreX() - (eqQKnobSize / 2),
                                            sliderBounds.getBottom() + 16,
                                            eqQKnobSize,
                                            eqQKnobSize);
        eqQSliders[(size_t) slot][(size_t) bandIndex].setBounds(qBounds);

        eqFrequencyEditors[(size_t) slot][(size_t) bandIndex].setBounds(column.getCentreX() - (eqFrequencyEditorWidth / 2),
                                                                        qBounds.getBottom() - 2,
                                                                        eqFrequencyEditorWidth,
                                                                        eqFrequencyEditorHeight);
    }
}

void TrackControlChain::paintEqModule(juce::Graphics& g, int slot)
{
    g.setFont(AppFonts::getFont(11.0f));
    g.setColour(juce::Colours::white.withAlpha(0.7f));

    for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
    {
        const auto sliderBounds = eqGainSliders[(size_t) slot][(size_t) bandIndex].getBounds();
        drawVerticalSliderTrack(g, sliderBounds);
        g.drawText(formatDbValue((float) eqGainSliders[(size_t) slot][(size_t) bandIndex].getValue()),
                   juce::Rectangle<int>(sliderBounds.getX() - 12, sliderBounds.getBottom() + 2, sliderBounds.getWidth() + 24, 14),
                   juce::Justification::centred,
                   false);
    }
}
