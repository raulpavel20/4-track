#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int moduleInnerPadding = 10;
constexpr int saturationAmountKnobSize = 70;
constexpr int meterWidth = 10;
}

void TrackControlChain::layoutSaturationModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(34);
    content.removeFromLeft(meterWidth + 14);
    content.removeFromRight(meterWidth + 14);
    auto centerArea = content.reduced(6, 4);
    saturationModeButtons[(size_t) slot].setBounds(centerArea.getCentreX() - 34,
                                                   centerArea.getY() + 6,
                                                   68,
                                                   20);
    centerArea.removeFromTop(32);
    saturationAmountSliders[(size_t) slot].setBounds(centerArea.getCentreX() - (saturationAmountKnobSize / 2),
                                                     centerArea.getY() + 16,
                                                     saturationAmountKnobSize,
                                                     saturationAmountKnobSize);
}

void TrackControlChain::paintSaturationModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
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

    g.setFont(AppFonts::getFont(12.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));
    g.drawText(juce::String((float) saturationAmountSliders[(size_t) slot].getValue(), 2),
               juce::Rectangle<int>(bounds.getX() + 28,
                                    saturationAmountSliders[(size_t) slot].getBounds().getBottom() - 6,
                                    bounds.getWidth() - 56,
                                    16),
               juce::Justification::centred,
               false);
}
