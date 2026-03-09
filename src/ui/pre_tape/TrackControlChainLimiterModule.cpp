#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int moduleInnerPadding = 10;
constexpr int limiterKnobSize = 60;
constexpr int meterWidth = 10;

juce::String formatDbValue(float value)
{
    return juce::String(value, 1) + " dB";
}

juce::String formatMsValue(float value)
{
    return juce::String(value, value < 10.0f ? 1 : 0) + " ms";
}
}

void TrackControlChain::layoutLimiterModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(34);
    content.removeFromLeft(meterWidth + 14);
    content.removeFromRight(meterWidth + 14);
    auto centerArea = content.reduced(4, 2);
    const auto rowHeight = limiterKnobSize + 14;
    auto topRow = centerArea.removeFromTop(rowHeight);
    centerArea.removeFromTop(6);
    auto bottomRow = centerArea.removeFromTop(rowHeight);

    limiterSliders[(size_t) slot][0].setBounds(topRow.getCentreX() - (limiterKnobSize / 2),
                                               topRow.getY() + 2,
                                               limiterKnobSize,
                                               limiterKnobSize);
    limiterSliders[(size_t) slot][1].setBounds(bottomRow.getCentreX() - (limiterKnobSize / 2),
                                               bottomRow.getY(),
                                               limiterKnobSize,
                                               limiterKnobSize);
}

void TrackControlChain::paintLimiterModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    static const std::array<juce::String, limiterControlCount> labels { "Thr", "Rel" };

    const auto meterHeight = 140;
    const auto inputMeterBounds = juce::Rectangle<int>(bounds.getX() + moduleInnerPadding,
                                                       bounds.getY() + 44,
                                                       meterWidth,
                                                       meterHeight);
    const auto outputMeterBounds = juce::Rectangle<int>(bounds.getRight() - moduleInnerPadding - meterWidth,
                                                        bounds.getY() + 44,
                                                        meterWidth,
                                                        meterHeight);
    const auto inputLevel = juce::jlimit(0.0f, 1.0f, getModuleInputMeter(slot));
    const auto outputLevel = juce::jlimit(0.0f, 1.0f, getModuleOutputMeter(slot));

    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(inputMeterBounds.toFloat(), 5.0f, 1.0f);
    g.drawRoundedRectangle(outputMeterBounds.toFloat(), 5.0f, 1.0f);

    g.setColour(accent);
    g.fillRoundedRectangle(juce::Rectangle<float>((float) inputMeterBounds.getX() + 2.0f,
                                                  (float) inputMeterBounds.getBottom() - 2.0f - ((float) (inputMeterBounds.getHeight() - 4) * inputLevel),
                                                  (float) inputMeterBounds.getWidth() - 4.0f,
                                                  (float) (inputMeterBounds.getHeight() - 4) * inputLevel),
                           3.0f);
    g.fillRoundedRectangle(juce::Rectangle<float>((float) outputMeterBounds.getX() + 2.0f,
                                                  (float) outputMeterBounds.getBottom() - 2.0f - ((float) (outputMeterBounds.getHeight() - 4) * outputLevel),
                                                  (float) outputMeterBounds.getWidth() - 4.0f,
                                                  (float) (outputMeterBounds.getHeight() - 4) * outputLevel),
                           3.0f);

    g.setFont(AppFonts::getFont(11.0f));
    g.setColour(juce::Colours::white.withAlpha(0.7f));

    for (int controlIndex = 0; controlIndex < limiterControlCount; ++controlIndex)
    {
        const auto boundsForSlider = limiterSliders[(size_t) slot][(size_t) controlIndex].getBounds();
        const auto value = (float) limiterSliders[(size_t) slot][(size_t) controlIndex].getValue();
        const auto valueText = controlIndex == 0 ? formatDbValue(value)
                                                 : formatMsValue(value);

        g.drawText(labels[(size_t) controlIndex],
                   juce::Rectangle<int>(boundsForSlider.getX() - 12, boundsForSlider.getY() - 8, boundsForSlider.getWidth() + 24, 12),
                   juce::Justification::centred,
                   false);
        g.drawText(valueText,
                   juce::Rectangle<int>(boundsForSlider.getX() - 18, boundsForSlider.getBottom() - 6, boundsForSlider.getWidth() + 36, 14),
                   juce::Justification::centred,
                   false);
    }
}
