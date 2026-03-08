#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

#include <cmath>

namespace
{
constexpr int delayKnobSize = 58;

juce::String formatPercentValue(float value)
{
    return juce::String((int) std::round(juce::jlimit(0.0f, 1.0f, value) * 100.0f)) + "%";
}
}

void TrackControlChain::layoutDelayModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(34);
    content.removeFromTop(84);
    const auto cellWidth = content.getWidth() / delayControlCount;

    for (int controlIndex = 0; controlIndex < delayControlCount; ++controlIndex)
    {
        auto cell = content.removeFromLeft(controlIndex == delayControlCount - 1 ? content.getWidth() : cellWidth);
        delaySliders[(size_t) slot][(size_t) controlIndex].setBounds(cell.getCentreX() - (delayKnobSize / 2),
                                                                     cell.getY(),
                                                                     delayKnobSize,
                                                                     delayKnobSize);

        if (controlIndex == 0)
            delayTimeModeButtons[(size_t) slot].setBounds(cell.getX() + (cell.getWidth() / 2) - 15, cell.getBottom() - 16, cell.getWidth() / 2, 14);
    }
}

void TrackControlChain::paintDelayModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    auto content = bounds.reduced(10, 10);
    content.removeFromTop(24);
    auto visualizerBounds = content.removeFromTop(84).reduced(10, 10).translated(0, -4);

    const auto timeMs = juce::jlimit(20.0f, 2000.0f, engine.getTrackResolvedDelayTimeMs(selectedTrack, slot));
    const auto feedback = juce::jlimit(0.0f, 0.95f, (float) delaySliders[(size_t) slot][1].getValue());
    const auto mix = juce::jlimit(0.0f, 1.0f, (float) delaySliders[(size_t) slot][2].getValue());
    const auto waveCount = juce::roundToInt(juce::jmap(timeMs, 20.0f, 2000.0f, 6.0f, 3.0f));
    const auto spacing = juce::jmap(timeMs, 20.0f, 2000.0f, 8.0f, 18.0f);
    const auto lineThickness = 2.0f;
    const auto circleRadius = juce::jmap(mix, 7.0f, 16.0f);
    const auto circleCentre = juce::Point<float>((float) visualizerBounds.getX() + circleRadius + 8.0f,
                                                 (float) visualizerBounds.getCentreY());

    g.setColour(accent);
    g.drawEllipse(circleCentre.x - circleRadius,
                  circleCentre.y - circleRadius,
                  circleRadius * 2.0f,
                  circleRadius * 2.0f,
                  lineThickness);

    for (int waveIndex = 0; waveIndex < waveCount; ++waveIndex)
    {
        const auto progress = waveCount > 1 ? (float) waveIndex / (float) (waveCount - 1) : 0.0f;
        const auto feedbackNorm = feedback / 0.95f;
        const auto baseAlpha = juce::jmap(feedbackNorm, 0.18f, 1.0f);
        const auto fadeStrength = juce::jmap(feedbackNorm, 0.82f, 0.3f);
        const auto alpha = juce::jlimit(0.05f, 1.0f, baseAlpha * (1.0f - progress * fadeStrength));
        const auto centreX = 4.0f + circleCentre.x + circleRadius + 10.0f + (float) waveIndex * spacing * 2.0f;
        const auto width = juce::jmap(progress, 12.0f, 24.0f);
        const auto height = juce::jmap(progress, 20.0f, 44.0f);
        g.setColour(accent.withAlpha(alpha));
        g.drawEllipse(centreX - width * 0.5f,
                      circleCentre.y - height * 0.5f,
                      width,
                      height,
                      lineThickness);
    }

    static const std::array<juce::String, delayControlCount> labels { "TIME", "FDBK", "MIX" };

    g.setFont(AppFonts::getFont(11.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));

    for (int controlIndex = 0; controlIndex < delayControlCount; ++controlIndex)
    {
        const auto sliderBounds = delaySliders[(size_t) slot][(size_t) controlIndex].getBounds();
        g.drawText(labels[(size_t) controlIndex],
                   juce::Rectangle<int>(sliderBounds.getX() - 12, sliderBounds.getY() - 8, sliderBounds.getWidth() + 24, 12),
                   juce::Justification::centred,
                   false);

        if (controlIndex != 0)
            g.drawText(formatPercentValue((float) delaySliders[(size_t) slot][(size_t) controlIndex].getValue()),
                       juce::Rectangle<int>(sliderBounds.getX() - 16, sliderBounds.getBottom() - 6, sliderBounds.getWidth() + 32, 14),
                       juce::Justification::centred,
                       false);
    }
}
