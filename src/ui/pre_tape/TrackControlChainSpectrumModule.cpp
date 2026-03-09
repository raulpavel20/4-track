#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int graphInset = 12;

float getFrequencyLabelRatio(float frequencyHz, float minFrequencyHz, float maxFrequencyHz)
{
    const auto safeFrequency = juce::jlimit(minFrequencyHz, maxFrequencyHz, frequencyHz);
    return std::log(safeFrequency / minFrequencyHz) / std::log(maxFrequencyHz / minFrequencyHz);
}

juce::String formatFrequencyLabel(float frequencyHz)
{
    if (frequencyHz >= 1000.0f)
    {
        const auto kiloHertz = frequencyHz / 1000.0f;
        return juce::String(kiloHertz, kiloHertz < 10.0f ? 1 : 0) + "k";
    }

    return juce::String((int) std::round(frequencyHz));
}
}

void TrackControlChain::layoutSpectrumAnalyzerModule(int, juce::Rectangle<int>)
{
}

void TrackControlChain::paintSpectrumAnalyzerModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds)
{
    auto graphBounds = bounds.reduced(graphInset, 10);
    graphBounds.removeFromTop(30);
    graphBounds.reduce(2, 6);
    graphBounds.removeFromTop(18);

    static constexpr std::array<float, 7> frequencyLabels { 30.0f, 100.0f, 300.0f, 1000.0f, 3000.0f, 10000.0f, 20000.0f };
    constexpr auto minFrequency = 30.0f;
    constexpr auto maxFrequency = 20000.0f;

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.setFont(AppFonts::getFont(10.0f));

    for (int labelIndex = 0; labelIndex < (int) frequencyLabels.size(); ++labelIndex)
    {
        const auto frequency = frequencyLabels[(size_t) labelIndex];
        const auto ratio = getFrequencyLabelRatio(frequency, minFrequency, maxFrequency);
        const auto x = juce::jmap(ratio,
                                  (float) graphBounds.getX() + 8.0f,
                                  (float) graphBounds.getRight() - 8.0f);
        const auto labelWidth = 34;
        auto labelBounds = juce::Rectangle<int>((int) std::round(x) - (labelWidth / 2),
                                                graphBounds.getY() - 16,
                                                labelWidth,
                                                12);

        if (labelIndex == 0)
            labelBounds.setX(graphBounds.getX());
        else if (labelIndex == (int) frequencyLabels.size() - 1)
            labelBounds.setX(graphBounds.getRight() - labelWidth);

        g.drawText(formatFrequencyLabel(frequency), labelBounds, juce::Justification::centred, false);
    }

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(graphBounds.toFloat(), 10.0f, 1.0f);

    for (int lineIndex = 1; lineIndex < 4; ++lineIndex)
    {
        const auto y = juce::jmap((float) lineIndex / 4.0f,
                                  (float) graphBounds.getY() + 6.0f,
                                  (float) graphBounds.getBottom() - 6.0f);
        g.drawHorizontalLine((int) std::round(y), (float) graphBounds.getX() + 6.0f, (float) graphBounds.getRight() - 6.0f);
    }

    for (int lineIndex = 1; lineIndex < 6; ++lineIndex)
    {
        const auto x = juce::jmap((float) lineIndex / 6.0f,
                                  (float) graphBounds.getX() + 6.0f,
                                  (float) graphBounds.getRight() - 6.0f);
        g.drawVerticalLine((int) std::round(x), (float) graphBounds.getY() + 6.0f, (float) graphBounds.getBottom() - 6.0f);
    }

    if (isModuleBypassed(slot))
    {
        g.drawHorizontalLine(graphBounds.getCentreY(), (float) graphBounds.getX() + 8.0f, (float) graphBounds.getRight() - 8.0f);
        return;
    }

    const auto spectrum = getSpectrumAnalyzerData(slot);
    juce::Path linePath;
    juce::Path fillPath;

    for (int binIndex = 0; binIndex < ModuleChain::spectrumAnalyzerBinCount; ++binIndex)
    {
        const auto x = juce::jmap((float) binIndex / (float) (ModuleChain::spectrumAnalyzerBinCount - 1),
                                  (float) graphBounds.getX() + 8.0f,
                                  (float) graphBounds.getRight() - 8.0f);
        const auto y = juce::jmap(juce::jlimit(0.0f, 1.0f, spectrum[(size_t) binIndex]),
                                  (float) graphBounds.getBottom() - 8.0f,
                                  (float) graphBounds.getY() + 8.0f);

        if (binIndex == 0)
        {
            linePath.startNewSubPath(x, y);
            fillPath.startNewSubPath(x, (float) graphBounds.getBottom() - 8.0f);
            fillPath.lineTo(x, y);
        }
        else
        {
            linePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
    }

    fillPath.lineTo((float) graphBounds.getRight() - 8.0f, (float) graphBounds.getBottom() - 8.0f);
    fillPath.closeSubPath();

    g.setColour(accent.withAlpha(0.18f));
    g.fillPath(fillPath);
    g.setColour(accent);
    g.strokePath(linePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
