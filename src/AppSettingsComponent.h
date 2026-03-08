#pragma once

#include <JuceHeader.h>

#include "TapeEngine.h"
#include "ui/settings/sections/ExportSettingsSection.h"
#include "ui/settings/sections/TechnicalSettingsSection.h"
#include "ui/settings/sections/UserSettingsSection.h"

class AppSettingsComponent : public juce::Component
{
public:
    AppSettingsComponent(juce::AudioDeviceManager& audioDeviceManagerToUse, TapeEngine& engineToUse);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    UserSettingsSection userSettingsSection;
    ExportSettingsSection exportSettingsSection;
    TechnicalSettingsSection technicalSettingsSection;
    juce::Rectangle<int> getContentBounds() const;
};
