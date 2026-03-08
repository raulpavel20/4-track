#include "../../TrackMixerPanel.h"

#include "../../AppFonts.h"
#include "../../AppSettings.h"

#include <cmath>

namespace
{
constexpr int moduleInnerPadding = 10;
constexpr int moduleVerticalInset = 10;
constexpr int gainSliderHeight = 110;
constexpr int meterWidth = 10;

juce::Colour getTrackColour(int index)
{
    return AppSettings::getInstance().getTrackColour(juce::jlimit(0, TapeEngine::numTracks - 1, index));
}

void drawVerticalSliderTrack(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto x = (float) bounds.getCentreX();
    const auto top = (float) bounds.getY() + 4.0f;
    const auto bottom = (float) bounds.getBottom() - 4.0f;
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawLine(x, top, x, bottom, 2.0f);
}

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

juce::String formatPercentValue(float value)
{
    return juce::String((int) std::round(value * 100.0f)) + "%";
}

juce::String formatSampleRateValue(double value)
{
    if (value >= 1000.0)
        return juce::String(value / 1000.0, std::abs(std::round(value / 1000.0) - (value / 1000.0)) < 0.01 ? 0 : 1) + " kHz";

    return juce::String((int) std::round(value)) + " Hz";
}
}

void TrackMixerPanel::paintContent(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        const auto bounds = trackModuleBounds[(size_t) trackIndex];
        const auto colour = getTrackColour(trackIndex);
        const auto meterValue = juce::jlimit(0.0f, 1.0f, engine.getTrackMixerMeter(trackIndex));

        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 14.0f, 1.0f);

        const auto content = bounds.reduced(moduleInnerPadding, moduleVerticalInset);
        const auto top = content.getY();
        const auto meterBounds = juce::Rectangle<int>(content.getCentreX() - 18,
                                                      top + 6,
                                                      meterWidth,
                                                      gainSliderHeight);
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawLine((float) meterBounds.getCentreX(),
                   (float) meterBounds.getY() + 4.0f,
                   (float) meterBounds.getCentreX(),
                   (float) meterBounds.getBottom() - 4.0f,
                   2.0f);

        auto filledBounds = meterBounds.reduced(2);
        filledBounds.removeFromBottom(juce::roundToInt((1.0f - meterValue) * (float) filledBounds.getHeight()));
        g.setColour(colour);
        g.fillRoundedRectangle(filledBounds.toFloat(), 3.0f);

        drawVerticalSliderTrack(g, gainSliders[(size_t) trackIndex].getBounds());

        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(AppFonts::getFont(11.0f));
        g.drawText(formatDbValue((float) gainSliders[(size_t) trackIndex].getValue()),
                   bounds.getX(),
                   content.getY() + gainSliderHeight + 6,
                   bounds.getWidth(),
                   14,
                   juce::Justification::centred);
        g.drawText(formatPanValue((float) panSliders[(size_t) trackIndex].getValue()),
                   bounds.getX(),
                   content.getBottom() - 12,
                   bounds.getWidth(),
                   14,
                   juce::Justification::centred);
    }

    auto paintFxModule = [&] (juce::Rectangle<int> bounds,
                              const juce::String& title,
                              const std::array<juce::Slider, TapeEngine::numTracks>& sendSliders,
                              const std::array<juce::String, 3>& controlLabels,
                              const std::array<juce::String, 3>& controlValues)
    {
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 14.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(AppFonts::getFont(12.0f));
        g.drawText(title,
                   bounds.getX(),
                   bounds.getY() + 6,
                   bounds.getWidth(),
                   16,
                   juce::Justification::centred);

        auto content = bounds.reduced(moduleInnerPadding, moduleVerticalInset);
        content.removeFromTop(24);
        auto sendRow = content.removeFromTop(62);
        const auto sendCellWidth = sendRow.getWidth() / TapeEngine::numTracks;

        for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
        {
            auto cell = sendRow.removeFromLeft(trackIndex == TapeEngine::numTracks - 1 ? sendRow.getWidth() : sendCellWidth);
            const auto colour = getTrackColour(trackIndex);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.setFont(AppFonts::getFont(10.0f));
            g.drawText(juce::String(trackIndex + 1),
                       cell.getX(),
                       cell.getY(),
                       cell.getWidth(),
                       12,
                       juce::Justification::centred);
            g.setColour(colour);
            g.drawText(formatPercentValue((float) sendSliders[(size_t) trackIndex].getValue()),
                       cell.getX(),
                       cell.getBottom() - 14,
                       cell.getWidth(),
                       12,
                       juce::Justification::centred);
        }

        content.removeFromTop(8);
        const auto controlCellWidth = content.getWidth() / 3;

        for (int controlIndex = 0; controlIndex < 3; ++controlIndex)
        {
            auto cell = content.removeFromLeft(controlIndex == 2 ? content.getWidth() : controlCellWidth);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.setFont(AppFonts::getFont(10.0f));
            g.drawText(controlLabels[(size_t) controlIndex],
                       cell.getX(),
                       cell.getY() + 10,
                       cell.getWidth(),
                       12,
                       juce::Justification::centred);
            if (! (title == "DELAY" && controlIndex == 0))
                g.drawText(controlValues[(size_t) controlIndex],
                           cell.getX(),
                           cell.getBottom() - 14,
                           cell.getWidth(),
                           12,
                           juce::Justification::centred);
        }
    };

    paintFxModule(delayModuleBounds,
                  "DELAY",
                  delaySendSliders,
                  { "TIME", "FDBK", "MIX" },
                  { delayTimeModeButton.getButtonText(),
                    formatPercentValue((float) delayControlSliders[1].getValue()),
                    formatPercentValue((float) delayControlSliders[2].getValue()) });

    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(exportModuleBounds.toFloat().reduced(0.5f), 14.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(12.0f));
    g.drawText("EXPORT",
               exportModuleBounds.getX(),
               exportModuleBounds.getY() + 6,
               exportModuleBounds.getWidth(),
               16,
               juce::Justification::centred);

    static const std::array<juce::String, 4> exportLabels { "FORMAT", "RATE", "DEPTH", "TAIL" };
    const std::array<juce::String, 4> exportValues
    {
        exportFormatBox.getText(),
        formatSampleRateValue(exportSampleRateBox.getSelectedId() == 2 ? 48000.0 : 44100.0),
        exportBitDepthBox.getText(),
        exportTailBox.getText()
    };

    auto exportContent = exportModuleBounds.reduced(moduleInnerPadding, moduleVerticalInset);
    exportContent.removeFromTop(28);
    const auto rowHeight = 56;
    auto topRow = exportContent.removeFromTop(rowHeight);
    auto bottomRow = exportContent.removeFromTop(rowHeight);
    const auto cellWidth = exportContent.getWidth() / 2;

    for (int index = 0; index < 4; ++index)
    {
        auto& row = index < 2 ? topRow : bottomRow;
        auto cell = row.removeFromLeft(index % 2 == 0 ? cellWidth : row.getWidth());
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(AppFonts::getFont(10.0f));
        g.drawText(exportLabels[(size_t) index],
                   cell.getX(),
                   cell.getY() + 2,
                   cell.getWidth(),
                   12,
                   juce::Justification::centred);
        g.drawText(exportValues[(size_t) index],
                   cell.getX(),
                   cell.getBottom() - 14,
                   cell.getWidth(),
                   12,
                   juce::Justification::centred);
    }
}
