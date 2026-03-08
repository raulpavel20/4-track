#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

#include <cmath>

namespace
{
constexpr int phaserKnobSize = 58;

juce::String formatPercentValue(float value)
{
    return juce::String((int) std::round(juce::jlimit(0.0f, 1.0f, value) * 100.0f)) + "%";
}

juce::String formatFrequencyValue(float value)
{
    const auto clamped = juce::jlimit(20.0f, 20000.0f, value);

    if (clamped >= 1000.0f)
    {
        const auto kilo = clamped / 1000.0f;
        const auto decimals = std::abs(kilo - std::round(kilo)) < 0.05f ? 0 : 1;
        return juce::String(kilo, decimals) + "k";
    }

    return juce::String((int) std::round(clamped));
}
}

void TrackControlChain::layoutPhaserModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(34);
    auto topRow = content.removeFromTop(76);
    content.removeFromTop(6);
    auto bottomRow = content.removeFromTop(phaserKnobSize + 18);
    const auto cellWidth = bottomRow.getWidth() / 4;

    for (int controlIndex = 0; controlIndex < 4; ++controlIndex)
    {
        auto cell = bottomRow.removeFromLeft(controlIndex == 3 ? bottomRow.getWidth() : cellWidth);
        phaserSliders[(size_t) slot][(size_t) controlIndex].setBounds(cell.getCentreX() - (phaserKnobSize / 2),
                                                                      cell.getY(),
                                                                      phaserKnobSize,
                                                                      phaserKnobSize);

        if (controlIndex == 0)
            phaserRateModeButtons[(size_t) slot].setBounds(cell.getX() + (cell.getWidth() / 2) - 19, cell.getBottom() - 18, (cell.getWidth() / 2) + 6, 14);
    }

    auto mixCell = topRow.removeFromRight(cellWidth);
    phaserSliders[(size_t) slot][4].setBounds(mixCell.getCentreX() - (phaserKnobSize / 2),
                                              topRow.getCentreY() - (phaserKnobSize / 2) - 8,
                                              phaserKnobSize,
                                              phaserKnobSize);
}

void TrackControlChain::paintPhaserModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    static const std::array<juce::String, phaserControlCount> labels { "RATE", "DEPTH", "CENTRE FQ", "FDBK", "MIX" };

    auto content = bounds.reduced(10, 10);
    content.removeFromTop(24);
    auto topRow = content.removeFromTop(82);
    const auto mixBounds = phaserSliders[(size_t) slot][4].getBounds();
    auto visualizerBounds = topRow.withRight(mixBounds.getX() - 6).reduced(10, 10);
    visualizerBounds.translate(0, 2);

    const auto depth = juce::jlimit(0.0f, 1.0f, (float) phaserSliders[(size_t) slot][1].getValue());
    const auto centreFrequency = juce::jlimit(200.0f, 2000.0f, (float) phaserSliders[(size_t) slot][2].getValue());
    const auto feedback = juce::jlimit(0.0f, 0.95f, (float) phaserSliders[(size_t) slot][3].getValue());
    const auto mix = juce::jlimit(0.0f, 1.0f, (float) phaserSliders[(size_t) slot][4].getValue());
    const auto circleRadius = juce::jmap(mix,
                                         8.0f,
                                         juce::jmin((float) visualizerBounds.getWidth(), (float) visualizerBounds.getHeight()) * 0.42f);
    const auto circleCentre = visualizerBounds.toFloat().getCentre();
    const auto groupWidth = juce::jmap(depth,
                                       (float) visualizerBounds.getWidth() * 0.42f,
                                       (float) visualizerBounds.getWidth() * 0.82f);
    const auto spacing = groupWidth / 5.0f;
    const auto startX = (float) visualizerBounds.getCentreX() - (groupWidth * 0.5f);
    const auto lineThickness = juce::jmap(feedback, 1.0f, 4.0f);
    const auto motionAmount = 10.0f;
    const auto phase = phaserVisualPhases[(size_t) slot];
    const auto centreYOffset = juce::jmap(centreFrequency, 200.0f, 2000.0f, -6.0f, 6.0f);
    const auto topY = (float) visualizerBounds.getY() + 12.0f + centreYOffset;
    const auto bottomY = (float) visualizerBounds.getBottom() - 28.0f + centreYOffset;
    const auto lineHeight = 18.0f;

    g.setColour(accent.withAlpha(0.5f));
    g.fillEllipse(circleCentre.x - circleRadius,
                  circleCentre.y - circleRadius,
                  circleRadius * 2.0f,
                  circleRadius * 2.0f);

    for (int lineIndex = 0; lineIndex < 6; ++lineIndex)
    {
        const auto baseX = startX + ((float) lineIndex * spacing);
        const auto topOffset = std::sin(phase + ((float) lineIndex * 0.4f)) * motionAmount;
        const auto bottomOffset = -topOffset;

        g.setColour(accent);
        g.drawLine(baseX + topOffset,
                   topY,
                   baseX + topOffset,
                   topY + lineHeight,
                   lineThickness);

        g.setColour(juce::Colours::white);
        g.drawLine(baseX + bottomOffset,
                   bottomY,
                   baseX + bottomOffset,
                   bottomY + lineHeight,
                   lineThickness);
    }

    g.setFont(AppFonts::getFont(11.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));

    for (int controlIndex = 0; controlIndex < phaserControlCount; ++controlIndex)
    {
        const auto sliderBounds = phaserSliders[(size_t) slot][(size_t) controlIndex].getBounds();
        juce::String valueText;

        if (controlIndex == 1)
            valueText = formatPercentValue(depth);
        else if (controlIndex == 2)
            valueText = formatFrequencyValue(centreFrequency);
        else if (controlIndex == 3)
            valueText = formatPercentValue(feedback);
        else
            valueText = formatPercentValue(mix);

        g.drawText(labels[(size_t) controlIndex],
                   juce::Rectangle<int>(sliderBounds.getX() - 18, sliderBounds.getY() - 8, sliderBounds.getWidth() + 36, 12),
                   juce::Justification::centred,
                   false);

        if (controlIndex != 0)
            g.drawText(valueText,
                       juce::Rectangle<int>(sliderBounds.getX() - 20, sliderBounds.getBottom() - 6, sliderBounds.getWidth() + 40, 14),
                       juce::Justification::centred,
                       false);
    }
}
