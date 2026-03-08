#include "AppSettingsComponent.h"

#include "AppFonts.h"
#include "AppSettings.h"

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

AppSettingsComponent::ColourButton::ColourButton()
    : juce::Button({})
{
}

void AppSettingsComponent::ColourButton::setButtonColour(juce::Colour newColour)
{
    colour = newColour;
    repaint();
}

juce::Colour AppSettingsComponent::ColourButton::getButtonColour() const noexcept
{
    return colour;
}

void AppSettingsComponent::ColourButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    auto fill = colour;

    if (isButtonDown)
        fill = fill.brighter(0.08f);
    else if (isMouseOverButton)
        fill = fill.brighter(0.04f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
}

AppSettingsComponent::ToggleButton::ToggleButton(const juce::String& textToUse)
    : juce::Button({}),
      text(textToUse)
{
    setClickingTogglesState(true);
}

void AppSettingsComponent::ToggleButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    auto background = getToggleState() ? juce::Colours::white : juce::Colours::black;

    if (isButtonDown)
        background = getToggleState() ? background.darker(0.08f) : background.brighter(0.12f);
    else if (isMouseOverButton)
        background = getToggleState() ? background.darker(0.04f) : background.brighter(0.06f);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(getToggleState() ? juce::Colours::white : juce::Colours::white.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    g.setColour(getToggleState() ? juce::Colours::black : juce::Colours::white);
    g.setFont(AppFonts::getFont(12.0f));
    g.drawFittedText(text, getLocalBounds().reduced(10, 0), juce::Justification::centred, 1);
}

AppSettingsComponent::AppSettingsComponent(juce::AudioDeviceManager& audioDeviceManagerToUse, TapeEngine& engineToUse)
    : audioDeviceManager(audioDeviceManagerToUse),
      engine(engineToUse)
{
    setSize(860, 720);

    AppSettings::getInstance().addChangeListener(this);
    audioDeviceManager.addChangeListener(this);

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        auto& button = trackColourButtons[(size_t) trackIndex];
        button.onClick = [this, trackIndex] { openColourSelector(trackIndex); };
        addAndMakeVisible(button);
    }

    configureLabelEditor(bpmEditor, 3);
    bpmEditor.onTextChange = [this]
    {
        const auto numericText = bpmEditor.getText().retainCharacters("0123456789");

        if (numericText.isEmpty())
            return;

        const auto bpm = juce::jlimit(30, 300, numericText.getIntValue());
        AppSettings::getInstance().setDefaultBpm(bpm);
        engine.setBpm((float) bpm);
    };
    addAndMakeVisible(bpmEditor);

    configureLabelEditor(beatsPerBarEditor, 2);
    beatsPerBarEditor.onTextChange = [this]
    {
        const auto numericText = beatsPerBarEditor.getText().retainCharacters("0123456789");

        if (numericText.isEmpty())
            return;

        const auto beatsPerBar = juce::jlimit(1, 16, numericText.getIntValue());
        AppSettings::getInstance().setDefaultBeatsPerBar(beatsPerBar);
        engine.setBeatsPerBar(beatsPerBar);
    };
    addAndMakeVisible(beatsPerBarEditor);

    configureToggleButton(metronomeDefaultButton);
    metronomeDefaultButton.onClick = [this]
    {
        const auto enabled = metronomeDefaultButton.getToggleState();
        AppSettings::getInstance().setDefaultMetronomeEnabled(enabled);
        engine.setMetronomeEnabled(enabled);
    };
    addAndMakeVisible(metronomeDefaultButton);

    configureToggleButton(countInDefaultButton);
    countInDefaultButton.onClick = [this]
    {
        const auto enabled = countInDefaultButton.getToggleState();
        AppSettings::getInstance().setDefaultCountInEnabled(enabled);
        engine.setCountInEnabled(enabled);
    };
    addAndMakeVisible(countInDefaultButton);

    for (auto* box : { &exportFormatBox, &exportSampleRateBox, &exportBitDepthBox, &exportTailBox,
                       &inputDeviceBox, &outputDeviceBox, &sampleRateBox, &bufferSizeBox })
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

    auto exportChanged = [this]
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
    };

    exportFormatBox.onChange = exportChanged;
    exportSampleRateBox.onChange = exportChanged;
    exportBitDepthBox.onChange = exportChanged;
    exportTailBox.onChange = exportChanged;

    auto audioChange = [this]
    {
        if (! refreshingAudioControls)
            applyAudioSetup();
    };

    inputDeviceBox.onChange = audioChange;
    outputDeviceBox.onChange = audioChange;
    sampleRateBox.onChange = audioChange;
    bufferSizeBox.onChange = audioChange;

    refreshFromSettings();
    refreshAudioOptions();
}

AppSettingsComponent::~AppSettingsComponent()
{
    AppSettings::getInstance().removeChangeListener(this);
    audioDeviceManager.removeChangeListener(this);
}

void AppSettingsComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    const auto contentBounds = getContentBounds();
    auto userBounds = juce::Rectangle<int>(contentBounds.getX(), contentBounds.getY() + 32, contentBounds.getWidth(), 272);
    auto exportBounds = juce::Rectangle<int>(contentBounds.getX(), userBounds.getBottom() + 18, contentBounds.getWidth(), 154);
    auto technicalBounds = juce::Rectangle<int>(contentBounds.getX(), exportBounds.getBottom() + 18,
                                                contentBounds.getWidth(), contentBounds.getBottom() - exportBounds.getBottom() - 18);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(24.0f));
    g.drawText("Settings", contentBounds.getX(), contentBounds.getY(), 180, 28, juce::Justification::centredLeft, false);
    g.setFont(AppFonts::getFont(14.0f));
    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.drawText("v" + juce::JUCEApplication::getInstance()->getApplicationVersion(),
               contentBounds.getRight() - 90, contentBounds.getY() + 2, 90, 20, juce::Justification::centredRight, false);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(16.0f));
    g.drawText("User", userBounds.getX() + 16, userBounds.getY() + 12, 120, 20, juce::Justification::centredLeft, false);
    g.drawText("Export", exportBounds.getX() + 16, exportBounds.getY() + 12, 120, 20, juce::Justification::centredLeft, false);
    g.drawText("Technical", technicalBounds.getX() + 16, technicalBounds.getY() + 12, 120, 20, juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(userBounds.toFloat().reduced(0.5f), 12.0f, 1.0f);
    g.drawRoundedRectangle(exportBounds.toFloat().reduced(0.5f), 12.0f, 1.0f);
    g.drawRoundedRectangle(technicalBounds.toFloat().reduced(0.5f), 12.0f, 1.0f);

    g.setFont(AppFonts::getFont(12.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));

    auto swatchArea = userBounds.withTrimmedTop(42).removeFromTop(28).reduced(16, 0);
    const auto swatchGap = 12;
    const auto swatchWidth = (swatchArea.getWidth() - (swatchGap * (TapeEngine::numTracks - 1))) / TapeEngine::numTracks;

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        const auto x = swatchArea.getX() + (trackIndex * (swatchWidth + swatchGap));
        g.drawText("Track " + juce::String(trackIndex + 1),
                   x, swatchArea.getY(), swatchWidth, 14, juce::Justification::centred, false);
    }

    auto settingsArea = userBounds.withTrimmedTop(138).reduced(16, 12);
    const auto columnGap = 14;
    const auto columnWidth = (settingsArea.getWidth() - columnGap) / 2;
    auto leftColumn = juce::Rectangle<int>(settingsArea.getX(), settingsArea.getY(), columnWidth, settingsArea.getHeight());
    auto rightColumn = juce::Rectangle<int>(leftColumn.getRight() + columnGap, settingsArea.getY(), columnWidth, settingsArea.getHeight());

    g.drawText("Default BPM", leftColumn.getX(), leftColumn.getY() - 32, leftColumn.getWidth(), 14, juce::Justification::centredLeft, false);
    g.drawText("Default Beats / Bar", rightColumn.getX(), rightColumn.getY() - 32, rightColumn.getWidth(), 14, juce::Justification::centredLeft, false);
    g.drawText("Startup Defaults", settingsArea.getX(), settingsArea.getBottom() - 64, settingsArea.getWidth(), 14, juce::Justification::centredLeft, false);

    auto exportGrid = exportBounds.withTrimmedTop(36).reduced(16, 14);
    const auto exportCellGap = 10;
    const auto exportCellWidth = (exportGrid.getWidth() - exportCellGap) / 2;
    const auto exportLabelHeight = 14;
    const std::array<juce::String, 4> exportLabels { "Format", "Sample Rate", "Bit Depth", "Tail" };

    for (int index = 0; index < 4; ++index)
    {
        const auto row = index < 2 ? 0 : 1;
        const auto column = index % 2;
        const auto x = exportGrid.getX() + (column * (exportCellWidth + exportCellGap));
        const auto y = exportGrid.getY() + (row == 0 ? -12 : 42);
        g.drawText(exportLabels[(size_t) index], x, y, exportCellWidth, exportLabelHeight, juce::Justification::centredLeft, false);
    }

    auto technicalGrid = technicalBounds.withTrimmedTop(36).reduced(16, 12);
    const std::array<juce::String, 4> technicalLabels { "Input Device", "Output Device", "Sample Rate", "Buffer Size" };

    for (int index = 0; index < 4; ++index)
    {
        const auto row = index < 2 ? 0 : 1;
        const auto column = index % 2;
        const auto x = technicalGrid.getX() + (column * (exportCellWidth + exportCellGap));
        const auto y = technicalGrid.getY() + (row == 0 ? -12 : 42);
        g.drawText(technicalLabels[(size_t) index], x, y, exportCellWidth, exportLabelHeight, juce::Justification::centredLeft, false);
    }
}

void AppSettingsComponent::resized()
{
    const auto contentBounds = getContentBounds();
    auto userBounds = juce::Rectangle<int>(contentBounds.getX(), contentBounds.getY() + 32, contentBounds.getWidth(), 272);
    auto exportBounds = juce::Rectangle<int>(contentBounds.getX(), userBounds.getBottom() + 18, contentBounds.getWidth(), 154);
    auto technicalBounds = juce::Rectangle<int>(contentBounds.getX(), exportBounds.getBottom() + 18,
                                                contentBounds.getWidth(), contentBounds.getBottom() - exportBounds.getBottom() - 18);

    auto swatchArea = userBounds.withTrimmedTop(46).removeFromTop(28).reduced(16, 0);
    const auto swatchGap = 12;
    const auto swatchWidth = (swatchArea.getWidth() - (swatchGap * (TapeEngine::numTracks - 1))) / TapeEngine::numTracks;

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        trackColourButtons[(size_t) trackIndex].setBounds(swatchArea.getX() + (trackIndex * (swatchWidth + swatchGap)),
                                                          swatchArea.getY() + 18,
                                                          swatchWidth,
                                                          28);
    }

    auto settingsArea = userBounds.withTrimmedTop(112).reduced(16, 12);
    const auto columnGap = 14;
    const auto columnWidth = (settingsArea.getWidth() - columnGap) / 2;
    auto leftColumn = juce::Rectangle<int>(settingsArea.getX(), settingsArea.getY(), columnWidth, settingsArea.getHeight());
    auto rightColumn = juce::Rectangle<int>(leftColumn.getRight() + columnGap, settingsArea.getY(), columnWidth, settingsArea.getHeight());

    bpmEditor.setBounds(leftColumn.getX(), leftColumn.getY() + 16, 92, 30);
    beatsPerBarEditor.setBounds(rightColumn.getX(), rightColumn.getY() + 16, 92, 30);

    const auto toggleWidth = (settingsArea.getWidth() - columnGap) / 2;
    const auto toggleY = userBounds.getBottom() - 54;
    metronomeDefaultButton.setBounds(settingsArea.getX(), toggleY, toggleWidth, 30);
    countInDefaultButton.setBounds(settingsArea.getX() + toggleWidth + columnGap, toggleY, toggleWidth, 30);

    auto exportGrid = exportBounds.withTrimmedTop(36).reduced(16, 14);
    const auto fieldGap = 10;
    const auto fieldWidth = (exportGrid.getWidth() - fieldGap) / 2;

    exportFormatBox.setBounds(exportGrid.getX(), exportGrid.getY() + 6, fieldWidth, 28);
    exportSampleRateBox.setBounds(exportGrid.getX() + fieldWidth + fieldGap, exportGrid.getY() + 6, fieldWidth, 28);
    exportBitDepthBox.setBounds(exportGrid.getX(), exportGrid.getY() + 58, fieldWidth, 28);
    exportTailBox.setBounds(exportGrid.getX() + fieldWidth + fieldGap, exportGrid.getY() + 58, fieldWidth, 28);

    auto technicalGrid = technicalBounds.withTrimmedTop(36).reduced(16, 12);
    inputDeviceBox.setBounds(technicalGrid.getX(), technicalGrid.getY() + 6, fieldWidth, 28);
    outputDeviceBox.setBounds(technicalGrid.getX() + fieldWidth + fieldGap, technicalGrid.getY() + 6, fieldWidth, 28);
    sampleRateBox.setBounds(technicalGrid.getX(), technicalGrid.getY() + 58, fieldWidth, 28);
    bufferSizeBox.setBounds(technicalGrid.getX() + fieldWidth + fieldGap, technicalGrid.getY() + 58, fieldWidth, 28);
}

void AppSettingsComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &AppSettings::getInstance())
    {
        refreshFromSettings();
        return;
    }

    if (source == &audioDeviceManager)
    {
        refreshAudioOptions();
        return;
    }

    if (activeColourSelector != nullptr && source == activeColourSelector.getComponent())
    {
        const auto colour = activeColourSelector->getCurrentColour();

        if (juce::isPositiveAndBelow(activeColourTrackIndex, TapeEngine::numTracks))
        {
            AppSettings::getInstance().setTrackColour(activeColourTrackIndex, colour);
            trackColourButtons[(size_t) activeColourTrackIndex].setButtonColour(colour);
        }
    }
}

void AppSettingsComponent::configureLabelEditor(juce::Label& editor, int maxCharacters)
{
    editor.setEditable(true, true, false);
    editor.setJustificationType(juce::Justification::centred);
    editor.setFont(AppFonts::getFont(16.0f));
    editor.setColour(juce::Label::textColourId, juce::Colours::white);
    editor.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    editor.setColour(juce::Label::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    editor.setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
    editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.22f));
    editor.onEditorShow = [maxCharacters, &editor]
    {
        if (auto* textEditor = editor.getCurrentTextEditor())
            textEditor->setInputRestrictions(maxCharacters, "0123456789");
    };
}

void AppSettingsComponent::configureComboBox(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    box.setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    box.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    box.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
}

void AppSettingsComponent::configureToggleButton(ToggleButton& button)
{
    button.setTriggeredOnMouseDown(false);
}

void AppSettingsComponent::refreshFromSettings()
{
    const auto& settings = AppSettings::getInstance();

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
        trackColourButtons[(size_t) trackIndex].setButtonColour(settings.getTrackColour(trackIndex));

    bpmEditor.setText(juce::String(settings.getDefaultBpm()), juce::dontSendNotification);
    beatsPerBarEditor.setText(juce::String(settings.getDefaultBeatsPerBar()), juce::dontSendNotification);
    metronomeDefaultButton.setToggleState(settings.getDefaultMetronomeEnabled(), juce::dontSendNotification);
    countInDefaultButton.setToggleState(settings.getDefaultCountInEnabled(), juce::dontSendNotification);

    const auto exportDefaults = settings.getExportDefaults();
    exportFormatBox.setSelectedId(exportDefaults.formatId == 2 ? 2 : 1, juce::dontSendNotification);
    exportSampleRateBox.setSelectedId(std::abs(exportDefaults.sampleRate - 48000.0) < 1.0 ? 2 : 1, juce::dontSendNotification);
    exportBitDepthBox.setSelectedId(exportDefaults.bitDepth == 16 ? 1 : 2, juce::dontSendNotification);
    exportTailBox.setSelectedId(exportDefaults.tailSeconds == 1.0 ? 1
                                 : exportDefaults.tailSeconds == 4.0 ? 3
                                 : exportDefaults.tailSeconds == 8.0 ? 4
                                                                     : 2,
                                juce::dontSendNotification);

    repaint();
}

void AppSettingsComponent::refreshAudioOptions()
{
    refreshingAudioControls = true;

    inputDeviceBox.clear(juce::dontSendNotification);
    outputDeviceBox.clear(juce::dontSendNotification);
    sampleRateBox.clear(juce::dontSendNotification);
    bufferSizeBox.clear(juce::dontSendNotification);

    auto deviceSetup = juce::AudioDeviceManager::AudioDeviceSetup();
    audioDeviceManager.getAudioDeviceSetup(deviceSetup);
    auto* deviceType = audioDeviceManager.getCurrentDeviceTypeObject();

    if (deviceType != nullptr)
    {
        deviceType->scanForDevices();

        const auto outputNames = deviceType->getDeviceNames(false);
        for (int index = 0; index < outputNames.size(); ++index)
            outputDeviceBox.addItem(outputNames[index], index + 1);

        const auto inputNames = deviceType->getDeviceNames(true);
        for (int index = 0; index < inputNames.size(); ++index)
            inputDeviceBox.addItem(inputNames[index], index + 1);

        outputDeviceBox.setText(deviceSetup.outputDeviceName, juce::dontSendNotification);
        inputDeviceBox.setText(deviceType->hasSeparateInputsAndOutputs() ? deviceSetup.inputDeviceName
                                                                         : deviceSetup.outputDeviceName,
                               juce::dontSendNotification);
        inputDeviceBox.setEnabled(deviceType->hasSeparateInputsAndOutputs() && inputNames.isEmpty() == false);
    }
    else
    {
        inputDeviceBox.setEnabled(false);
    }

    if (auto* currentDevice = audioDeviceManager.getCurrentAudioDevice())
    {
        const auto sampleRates = currentDevice->getAvailableSampleRates();
        const auto bufferSizes = currentDevice->getAvailableBufferSizes();

        for (int index = 0; index < sampleRates.size(); ++index)
            sampleRateBox.addItem(formatSampleRate(sampleRates[index]), index + 1);

        for (int index = 0; index < bufferSizes.size(); ++index)
            bufferSizeBox.addItem(juce::String(bufferSizes[index]) + " samples", index + 1);

        sampleRateBox.setText(formatSampleRate(currentDevice->getCurrentSampleRate()), juce::dontSendNotification);
        bufferSizeBox.setText(juce::String(currentDevice->getCurrentBufferSizeSamples()) + " samples", juce::dontSendNotification);
    }

    outputDeviceBox.setEnabled(outputDeviceBox.getNumItems() > 0);
    sampleRateBox.setEnabled(sampleRateBox.getNumItems() > 0);
    bufferSizeBox.setEnabled(bufferSizeBox.getNumItems() > 0);
    refreshingAudioControls = false;
}

void AppSettingsComponent::applyAudioSetup()
{
    auto* deviceType = audioDeviceManager.getCurrentDeviceTypeObject();

    if (deviceType == nullptr)
        return;

    auto setup = juce::AudioDeviceManager::AudioDeviceSetup();
    audioDeviceManager.getAudioDeviceSetup(setup);

    if (outputDeviceBox.getSelectedId() > 0)
        setup.outputDeviceName = outputDeviceBox.getText();

    if (deviceType->hasSeparateInputsAndOutputs())
    {
        if (inputDeviceBox.getSelectedId() > 0)
            setup.inputDeviceName = inputDeviceBox.getText();
    }
    else
    {
        setup.inputDeviceName = setup.outputDeviceName;
    }

    const auto newSampleRate = parseSampleRate(sampleRateBox.getText());
    const auto newBufferSize = parseBufferSize(bufferSizeBox.getText());

    if (newSampleRate > 0.0)
        setup.sampleRate = newSampleRate;

    if (newBufferSize > 0)
        setup.bufferSize = newBufferSize;

    const auto error = audioDeviceManager.setAudioDeviceSetup(setup, true);

    if (error.isNotEmpty())
    {
        refreshAudioOptions();
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Audio Settings", error);
    }
}

void AppSettingsComponent::openColourSelector(int trackIndex)
{
    activeColourTrackIndex = trackIndex;
    auto selector = std::make_unique<juce::ColourSelector>(juce::ColourSelector::showSliders
                                                           | juce::ColourSelector::showColourAtTop
                                                           | juce::ColourSelector::showColourspace);
    selector->setName("Track " + juce::String(trackIndex + 1));
    selector->setCurrentColour(AppSettings::getInstance().getTrackColour(trackIndex));
    selector->addChangeListener(this);
    selector->setSize(340, 360);
    activeColourSelector = selector.get();
    juce::CallOutBox::launchAsynchronously(std::move(selector), trackColourButtons[(size_t) trackIndex].getScreenBounds(), nullptr);
}

juce::Rectangle<int> AppSettingsComponent::getContentBounds() const
{
    return getLocalBounds().reduced(28, 24);
}
