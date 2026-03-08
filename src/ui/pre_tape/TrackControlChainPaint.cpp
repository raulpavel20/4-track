#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int bypassCircleSize = 10;

juce::String getModuleTitle(ChainModuleType type)
{
    switch (type)
    {
        case ChainModuleType::filter:
            return "Filter";
        case ChainModuleType::eq:
            return "EQ";
        case ChainModuleType::compressor:
            return "Compressor";
        case ChainModuleType::saturation:
            return "Saturation";
        case ChainModuleType::delay:
            return "Delay";
        case ChainModuleType::reverb:
            return "Reverb";
        case ChainModuleType::gain:
            return "Utility";
        case ChainModuleType::none:
            break;
    }

    return {};
}
}

void TrackControlChain::paintContent(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    const auto accent = getAccentColour();

    if (showsInputModule())
        paintInputModule(g, accent, getInputModuleBounds());

    static const std::array<juce::String, compressorControlCount> compressorLabels { "Thr", "Rat", "Atk", "Rel", "Make" };

    for (int visibleIndex = 0; visibleIndex < visibleModuleCount; ++visibleIndex)
    {
        const auto slot = visibleSlots[(size_t) visibleIndex];
        const auto type = visibleTypes[(size_t) visibleIndex];
        const auto bounds = moduleBounds[(size_t) visibleIndex];
        auto titleBounds = bounds.withTrimmedTop(10).removeFromTop(18).reduced(12, 0);
        titleBounds.removeFromLeft(bypassCircleSize + 4);
        titleBounds.removeFromRight(18 + 8);

        g.setColour(accent);
        g.drawRoundedRectangle(bounds.toFloat(), 14.0f, 2.0f);

        g.setColour(juce::Colours::white);
        g.setFont(AppFonts::getFont(16.0f));
        g.drawText(getModuleTitle(type), titleBounds, juce::Justification::centredLeft, false);

        if (type == ChainModuleType::filter)
            paintFilterModule(g, accent, slot, bounds);
        else if (type == ChainModuleType::eq)
            paintEqModule(g, slot);
        else if (type == ChainModuleType::compressor)
            paintCompressorModule(g, accent, slot, bounds);
        else if (type == ChainModuleType::saturation)
            paintSaturationModule(g, accent, slot, bounds);
        else if (type == ChainModuleType::delay)
            paintDelayModule(g, accent, slot, bounds);
        else if (type == ChainModuleType::reverb)
            paintReverbModule(g, accent, slot, bounds);
        else if (type == ChainModuleType::gain)
            paintGainModule(g, slot, bounds);
    }

    const auto addBounds = getAddButtonBounds().toFloat();
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(addBounds, 10.0f, 1.0f);
}
