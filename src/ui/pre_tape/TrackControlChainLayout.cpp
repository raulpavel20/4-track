#include "../../TrackControlChain.h"

namespace
{
constexpr int moduleGap = 10;
constexpr int inputModuleWidth = 120;
constexpr int filterModuleWidth = 166;
constexpr int eqModuleWidth = 280;
constexpr int compressorModuleWidth = 260;
constexpr int saturationModuleWidth = 140;
constexpr int delayModuleWidth = 214;
constexpr int reverbModuleWidth = 214;
constexpr int phaserModuleWidth = 290;
constexpr int spectrumAnalyzerModuleWidth = 224;
constexpr int gainModuleWidth = 100;
constexpr int moduleVerticalInset = 10;
constexpr int moduleInnerPadding = 10;
constexpr int addButtonSize = 32;
constexpr int closeButtonSize = 18;
constexpr int bypassCircleSize = 10;

int getModuleWidth(ChainModuleType type)
{
    switch (type)
    {
        case ChainModuleType::filter:
            return filterModuleWidth;
        case ChainModuleType::eq:
            return eqModuleWidth;
        case ChainModuleType::compressor:
            return compressorModuleWidth;
        case ChainModuleType::saturation:
            return saturationModuleWidth;
        case ChainModuleType::delay:
            return delayModuleWidth;
        case ChainModuleType::reverb:
            return reverbModuleWidth;
        case ChainModuleType::phaser:
            return phaserModuleWidth;
        case ChainModuleType::spectrumAnalyzer:
            return spectrumAnalyzerModuleWidth;
        case ChainModuleType::gain:
            return gainModuleWidth;
        case ChainModuleType::none:
            break;
    }

    return filterModuleWidth;
}
}

void TrackControlChain::layoutContent()
{
    const auto viewportBounds = modulesViewport.getBounds();
    const auto contentHeight = juce::jmax(1, viewportBounds.getHeight());

    auto x = 0;
    const auto inputWidth = showsInputModule() ? inputModuleWidth : 0;
    const auto inputBounds = juce::Rectangle<int>(x, 0, inputWidth, contentHeight);

    if (showsInputModule())
        x += inputBounds.getWidth() + moduleGap;

    for (int visibleIndex = 0; visibleIndex < visibleModuleCount; ++visibleIndex)
    {
        const auto width = getModuleWidth(visibleTypes[(size_t) visibleIndex]);
        moduleBounds[(size_t) visibleIndex] = juce::Rectangle<int>(x, 0, width, contentHeight);
        x += width + moduleGap;
    }

    const auto addBounds = juce::Rectangle<int>(x, (contentHeight - addButtonSize) / 2, addButtonSize, addButtonSize);
    const auto requiredWidth = addBounds.getRight();
    contentComponent.setSize(juce::jmax(viewportBounds.getWidth(), requiredWidth), contentHeight);

    if (showsInputModule())
        layoutInputModule(inputBounds);

    for (int visibleIndex = 0; visibleIndex < visibleModuleCount; ++visibleIndex)
    {
        const auto slot = visibleSlots[(size_t) visibleIndex];
        const auto type = visibleTypes[(size_t) visibleIndex];
        auto moduleArea = moduleBounds[(size_t) visibleIndex].reduced(moduleInnerPadding, moduleVerticalInset);

        const auto titleBarCentreY = moduleArea.getY() + 9;
        const auto bypassY = titleBarCentreY - (bypassCircleSize / 2);
        bypassButtons[(size_t) slot].setBounds(moduleArea.getX(),
                                               bypassY,
                                               bypassCircleSize,
                                               bypassCircleSize);
        removeButtons[(size_t) slot].setBounds(moduleArea.getRight() - closeButtonSize,
                                               moduleArea.getY() + 1,
                                               closeButtonSize,
                                               closeButtonSize);

        if (type == ChainModuleType::filter)
            layoutFilterModule(slot, moduleArea);
        else if (type == ChainModuleType::eq)
            layoutEqModule(slot, moduleArea);
        else if (type == ChainModuleType::compressor)
            layoutCompressorModule(slot, moduleArea);
        else if (type == ChainModuleType::saturation)
            layoutSaturationModule(slot, moduleArea);
        else if (type == ChainModuleType::delay)
            layoutDelayModule(slot, moduleArea);
        else if (type == ChainModuleType::reverb)
            layoutReverbModule(slot, moduleArea);
        else if (type == ChainModuleType::phaser)
            layoutPhaserModule(slot, moduleArea);
        else if (type == ChainModuleType::spectrumAnalyzer)
            layoutSpectrumAnalyzerModule(slot, moduleArea);
        else if (type == ChainModuleType::gain)
            layoutGainModule(slot, moduleArea);
    }

    addModuleButton.setBounds(addBounds);
}

juce::Rectangle<int> TrackControlChain::getFrameBounds() const
{
    return getLocalBounds().reduced(6);
}

juce::Rectangle<int> TrackControlChain::getViewportBounds() const
{
    return getFrameBounds().reduced(8);
}

juce::Rectangle<int> TrackControlChain::getInputModuleBounds() const
{
    return juce::Rectangle<int>(0, 0, showsInputModule() ? inputModuleWidth : 0, contentComponent.getHeight());
}

juce::Rectangle<int> TrackControlChain::getAddButtonBounds() const
{
    auto x = 0;

    if (showsInputModule())
        x = inputModuleWidth + moduleGap;

    for (int visibleIndex = 0; visibleIndex < visibleModuleCount; ++visibleIndex)
        x += getModuleWidth(visibleTypes[(size_t) visibleIndex]) + moduleGap;

    return juce::Rectangle<int>(x, (contentComponent.getHeight() - addButtonSize) / 2, addButtonSize, addButtonSize);
}
