#include "TrackMixerPanel.h"

#include "AppSettings.h"
#include "AppFonts.h"

#include <cmath>

namespace
{
juce::Colour getTrackColour(int index)
{
    return AppSettings::getInstance().getTrackColour(juce::jlimit(0, TapeEngine::numTracks - 1, index));
}

void configureRotarySlider(juce::Slider& slider, double min, double max, double step, double resetValue)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(min, max, step);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f,
                               true);
    slider.setDoubleClickReturnValue(true, resetValue);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha(0.18f));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
}

void configureVerticalSlider(juce::Slider& slider, double min, double max, double step, double resetValue)
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(min, max, step);
    slider.setDoubleClickReturnValue(true, resetValue);
    slider.setColour(juce::Slider::trackColourId, juce::Colours::white.withAlpha(0.16f));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colours::black);
}

juce::String formatSampleRateValue(double value)
{
    if (value >= 1000.0)
        return juce::String(value / 1000.0, std::abs(std::round(value / 1000.0) - (value / 1000.0)) < 0.01 ? 0 : 1) + " kHz";

    return juce::String((int) std::round(value)) + " Hz";
}
}

TrackMixerPanel::ContentComponent::ContentComponent(TrackMixerPanel& ownerToUse)
    : owner(ownerToUse)
{
}

void TrackMixerPanel::ContentComponent::paint(juce::Graphics& g)
{
    owner.paintContent(g);
}

TrackMixerPanel::TrackMixerPanel(TapeEngine& engineToUse)
    : engine(engineToUse),
      contentComponent(*this)
{
    AppSettings::getInstance().addChangeListener(this);
    addAndMakeVisible(modulesViewport);
    modulesViewport.setViewedComponent(&contentComponent, false);
    modulesViewport.setScrollBarsShown(false, true);
    modulesViewport.setScrollBarThickness(8);
    modulesViewport.getHorizontalScrollBar().setColour(juce::ScrollBar::thumbColourId, juce::Colours::white.withAlpha(0.32f));
    modulesViewport.getHorizontalScrollBar().setColour(juce::ScrollBar::trackColourId, juce::Colours::white.withAlpha(0.08f));

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        auto& gainSlider = gainSliders[(size_t) trackIndex];
        configureVerticalSlider(gainSlider, -24.0, 12.0, 0.1, 0.0);
        gainSlider.onValueChange = [this, trackIndex]
        {
            engine.setTrackMixerGainDb(trackIndex, (float) gainSliders[(size_t) trackIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(gainSlider);

        auto& panSlider = panSliders[(size_t) trackIndex];
        configureRotarySlider(panSlider, -1.0, 1.0, 0.01, 0.0);
        panSlider.onValueChange = [this, trackIndex]
        {
            engine.setTrackMixerPan(trackIndex, (float) panSliders[(size_t) trackIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(panSlider);
    }

    for (int sendIndex = 0; sendIndex < TapeEngine::numSendBuses; ++sendIndex)
    {
        for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
        {
            auto& sendSlider = sendSliders[(size_t) sendIndex][(size_t) trackIndex];
            configureRotarySlider(sendSlider, 0.0, 1.0, 0.01, 0.0);
            sendSlider.onValueChange = [this, sendIndex, trackIndex]
            {
                engine.setTrackSendLevel(trackIndex,
                                         sendIndex,
                                         (float) sendSliders[(size_t) sendIndex][(size_t) trackIndex].getValue());
                contentComponent.repaint();
            };
            contentComponent.addAndMakeVisible(sendSlider);
        }
    }

    auto configureExportBox = [] (juce::ComboBox& box)
    {
        box.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
        box.setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.18f));
        box.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        box.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    };

    configureExportBox(exportFormatBox);
    configureExportBox(exportSampleRateBox);
    configureExportBox(exportBitDepthBox);
    configureExportBox(exportTailBox);
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
    exportFormatBox.onChange = [this] { persistExportDefaults(); };
    exportSampleRateBox.onChange = [this] { persistExportDefaults(); };
    exportBitDepthBox.onChange = [this] { persistExportDefaults(); };
    exportTailBox.onChange = [this] { persistExportDefaults(); };
    contentComponent.addAndMakeVisible(exportFormatBox);
    contentComponent.addAndMakeVisible(exportSampleRateBox);
    contentComponent.addAndMakeVisible(exportBitDepthBox);
    contentComponent.addAndMakeVisible(exportTailBox);

    exportButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    exportButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::black);
    exportButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    exportButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    exportButton.onClick = [this]
    {
        TapeEngine::ExportSettings settings;
        settings.format = exportFormatBox.getSelectedId() == 2 ? TapeEngine::ExportFormat::aiff
                                                               : TapeEngine::ExportFormat::wav;
        settings.sampleRate = exportSampleRateBox.getSelectedId() == 2 ? 48000.0 : 44100.0;
        settings.bitDepth = exportBitDepthBox.getSelectedId() == 1 ? 16 : 24;
        settings.tailSeconds = exportTailBox.getSelectedId() == 1 ? 1.0
                               : exportTailBox.getSelectedId() == 3 ? 4.0
                               : exportTailBox.getSelectedId() == 4 ? 8.0
                                                                    : 2.0;
        const auto wildcard = settings.format == TapeEngine::ExportFormat::aiff ? "*.aiff" : "*.wav";
        const auto suffix = settings.format == TapeEngine::ExportFormat::aiff ? ".aiff" : ".wav";
        exportChooser = std::make_unique<juce::FileChooser>("Export Mix", juce::File(), wildcard);
        auto safeThis = juce::Component::SafePointer<TrackMixerPanel>(this);
        exportChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                   [safeThis, settings, suffix] (const juce::FileChooser& chooser)
                                   {
                                       if (safeThis == nullptr)
                                           return;

                                       auto file = chooser.getResult();

                                       if (file == juce::File())
                                           return;

                                       if (! file.hasFileExtension(suffix))
                                           file = file.withFileExtension(suffix);

                                       const auto result = safeThis->engine.exportMixToFile(file, settings);

                                       if (result.failed())
                                           juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Export Failed", result.getErrorMessage());
                                       else
                                           juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                                                  "Export Complete",
                                                                                  "Your mix was exported successfully.\n\n"
                                                                                  "File: " + file.getFileName() + "\n"
                                                                                  "Format: " + (settings.format == TapeEngine::ExportFormat::aiff ? juce::String("AIFF") : juce::String("WAV")) + "\n"
                                                                                  "Sample rate: " + formatSampleRateValue(settings.sampleRate) + "\n"
                                                                                  "Bit depth: " + juce::String(settings.bitDepth) + "-bit\n"
                                                                                  "Tail: " + juce::String(settings.tailSeconds, settings.tailSeconds == std::round(settings.tailSeconds) ? 0 : 1) + " sec");

                                       safeThis->exportChooser.reset();
                                   });
    };
    contentComponent.addAndMakeVisible(exportButton);

    syncExportDefaultsFromSettings();
    updateColours();
    refreshFromEngine();
    startTimerHz(30);
}

TrackMixerPanel::~TrackMixerPanel()
{
    AppSettings::getInstance().removeChangeListener(this);
}

void TrackMixerPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(getFrameBounds().toFloat(), 18.0f, 1.0f);
}

void TrackMixerPanel::resized()
{
    modulesViewport.setBounds(getViewportBounds());
    layoutContent();
}

void TrackMixerPanel::timerCallback()
{
    contentComponent.repaint();
}

void TrackMixerPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &AppSettings::getInstance())
        return;

    syncExportDefaultsFromSettings();
    updateColours();
    repaint();
    contentComponent.repaint();
}


void TrackMixerPanel::refreshFromEngine()
{
    updateColours();

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        gainSliders[(size_t) trackIndex].setValue(engine.getTrackMixerGainDb(trackIndex), juce::dontSendNotification);
        panSliders[(size_t) trackIndex].setValue(engine.getTrackMixerPan(trackIndex), juce::dontSendNotification);
    }

    for (int sendIndex = 0; sendIndex < TapeEngine::numSendBuses; ++sendIndex)
    {
        for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
            sendSliders[(size_t) sendIndex][(size_t) trackIndex].setValue(engine.getTrackSendLevel(trackIndex, sendIndex),
                                                                          juce::dontSendNotification);
    }

    layoutContent();
    repaint();
    contentComponent.repaint();
}

void TrackMixerPanel::updateColours()
{
    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        const auto colour = getTrackColour(trackIndex);
        gainSliders[(size_t) trackIndex].setColour(juce::Slider::thumbColourId, colour);
        panSliders[(size_t) trackIndex].setColour(juce::Slider::rotarySliderFillColourId, colour);

        for (int sendIndex = 0; sendIndex < TapeEngine::numSendBuses; ++sendIndex)
            sendSliders[(size_t) sendIndex][(size_t) trackIndex].setColour(juce::Slider::rotarySliderFillColourId, colour);
    }
}

void TrackMixerPanel::syncExportDefaultsFromSettings()
{
    const auto defaults = AppSettings::getInstance().getExportDefaults();
    exportFormatBox.setSelectedId(defaults.formatId == 2 ? 2 : 1, juce::dontSendNotification);
    exportSampleRateBox.setSelectedId(std::abs(defaults.sampleRate - 48000.0) < 1.0 ? 2 : 1, juce::dontSendNotification);
    exportBitDepthBox.setSelectedId(defaults.bitDepth == 16 ? 1 : 2, juce::dontSendNotification);

    auto tailId = 2;

    if (defaults.tailSeconds == 1.0)
        tailId = 1;
    else if (defaults.tailSeconds == 4.0)
        tailId = 3;
    else if (defaults.tailSeconds == 8.0)
        tailId = 4;

    exportTailBox.setSelectedId(tailId, juce::dontSendNotification);
}

void TrackMixerPanel::persistExportDefaults()
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

