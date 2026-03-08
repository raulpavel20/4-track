#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

#include <cmath>

namespace
{
constexpr int reverbKnobSize = 58;

juce::String formatPercentValue(float value)
{
    return juce::String((int) std::round(juce::jlimit(0.0f, 1.0f, value) * 100.0f)) + "%";
}
}

void TrackControlChain::layoutReverbModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(34);
    content.removeFromTop(84);
    const auto cellWidth = content.getWidth() / reverbControlCount;

    for (int controlIndex = 0; controlIndex < reverbControlCount; ++controlIndex)
    {
        auto cell = content.removeFromLeft(controlIndex == reverbControlCount - 1 ? content.getWidth() : cellWidth);
        reverbSliders[(size_t) slot][(size_t) controlIndex].setBounds(cell.getCentreX() - (reverbKnobSize / 2),
                                                                      cell.getY(),
                                                                      reverbKnobSize,
                                                                      reverbKnobSize);
    }
}

void TrackControlChain::paintReverbModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    auto content = bounds.reduced(10, 10);
    content.removeFromTop(24);
    auto visualizerBounds = content.removeFromTop(84).reduced(10, 10).translated(0, -4);

    const auto size = juce::jlimit(0.0f, 1.0f, (float) reverbSliders[(size_t) slot][0].getValue());
    const auto damping = juce::jlimit(0.0f, 1.0f, (float) reverbSliders[(size_t) slot][1].getValue());
    const auto mix = juce::jlimit(0.0f, 1.0f, (float) reverbSliders[(size_t) slot][2].getValue());
    const auto depth = juce::roundToInt(juce::jmap(size, 10.0f, 28.0f));
    const auto lineThickness = juce::jmap(damping, 1.0f, 3.6f);
    auto frontFace = visualizerBounds.reduced(18, 10);
    frontFace.removeFromLeft(depth);
    frontFace.removeFromBottom(depth / 2);
    auto backFace = frontFace.translated(-depth, depth / 2);
    const auto front = frontFace.toFloat();
    const auto back = backFace.toFloat();
    const auto frontTopLeft = front.getTopLeft();
    const auto frontTopRight = front.getTopRight();
    const auto frontBottomLeft = front.getBottomLeft();
    const auto frontBottomRight = front.getBottomRight();
    const auto backTopLeft = back.getTopLeft();
    const auto backTopRight = back.getTopRight();
    const auto backBottomLeft = back.getBottomLeft();
    const auto backBottomRight = back.getBottomRight();
    const auto circleCentre = juce::Point<float>((front.getCentreX() + back.getCentreX()) * 0.5f,
                                                 (front.getCentreY() + back.getCentreY()) * 0.5f);
    const auto circleRadius = juce::jmap(mix, 5.0f, juce::jmin(front.getWidth(), front.getHeight()) * 0.22f);

    g.setColour(accent);
    g.drawRect(front, lineThickness);
    g.drawRect(back, lineThickness);
    g.drawLine(frontTopLeft.x, frontTopLeft.y, backTopLeft.x, backTopLeft.y, lineThickness);
    g.drawLine(frontTopRight.x, frontTopRight.y, backTopRight.x, backTopRight.y, lineThickness);
    g.drawLine(frontBottomLeft.x, frontBottomLeft.y, backBottomLeft.x, backBottomLeft.y, lineThickness);
    g.drawLine(frontBottomRight.x, frontBottomRight.y, backBottomRight.x, backBottomRight.y, lineThickness);
    g.drawEllipse(circleCentre.x - circleRadius,
                  circleCentre.y - circleRadius,
                  circleRadius * 2.0f,
                  circleRadius * 2.0f,
                  juce::jmax(1.0f, lineThickness - 0.4f));

    static const std::array<juce::String, reverbControlCount> labels { "SIZE", "DAMP", "MIX" };

    g.setFont(AppFonts::getFont(11.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));

    for (int controlIndex = 0; controlIndex < reverbControlCount; ++controlIndex)
    {
        const auto sliderBounds = reverbSliders[(size_t) slot][(size_t) controlIndex].getBounds();
        g.drawText(labels[(size_t) controlIndex],
                   juce::Rectangle<int>(sliderBounds.getX() - 12, sliderBounds.getY() - 8, sliderBounds.getWidth() + 24, 12),
                   juce::Justification::centred,
                   false);
        g.drawText(formatPercentValue((float) reverbSliders[(size_t) slot][(size_t) controlIndex].getValue()),
                   juce::Rectangle<int>(sliderBounds.getX() - 16, sliderBounds.getBottom() - 6, sliderBounds.getWidth() + 32, 14),
                   juce::Justification::centred,
                   false);
    }
}
