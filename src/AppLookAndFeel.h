#pragma once

#include <JuceHeader.h>

#include "AppFonts.h"

class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override
    {
        return AppFonts::getTypeface();
    }
};
