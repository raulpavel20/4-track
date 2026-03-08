#include "TechnicalSettingsSection.h"

#include "../../../AppFonts.h"

#include <cmath>

namespace
{
juce::String formatSampleRate(double value)
{
    if (value >= 1000.0)
        return juce::String(value / 1000.0, std::abs((value / 1000.0) - std::round(value / 1000.0)) < 0.01 ? 0 : 1) + " kHz";

    return juce::String((int) std::round(value)) + " Hz";
}

double parseSampleRate(const juce::String& text)
{
    auto cleaned = text.retainCharacters("0123456789.");

    if (cleaned.isEmpty())
        return 0.0;

    auto value = cleaned.getDoubleValue();

    if (text.containsIgnoreCase("khz"))
        value *= 1000.0;

    return value;
}

int parseBufferSize(const juce::String& text)
{
    return text.retainCharacters("0123456789").getIntValue();
}
}

TechnicalSettingsSection::TechnicalSettingsSection(juce::AudioDeviceManager& audioDeviceManagerToUse)
    : audioDeviceManager(audioDeviceManagerToUse)
{
    audioDeviceManager.addChangeListener(this);

    for (auto* box : { &inputDeviceBox, &outputDeviceBox, &sampleRateBox, &bufferSizeBox })
    {
        configureComboBox(*box);
        addAndMakeVisible(*box);
    }

    auto audioChange = [this]
    {
        if (! refreshingAudioControls)
            applyAudioSetup();
    };

    inputDeviceBox.onChange = audioChange;
    outputDeviceBox.onChange = audioChange;
    sampleRateBox.onChange = audioChange;
    bufferSizeBox.onChange = audioChange;

    refreshAudioOptions();
}

TechnicalSettingsSection::~TechnicalSettingsSection()
{
    audioDeviceManager.removeChangeListener(this);
}

void TechnicalSettingsSection::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 14.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(17.0f));
    g.drawText("Technical", juce::Rectangle<int>(20, 12, getWidth() - 40, 20), juce::Justification::centredLeft, false);

    g.setFont(AppFonts::getFont(13.0f));
    g.setColour(juce::Colours::white.withAlpha(0.62f));
    const auto leftX = 24;
    const auto columnGap = 36;
    const auto columnWidth = (getWidth() - (leftX * 2) - columnGap) / 2;
    const auto rightX = leftX + columnWidth + columnGap;
    g.drawText("Input Device", juce::Rectangle<int>(leftX, 54, columnWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Output Device", juce::Rectangle<int>(rightX, 54, columnWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Sample Rate", juce::Rectangle<int>(leftX, 122, columnWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Buffer Size", juce::Rectangle<int>(rightX, 122, columnWidth, 16), juce::Justification::centredLeft, false);
}

void TechnicalSettingsSection::resized()
{
    const auto leftX = 24;
    const auto columnGap = 36;
    const auto columnWidth = (getWidth() - (leftX * 2) - columnGap) / 2;
    const auto rightX = leftX + columnWidth + columnGap;
    inputDeviceBox.setBounds(leftX, 76, columnWidth, 32);
    outputDeviceBox.setBounds(rightX, 76, columnWidth, 32);
    sampleRateBox.setBounds(leftX, 144, columnWidth, 32);
    bufferSizeBox.setBounds(rightX, 144, columnWidth, 32);
}

void TechnicalSettingsSection::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioDeviceManager)
        refreshAudioOptions();
}

void TechnicalSettingsSection::configureComboBox(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    box.setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    box.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    box.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
}

void TechnicalSettingsSection::refreshAudioOptions()
{
    refreshingAudioControls = true;

    const auto& availableTypes = audioDeviceManager.getAvailableDeviceTypes();
    juce::StringArray inputDeviceNames;
    juce::StringArray outputDeviceNames;

    for (auto* type : availableTypes)
    {
        inputDeviceNames.addArray(type->getDeviceNames(true));
        outputDeviceNames.addArray(type->getDeviceNames(false));
    }

    const auto setup = audioDeviceManager.getAudioDeviceSetup();

    inputDeviceBox.clear(juce::dontSendNotification);
    outputDeviceBox.clear(juce::dontSendNotification);
    sampleRateBox.clear(juce::dontSendNotification);
    bufferSizeBox.clear(juce::dontSendNotification);

    for (int index = 0; index < inputDeviceNames.size(); ++index)
        inputDeviceBox.addItem(inputDeviceNames[index], index + 1);

    for (int index = 0; index < outputDeviceNames.size(); ++index)
        outputDeviceBox.addItem(outputDeviceNames[index], index + 1);

    inputDeviceBox.setText(setup.inputDeviceName, juce::dontSendNotification);
    outputDeviceBox.setText(setup.outputDeviceName, juce::dontSendNotification);

    if (auto* device = audioDeviceManager.getCurrentAudioDevice())
    {
        const auto sampleRates = device->getAvailableSampleRates();
        const auto bufferSizes = device->getAvailableBufferSizes();

        for (int index = 0; index < sampleRates.size(); ++index)
            sampleRateBox.addItem(formatSampleRate(sampleRates[index]), index + 1);

        for (int index = 0; index < bufferSizes.size(); ++index)
            bufferSizeBox.addItem(juce::String(bufferSizes[index]) + " samples", index + 1);

        sampleRateBox.setText(formatSampleRate(device->getCurrentSampleRate()), juce::dontSendNotification);
        bufferSizeBox.setText(juce::String(device->getCurrentBufferSizeSamples()) + " samples", juce::dontSendNotification);
    }

    refreshingAudioControls = false;
}

void TechnicalSettingsSection::applyAudioSetup()
{
    auto setup = audioDeviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = inputDeviceBox.getText();
    setup.outputDeviceName = outputDeviceBox.getText();

    const auto selectedSampleRate = parseSampleRate(sampleRateBox.getText());
    const auto selectedBufferSize = parseBufferSize(bufferSizeBox.getText());

    if (selectedSampleRate > 0.0)
        setup.sampleRate = selectedSampleRate;

    if (selectedBufferSize > 0)
        setup.bufferSize = selectedBufferSize;

    audioDeviceManager.setAudioDeviceSetup(setup, true);
}
