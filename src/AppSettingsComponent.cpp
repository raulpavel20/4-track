#include "AppSettingsComponent.h"

#include "AppFonts.h"

AppSettingsComponent::AppSettingsComponent(juce::AudioDeviceManager& audioDeviceManagerToUse, TapeEngine& engineToUse)
    : userSettingsSection(engineToUse),
      technicalSettingsSection(audioDeviceManagerToUse)
{
    setSize(860, 784);
    addAndMakeVisible(userSettingsSection);
    addAndMakeVisible(exportSettingsSection);
    addAndMakeVisible(technicalSettingsSection);
}

void AppSettingsComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    const auto contentBounds = getContentBounds();

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(24.0f));
    g.drawText("Settings", contentBounds.getX(), contentBounds.getY(), 180, 28, juce::Justification::centredLeft, false);
    g.setFont(AppFonts::getFont(14.0f));
    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.drawText("v" + juce::JUCEApplication::getInstance()->getApplicationVersion(),
               contentBounds.getRight() - 90, contentBounds.getY() + 2, 90, 20, juce::Justification::centredRight, false);
}

void AppSettingsComponent::resized()
{
    const auto contentBounds = getContentBounds();
    auto userBounds = juce::Rectangle<int>(contentBounds.getX(), contentBounds.getY() + 32, contentBounds.getWidth(), 252);
    auto exportBounds = juce::Rectangle<int>(contentBounds.getX(), userBounds.getBottom() + 18, contentBounds.getWidth(), 196);
    auto technicalBounds = juce::Rectangle<int>(contentBounds.getX(), exportBounds.getBottom() + 18, contentBounds.getWidth(), 196);
    userSettingsSection.setBounds(userBounds);
    exportSettingsSection.setBounds(exportBounds);
    technicalSettingsSection.setBounds(technicalBounds);
}

juce::Rectangle<int> AppSettingsComponent::getContentBounds() const
{
    return getLocalBounds().reduced(28, 24);
}
