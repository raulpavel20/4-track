#include "../../TrackMixerPanel.h"

namespace
{
constexpr int moduleGap = 10;
constexpr int trackModuleWidth = 83;
constexpr int exportModuleWidth = 232;
constexpr int moduleVerticalInset = 10;
constexpr int moduleInnerPadding = 10;
constexpr int gainSliderWidth = 20;
constexpr int gainSliderHeight = 110;
constexpr int panKnobSize = 54;
}

void TrackMixerPanel::layoutContent()
{
    const auto viewportBounds = modulesViewport.getBounds();
    const auto contentHeight = juce::jmax(1, viewportBounds.getHeight());

    auto x = 0;

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        trackModuleBounds[(size_t) trackIndex] = juce::Rectangle<int>(x, 0, trackModuleWidth, contentHeight);
        x += trackModuleWidth + moduleGap;
    }

    exportModuleBounds = juce::Rectangle<int>(x, 0, exportModuleWidth, contentHeight);
    const auto requiredWidth = exportModuleBounds.getRight();
    contentComponent.setSize(juce::jmax(viewportBounds.getWidth(), requiredWidth), contentHeight);

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        auto content = trackModuleBounds[(size_t) trackIndex].reduced(moduleInnerPadding, moduleVerticalInset);
        const auto top = content.getY();
        const auto meterX = content.getCentreX() - 18;
        gainSliders[(size_t) trackIndex].setBounds(meterX + 14,
                                                   top + 6,
                                                   gainSliderWidth,
                                                   gainSliderHeight);
        panSliders[(size_t) trackIndex].setBounds(content.getCentreX() - (panKnobSize / 2),
                                                  content.getBottom() - panKnobSize - 4,
                                                  panKnobSize,
                                                  panKnobSize);
    }

    auto exportContent = exportModuleBounds.reduced(moduleInnerPadding, moduleVerticalInset);
    exportContent.removeFromTop(28);
    auto topRow = exportContent.removeFromTop(56);
    auto bottomRow = exportContent.removeFromTop(56);
    const auto cellGap = 8;
    const auto cellWidth = (topRow.getWidth() - cellGap) / 2;
    const auto boxHeight = 28;
    const auto buttonWidth = 94;
    auto placeBox = [&] (juce::Rectangle<int> row, int columnIndex)
    {
        const auto xOffset = columnIndex == 0 ? 0 : cellWidth + cellGap;
        return juce::Rectangle<int>(row.getX() + xOffset, row.getY() + 16, cellWidth, boxHeight);
    };

    exportFormatBox.setBounds(placeBox(topRow, 0));
    exportSampleRateBox.setBounds(placeBox(topRow, 1));
    exportBitDepthBox.setBounds(placeBox(bottomRow, 0));
    exportTailBox.setBounds(placeBox(bottomRow, 1));
    exportButton.setBounds(exportModuleBounds.getCentreX() - (buttonWidth / 2),
                           exportModuleBounds.getBottom() - moduleVerticalInset - 38,
                           buttonWidth,
                           28);
}

juce::Rectangle<int> TrackMixerPanel::getFrameBounds() const
{
    return getLocalBounds().reduced(6);
}

juce::Rectangle<int> TrackMixerPanel::getViewportBounds() const
{
    return getFrameBounds().reduced(8);
}
