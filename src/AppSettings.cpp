#include "AppSettings.h"

namespace
{
const std::array<juce::Colour, 4> defaultTrackColours
{
    juce::Colour::fromRGB(255, 92, 92),
    juce::Colour::fromRGB(255, 184, 77),
    juce::Colour::fromRGB(94, 233, 196),
    juce::Colour::fromRGB(94, 146, 255)
};

juce::String getTrackColourKey(int trackIndex)
{
    return "trackColour" + juce::String(trackIndex);
}
}

AppSettings& AppSettings::getInstance()
{
    static AppSettings instance;
    return instance;
}

void AppSettings::initialise()
{
    if (initialised)
        return;

    juce::PropertiesFile::Options options;
    options.applicationName = "4-Track Tape";
    options.filenameSuffix = "settings";
    options.folderName = "4-Track Tape";
    options.osxLibrarySubFolder = "Application Support";
    options.commonToAllUsers = false;
    applicationProperties.setStorageParameters(options);
    initialised = true;
}

void AppSettings::shutdown()
{
    if (! initialised)
        return;

    applicationProperties.saveIfNeeded();
    initialised = false;
}

juce::Colour AppSettings::getTrackColour(int trackIndex) const
{
    const auto safeIndex = juce::jlimit(0, (int) defaultTrackColours.size() - 1, trackIndex);
    const auto* properties = getProperties();

    if (properties == nullptr)
        return defaultTrackColours[(size_t) safeIndex];

    const auto storedValue = properties->getIntValue(getTrackColourKey(safeIndex),
                                                     (int) defaultTrackColours[(size_t) safeIndex].getARGB());
    return juce::Colour((juce::uint32) storedValue);
}

void AppSettings::setTrackColour(int trackIndex, juce::Colour colour)
{
    const auto safeIndex = juce::jlimit(0, (int) defaultTrackColours.size() - 1, trackIndex);
    setValue(getTrackColourKey(safeIndex), (int) colour.getARGB());
}

int AppSettings::getDefaultBpm() const
{
    const auto* properties = getProperties();
    return properties != nullptr ? properties->getIntValue("defaultBpm", 120) : 120;
}

void AppSettings::setDefaultBpm(int bpm)
{
    setValue("defaultBpm", juce::jlimit(30, 300, bpm));
}

int AppSettings::getDefaultBeatsPerBar() const
{
    const auto* properties = getProperties();
    return properties != nullptr ? properties->getIntValue("defaultBeatsPerBar", 4) : 4;
}

void AppSettings::setDefaultBeatsPerBar(int beatsPerBar)
{
    setValue("defaultBeatsPerBar", juce::jlimit(1, 16, beatsPerBar));
}

bool AppSettings::getDefaultMetronomeEnabled() const
{
    const auto* properties = getProperties();
    return properties != nullptr ? properties->getBoolValue("defaultMetronomeEnabled", false) : false;
}

void AppSettings::setDefaultMetronomeEnabled(bool shouldBeEnabled)
{
    setValue("defaultMetronomeEnabled", shouldBeEnabled);
}

bool AppSettings::getDefaultCountInEnabled() const
{
    const auto* properties = getProperties();
    return properties != nullptr ? properties->getBoolValue("defaultCountInEnabled", true) : true;
}

void AppSettings::setDefaultCountInEnabled(bool shouldBeEnabled)
{
    setValue("defaultCountInEnabled", shouldBeEnabled);
}

AppSettings::ExportDefaults AppSettings::getExportDefaults() const
{
    const auto* properties = getProperties();
    ExportDefaults defaults;

    if (properties == nullptr)
        return defaults;

    defaults.formatId = properties->getIntValue("exportFormatId", defaults.formatId);
    defaults.sampleRate = properties->getDoubleValue("exportSampleRate", defaults.sampleRate);
    defaults.bitDepth = properties->getIntValue("exportBitDepth", defaults.bitDepth);
    defaults.tailSeconds = properties->getDoubleValue("exportTailSeconds", defaults.tailSeconds);
    return defaults;
}

void AppSettings::setExportDefaults(const ExportDefaults& defaults)
{
    const auto safeFormatId = defaults.formatId == 2 ? 2 : 1;
    const auto safeSampleRate = std::abs(defaults.sampleRate - 48000.0) < 1.0 ? 48000.0 : 44100.0;
    const auto safeBitDepth = defaults.bitDepth == 16 ? 16 : 24;
    const auto safeTailSeconds = defaults.tailSeconds == 1.0 || defaults.tailSeconds == 2.0
                                     || defaults.tailSeconds == 4.0 || defaults.tailSeconds == 8.0
                                     ? defaults.tailSeconds
                                     : 2.0;

    auto* properties = getProperties();

    if (properties == nullptr)
        return;

    properties->setValue("exportFormatId", safeFormatId);
    properties->setValue("exportSampleRate", safeSampleRate);
    properties->setValue("exportBitDepth", safeBitDepth);
    properties->setValue("exportTailSeconds", safeTailSeconds);
    properties->saveIfNeeded();
    sendChangeMessage();
}

std::unique_ptr<juce::XmlElement> AppSettings::createAudioDeviceState() const
{
    const auto* properties = getProperties();

    if (properties == nullptr)
        return {};

    const auto xmlText = properties->getValue("audioDeviceState");

    if (xmlText.isEmpty())
        return {};

    return juce::parseXML(xmlText);
}

void AppSettings::setAudioDeviceState(const juce::XmlElement* state)
{
    auto* properties = getProperties();

    if (properties == nullptr)
        return;

    properties->setValue("audioDeviceState", state != nullptr ? state->toString() : juce::String());
    properties->saveIfNeeded();
    sendChangeMessage();
}

juce::PropertiesFile* AppSettings::getProperties() const
{
    if (! initialised)
        return nullptr;

    return applicationProperties.getUserSettings();
}

void AppSettings::setValue(const juce::String& key, const juce::var& value)
{
    auto* properties = getProperties();

    if (properties == nullptr)
        return;

    properties->setValue(key, value);
    properties->saveIfNeeded();
    sendChangeMessage();
}
