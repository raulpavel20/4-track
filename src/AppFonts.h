#pragma once

#include <JuceHeader.h>
#include <FourTrackBinaryData.h>

namespace AppFonts
{
inline juce::Typeface::Ptr getTypeface()
{
    static auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::ManropeVariableFont_wght_ttf,
        BinaryData::ManropeVariableFont_wght_ttfSize);

    return typeface;
}

inline juce::Font getFont(float height)
{
    return juce::Font(juce::FontOptions(getTypeface()).withHeight(height));
}
}
