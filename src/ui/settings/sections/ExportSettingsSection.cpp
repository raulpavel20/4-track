#include "ExportSettingsSection.h"

#include "../../../AppFonts.h"
#include "../../../AppSettings.h"

ExportSettingsSection::ExportSettingsSection()
{
    AppSettings::getInstance().addChangeListener(this);

    for (auto* box : { &exportFormatBox, &exportSampleRateBox, &exportBitDepthBox, &exportTailBox })
    {
        configureComboBox(*box);
        addAndMakeVisible(*box);
    }

    exportFormatBox.addItem("WAV", 1);
    exportFormatBox.addItem("AIFF", 2);
    exportSampleRateBox.addItem("44.1 kHz", 1);
    exportSampleRateBox.addItem("48 kHz", 2);
    exportBitDepthBox.addItem("16-bit", 1);
    exportBitDepthBox.addItem("24-bit", 2);
    exportTailBox.addItem("1 sec", 1);
    exportTailBox.addItem("2 sec", 2);
    exportTailBox.addItem("4 sec", 3);
    exportTailBox.addItem("8 sec", 4);

    exportFormatBox.onChange = [this] { persistDefaults(); };
    exportSampleRateBox.onChange = [this] { persistDefaults(); };
    exportBitDepthBox.onChange = [this] { persistDefaults(); };
    exportTailBox.onChange = [this] { persistDefaults(); };

    refreshFromSettings();
}

ExportSettingsSection::~ExportSettingsSection()
{
    AppSettings::getInstance().removeChangeListener(this);
}

void ExportSettingsSection::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 14.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(17.0f));
    g.drawText("Export", juce::Rectangle<int>(20, 12, getWidth() - 40, 20), juce::Justification::centredLeft, false);

    g.setFont(AppFonts::getFont(13.0f));
    g.setColour(juce::Colours::white.withAlpha(0.62f));
    const auto leftX = 24;
    const auto columnGap = 36;
    const auto columnWidth = (getWidth() - (leftX * 2) - columnGap) / 2;
    const auto rightX = leftX + columnWidth + columnGap;
    g.drawText("Format", juce::Rectangle<int>(leftX, 54, columnWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Sample Rate", juce::Rectangle<int>(rightX, 54, columnWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Bit Depth", juce::Rectangle<int>(leftX, 122, columnWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Tail", juce::Rectangle<int>(rightX, 122, columnWidth, 16), juce::Justification::centredLeft, false);
}

void ExportSettingsSection::resized()
{
    const auto leftX = 24;
    const auto columnGap = 36;
    const auto columnWidth = (getWidth() - (leftX * 2) - columnGap) / 2;
    const auto rightX = leftX + columnWidth + columnGap;
    exportFormatBox.setBounds(leftX, 76, columnWidth, 32);
    exportSampleRateBox.setBounds(rightX, 76, columnWidth, 32);
    exportBitDepthBox.setBounds(leftX, 144, columnWidth, 32);
    exportTailBox.setBounds(rightX, 144, columnWidth, 32);
}

void ExportSettingsSection::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &AppSettings::getInstance())
        refreshFromSettings();
}

void ExportSettingsSection::configureComboBox(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    box.setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    box.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    box.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
}

void ExportSettingsSection::refreshFromSettings()
{
    const auto defaults = AppSettings::getInstance().getExportDefaults();
    exportFormatBox.setSelectedId(defaults.formatId == 2 ? 2 : 1, juce::dontSendNotification);
    exportSampleRateBox.setSelectedId(std::abs(defaults.sampleRate - 48000.0) < 1.0 ? 2 : 1, juce::dontSendNotification);
    exportBitDepthBox.setSelectedId(defaults.bitDepth == 16 ? 1 : 2, juce::dontSendNotification);
    exportTailBox.setSelectedId(defaults.tailSeconds == 1.0 ? 1
                                     : defaults.tailSeconds == 4.0 ? 3
                                     : defaults.tailSeconds == 8.0 ? 4
                                                                    : 2,
                                juce::dontSendNotification);
}

void ExportSettingsSection::persistDefaults()
{
    AppSettings::ExportDefaults defaults;
    defaults.formatId = exportFormatBox.getSelectedId() == 2 ? 2 : 1;
    defaults.sampleRate = exportSampleRateBox.getSelectedId() == 2 ? 48000.0 : 44100.0;
    defaults.bitDepth = exportBitDepthBox.getSelectedId() == 1 ? 16 : 24;
    defaults.tailSeconds = exportTailBox.getSelectedId() == 1 ? 1.0
                           : exportTailBox.getSelectedId() == 3 ? 4.0
                           : exportTailBox.getSelectedId() == 4 ? 8.0
                                                                : 2.0;
    AppSettings::getInstance().setExportDefaults(defaults);
}
