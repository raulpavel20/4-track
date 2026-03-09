#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int moduleInnerPadding = 10;
constexpr int noiseGateKnobSize = 60;
constexpr int meterWidth = 10;

juce::String formatDbValue(float value)
{
    return juce::String(value, 1) + " dB";
}

juce::String formatRatioValue(float value)
{
    return juce::String(value, 1) + ":1";
}

juce::String formatMsValue(float value)
{
    return juce::String(value, value < 10.0f ? 1 : 0) + " ms";
}
}

void TrackControlChain::layoutNoiseGateModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(34);
    const auto meterSpace = meterWidth + 14;
    content.removeFromLeft(meterSpace);
    content.removeFromRight(meterSpace);
    const auto rowHeight = noiseGateKnobSize + 12;
    auto topRow = content.removeFromTop(rowHeight);
    content.removeFromTop(6);
    auto bottomRow = content.removeFromTop(rowHeight);
    const auto topCellWidth = topRow.getWidth() / 2;
    const auto bottomCellWidth = bottomRow.getWidth() / 2;

    for (int controlIndex = 0; controlIndex < 2; ++controlIndex)
    {
        auto cell = topRow.removeFromLeft(controlIndex == 1 ? topRow.getWidth() : topCellWidth);
        noiseGateSliders[(size_t) slot][(size_t) controlIndex].setBounds(cell.getCentreX() - (noiseGateKnobSize / 2),
                                                                         cell.getY(),
                                                                         noiseGateKnobSize,
                                                                         noiseGateKnobSize);
    }

    for (int controlIndex = 0; controlIndex < 2; ++controlIndex)
    {
        auto cell = bottomRow.removeFromLeft(controlIndex == 1 ? bottomRow.getWidth() : bottomCellWidth);
        noiseGateSliders[(size_t) slot][(size_t) (controlIndex + 2)].setBounds(cell.getCentreX() - (noiseGateKnobSize / 2),
                                                                                cell.getY(),
                                                                                noiseGateKnobSize,
                                                                                noiseGateKnobSize);
    }
}

void TrackControlChain::paintNoiseGateModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    static const std::array<juce::String, noiseGateControlCount> labels { "Thr", "Rat", "Atk", "Rel" };

    auto moduleContent = bounds.reduced(moduleInnerPadding);
    moduleContent.removeFromTop(26);
    const auto meterHeight = 140;
    const auto inputMeterBounds = juce::Rectangle<int>(moduleContent.getX(), moduleContent.getY() + 8, meterWidth, meterHeight);
    const auto outputMeterBounds = juce::Rectangle<int>(bounds.getRight() - moduleInnerPadding - meterWidth,
                                                        moduleContent.getY() + 8,
                                                        meterWidth,
                                                        meterHeight);

    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(inputMeterBounds.toFloat(), 5.0f, 1.0f);
    g.drawRoundedRectangle(outputMeterBounds.toFloat(), 5.0f, 1.0f);
    const auto inputLevel = juce::jlimit(0.0f, 1.0f, getModuleInputMeter(slot));
    const auto outputLevel = juce::jlimit(0.0f, 1.0f, getModuleOutputMeter(slot));
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

    for (int controlIndex = 0; controlIndex < noiseGateControlCount; ++controlIndex)
    {
        const auto boundsForSlider = noiseGateSliders[(size_t) slot][(size_t) controlIndex].getBounds();
        juce::String valueText;

        if (controlIndex == 0)
            valueText = formatDbValue((float) noiseGateSliders[(size_t) slot][(size_t) controlIndex].getValue());
        else if (controlIndex == 1)
            valueText = formatRatioValue((float) noiseGateSliders[(size_t) slot][(size_t) controlIndex].getValue());
        else
            valueText = formatMsValue((float) noiseGateSliders[(size_t) slot][(size_t) controlIndex].getValue());

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
