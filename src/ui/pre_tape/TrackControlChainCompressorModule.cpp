#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int moduleInnerPadding = 10;
constexpr int compressorKnobSize = 60;
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

void drawVerticalSliderTrack(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto x = (float) bounds.getCentreX();
    const auto top = (float) bounds.getY() + 4.0f;
    const auto bottom = (float) bounds.getBottom() - 4.0f;
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawLine(x, top, x, bottom, 2.0f);
}
}

void TrackControlChain::layoutCompressorModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(34);
    const auto inputMeterSpace = meterWidth + 14;
    content.removeFromLeft(inputMeterSpace);
    auto makeupColumn = content.removeFromRight(meterWidth + 20);
    const auto makeupSliderX = makeupColumn.getX() - 18;
    auto knobArea = content;
    const auto rowHeight = compressorKnobSize + 12;
    auto topRow = knobArea.removeFromTop(rowHeight);
    knobArea.removeFromTop(6);
    auto bottomRow = knobArea.removeFromTop(rowHeight);
    const auto knobColumnWidth = knobArea.getWidth() / 2;
    const auto topOffset = knobColumnWidth / 4;
    const auto bottomOffset = knobColumnWidth / 2 + 3;
    constexpr int inwardNudge = 8;
    constexpr int rightKnobLeftShift = 20;

    compressorSliders[(size_t) slot][0].setBounds(topRow.getX() + topOffset - (compressorKnobSize / 2),
                                                  topRow.getY(),
                                                  compressorKnobSize,
                                                  compressorKnobSize);
    compressorSliders[(size_t) slot][1].setBounds(topRow.getX() + topOffset + knobColumnWidth - (compressorKnobSize / 2) - inwardNudge - rightKnobLeftShift,
                                                  topRow.getY(),
                                                  compressorKnobSize,
                                                  compressorKnobSize);
    compressorSliders[(size_t) slot][2].setBounds(bottomRow.getX() + bottomOffset - (compressorKnobSize / 2) + inwardNudge,
                                                  bottomRow.getY(),
                                                  compressorKnobSize,
                                                  compressorKnobSize);
    compressorSliders[(size_t) slot][3].setBounds(bottomRow.getX() + bottomOffset + knobColumnWidth - (compressorKnobSize / 2) - rightKnobLeftShift,
                                                  bottomRow.getY(),
                                                  compressorKnobSize,
                                                  compressorKnobSize);
    compressorSliders[(size_t) slot][4].setBounds(makeupSliderX,
                                                  content.getCentreY() - 72,
                                                  18,
                                                  128);
}

void TrackControlChain::paintCompressorModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    static const std::array<juce::String, compressorControlCount> compressorLabels { "Thr", "Rat", "Atk", "Rel", "Make" };

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

    for (int controlIndex = 0; controlIndex < compressorControlCount; ++controlIndex)
    {
        const auto boundsForSlider = compressorSliders[(size_t) slot][(size_t) controlIndex].getBounds();
        auto valueText = juce::String();

        if (controlIndex == 0 || controlIndex == 4)
            valueText = formatDbValue((float) compressorSliders[(size_t) slot][(size_t) controlIndex].getValue());
        else if (controlIndex == 1)
            valueText = formatRatioValue((float) compressorSliders[(size_t) slot][(size_t) controlIndex].getValue());
        else
            valueText = formatMsValue((float) compressorSliders[(size_t) slot][(size_t) controlIndex].getValue());

        if (controlIndex == 4)
        {
            drawVerticalSliderTrack(g, boundsForSlider);
            g.drawText(compressorLabels[(size_t) controlIndex],
                       juce::Rectangle<int>(boundsForSlider.getX() - 18, boundsForSlider.getY() - 10, boundsForSlider.getWidth() + 36, 12),
                       juce::Justification::centred,
                       false);
            g.drawText(valueText,
                       juce::Rectangle<int>(boundsForSlider.getX() - 24, boundsForSlider.getBottom() + 2, boundsForSlider.getWidth() + 48, 14),
                       juce::Justification::centred,
                       false);
        }
        else
        {
            g.drawText(compressorLabels[(size_t) controlIndex],
                       juce::Rectangle<int>(boundsForSlider.getX() - 12, boundsForSlider.getY() - 8, boundsForSlider.getWidth() + 24, 12),
                       juce::Justification::centred,
                       false);
            g.drawText(valueText,
                       juce::Rectangle<int>(boundsForSlider.getX() - 18, boundsForSlider.getBottom() - 6, boundsForSlider.getWidth() + 36, 14),
                       juce::Justification::centred,
                       false);
        }
    }
}
