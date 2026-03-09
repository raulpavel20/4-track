#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

#include <cmath>

namespace
{
constexpr int chorusKnobSize = 58;

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

void TrackControlChain::layoutChorusModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(34);
    auto topRow = content.removeFromTop(76);
    content.removeFromTop(6);
    auto bottomRow = content.removeFromTop(chorusKnobSize + 18);
    const auto cellWidth = bottomRow.getWidth() / 4;

    for (int controlIndex = 0; controlIndex < 4; ++controlIndex)
    {
        auto cell = bottomRow.removeFromLeft(controlIndex == 3 ? bottomRow.getWidth() : cellWidth);
        chorusSliders[(size_t) slot][(size_t) controlIndex].setBounds(cell.getCentreX() - (chorusKnobSize / 2),
                                                                      cell.getY(),
                                                                      chorusKnobSize,
                                                                      chorusKnobSize);

        if (controlIndex == 0)
            chorusRateModeButtons[(size_t) slot].setBounds(cell.getX() + (cell.getWidth() / 2) - 19, cell.getBottom() - 18, (cell.getWidth() / 2) + 6, 14);
    }

    auto mixCell = topRow.removeFromRight(cellWidth);
    chorusSliders[(size_t) slot][4].setBounds(mixCell.getCentreX() - (chorusKnobSize / 2),
                                              topRow.getCentreY() - (chorusKnobSize / 2) - 8,
                                              chorusKnobSize,
                                              chorusKnobSize);
}

void TrackControlChain::paintChorusModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    static const std::array<juce::String, chorusControlCount> labels { "RATE", "DEPTH", "CENTRE FQ", "FDBK", "MIX" };

    auto content = bounds.reduced(10, 10);
    content.removeFromTop(24);
    auto topRow = content.removeFromTop(82);
    const auto mixBounds = chorusSliders[(size_t) slot][4].getBounds();
    auto visualizerBounds = topRow.withRight(mixBounds.getX() - 6).reduced(10, 10);
    visualizerBounds.translate(0, 2);

    const auto depth = juce::jlimit(0.0f, 1.0f, (float) chorusSliders[(size_t) slot][1].getValue());
    const auto centreFrequency = juce::jlimit(200.0f, 2000.0f, (float) chorusSliders[(size_t) slot][2].getValue());
    const auto feedback = juce::jlimit(0.0f, 0.95f, (float) chorusSliders[(size_t) slot][3].getValue());
    const auto mix = juce::jlimit(0.0f, 1.0f, (float) chorusSliders[(size_t) slot][4].getValue());
    const auto circleRadius = juce::jmap(mix,
                                         8.0f,
                                         juce::jmin((float) visualizerBounds.getWidth(), (float) visualizerBounds.getHeight()) * 0.42f);
    const auto circleCentre = visualizerBounds.toFloat().getCentre();
    const auto amplitude = juce::jmap(depth, 4.0f, (float) visualizerBounds.getHeight() * 0.22f);
    const auto thickness = juce::jmap(feedback, 1.0f, 4.0f);
    const auto phase = chorusVisualPhases[(size_t) slot];
    const auto bottomWavePhaseOffset = juce::jmap(centreFrequency,
                                                  200.0f,
                                                  2000.0f,
                                                  -juce::MathConstants<float>::halfPi,
                                                  juce::MathConstants<float>::halfPi);
    const auto topY = (float) visualizerBounds.getCentreY() - 12.0f;
    const auto bottomY = (float) visualizerBounds.getCentreY() + 12.0f;
    const auto left = (float) visualizerBounds.getX();
    const auto right = (float) visualizerBounds.getRight();
    const auto width = juce::jmax(1.0f, right - left);
    const auto cycles = juce::jmap(depth, 1.2f, 2.2f);

    g.setColour(accent.withAlpha(0.5f));
    g.fillEllipse(circleCentre.x - circleRadius,
                  circleCentre.y - circleRadius,
                  circleRadius * 2.0f,
                  circleRadius * 2.0f);

    juce::Path topWave;
    juce::Path bottomWave;

    for (int pointIndex = 0; pointIndex <= 64; ++pointIndex)
    {
        const auto t = (float) pointIndex / 64.0f;
        const auto x = left + (t * width);
        const auto angle = (t * juce::MathConstants<float>::twoPi * cycles) + phase;
        const auto topValue = std::sin(angle) * amplitude;
        const auto bottomValue = std::sin(angle + juce::MathConstants<float>::pi + bottomWavePhaseOffset) * amplitude;
        const auto y1 = topY + topValue;
        const auto y2 = bottomY + bottomValue;

        if (pointIndex == 0)
        {
            topWave.startNewSubPath(x, y1);
            bottomWave.startNewSubPath(x, y2);
        }
        else
        {
            topWave.lineTo(x, y1);
            bottomWave.lineTo(x, y2);
        }
    }

    g.setColour(accent);
    g.strokePath(topWave, juce::PathStrokeType(thickness));
    g.setColour(juce::Colours::white);
    g.strokePath(bottomWave, juce::PathStrokeType(thickness));

    g.setFont(AppFonts::getFont(11.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));

    for (int controlIndex = 0; controlIndex < chorusControlCount; ++controlIndex)
    {
        const auto sliderBounds = chorusSliders[(size_t) slot][(size_t) controlIndex].getBounds();
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
