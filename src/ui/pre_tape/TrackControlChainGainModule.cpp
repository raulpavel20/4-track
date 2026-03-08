#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

namespace
{
constexpr int gainSliderHeight = 138;
constexpr int gainSliderWidth = 18;

juce::String formatDbValue(float value)
{
    return juce::String(value, 1) + " dB";
}

void drawVerticalSliderTrack(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto x = (float) bounds.getCentreX();
    const auto top = (float) bounds.getY() + 4.0f;
    const auto bottom = (float) bounds.getBottom() - 4.0f;
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawLine(x, top, x, bottom, 2.0f);
}
}

void TrackControlChain::layoutGainModule(int slot, juce::Rectangle<int> moduleArea)
{
    auto content = moduleArea;
    content.removeFromTop(24);
    gainModuleSliders[(size_t) slot].setBounds(content.getCentreX() - (gainSliderWidth / 2),
                                               content.getY() + 6,
                                               gainSliderWidth,
                                               gainSliderHeight);
}

void TrackControlChain::paintGainModule(juce::Graphics& g, int slot, juce::Rectangle<int> bounds)
{
    drawVerticalSliderTrack(g, gainModuleSliders[(size_t) slot].getBounds());
    g.setFont(AppFonts::getFont(12.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));
    g.drawText(formatDbValue((float) gainModuleSliders[(size_t) slot].getValue()),
               juce::Rectangle<int>(bounds.getX(),
                                    gainModuleSliders[(size_t) slot].getBounds().getBottom() + 4,
                                    bounds.getWidth(),
                                    16),
               juce::Justification::centred,
               false);
}
