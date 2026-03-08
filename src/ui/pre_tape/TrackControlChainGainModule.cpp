#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int utilityKnobSize = 58;

juce::String formatDbValue(float value)
{
    return juce::String(value, 1) + " dB";
}

juce::String formatPanValue(float value)
{
    if (std::abs(value) < 0.02f)
        return "C";

    return value < 0.0f ? "L" + juce::String(std::abs(value), 2) : "R" + juce::String(value, 2);
}
}

void TrackControlChain::layoutGainModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(44);
    const auto rowHeight = utilityKnobSize + 14;
    auto topRow = content.removeFromTop(rowHeight);
    content.removeFromTop(2);
    auto bottomRow = content.removeFromTop(rowHeight);
    auto& gainSlider = gainModuleSliders[(size_t) slot][0];
    auto& panSlider = gainModuleSliders[(size_t) slot][1];

    gainSlider.setBounds(topRow.getCentreX() - (utilityKnobSize / 2),
                         topRow.getY() - 6,
                         utilityKnobSize,
                         utilityKnobSize);
    panSlider.setBounds(bottomRow.getCentreX() - (utilityKnobSize / 2),
                        bottomRow.getY() + 4,
                        utilityKnobSize,
                        utilityKnobSize);
}

void TrackControlChain::paintGainModule(juce::Graphics& g, int slot, juce::Rectangle<int>)
{
    const auto gainBounds = gainModuleSliders[(size_t) slot][0].getBounds();
    const auto panBounds = gainModuleSliders[(size_t) slot][1].getBounds();

    g.setFont(AppFonts::getFont(12.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));

    g.drawText("GAIN",
               juce::Rectangle<int>(gainBounds.getX() - 14,
                                    gainBounds.getY() - 10,
                                    gainBounds.getWidth() + 28,
                                    12),
               juce::Justification::centred,
               false);
    g.drawText(formatDbValue((float) gainModuleSliders[(size_t) slot][0].getValue()),
               juce::Rectangle<int>(gainBounds.getX() - 18,
                                    gainBounds.getBottom() - 6,
                                    gainBounds.getWidth() + 36,
                                    14),
               juce::Justification::centred,
               false);

    g.drawText("PAN",
               juce::Rectangle<int>(panBounds.getX() - 14,
                                    panBounds.getY() - 10,
                                    panBounds.getWidth() + 28,
                                    12),
               juce::Justification::centred,
               false);
    g.drawText(formatPanValue((float) gainModuleSliders[(size_t) slot][1].getValue()),
               juce::Rectangle<int>(panBounds.getX() - 18,
                                    panBounds.getBottom() - 8,
                                    panBounds.getWidth() + 36,
                                    14),
               juce::Justification::centred,
               false);
}
