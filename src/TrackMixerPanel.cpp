#include "TrackMixerPanel.h"

#include "AppFonts.h"

#include <cmath>

namespace
{
constexpr int moduleGap = 10;
constexpr int trackModuleWidth = 83;
constexpr int fxModuleWidth = 210;
constexpr int exportModuleWidth = 232;
constexpr int moduleVerticalInset = 10;
constexpr int moduleInnerPadding = 10;
constexpr int gainSliderWidth = 20;
constexpr int gainSliderHeight = 110;
constexpr int meterWidth = 10;
constexpr int panKnobSize = 54;
constexpr int sendKnobSize = 40;
constexpr int fxKnobSize = 68;

juce::Colour getTrackColour(int index)
{
    static const std::array<juce::Colour, TapeEngine::numTracks> colours
    {
        juce::Colour::fromRGB(255, 92, 92),
        juce::Colour::fromRGB(255, 184, 77),
        juce::Colour::fromRGB(94, 233, 196),
        juce::Colour::fromRGB(94, 146, 255)
    };

    return colours[(size_t) juce::jlimit(0, TapeEngine::numTracks - 1, index)];
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

void drawVerticalSliderTrack(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto x = (float) bounds.getCentreX();
    const auto top = (float) bounds.getY() + 4.0f;
    const auto bottom = (float) bounds.getBottom() - 4.0f;
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawLine(x, top, x, bottom, 2.0f);
}

juce::String formatDbValue(float value)
{
    return juce::String(value, 1) + " dB";
}

juce::String formatPanValue(float value)
{
    if (std::abs(value) < 0.02f)
        return "C";

    return value < 0.0f ? "L" + juce::String(std::abs(value), 2) : "R" + juce::String(value, 2);
}

juce::String formatPercentValue(float value)
{
    return juce::String((int) std::round(value * 100.0f)) + "%";
}

juce::String formatTimeValue(float value)
{
    return juce::String((int) std::round(value)) + " ms";
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

        auto& delaySendSlider = delaySendSliders[(size_t) trackIndex];
        configureRotarySlider(delaySendSlider, 0.0, 1.0, 0.01, 0.0);
        delaySendSlider.onValueChange = [this, trackIndex]
        {
            engine.setTrackDelaySend(trackIndex, (float) delaySendSliders[(size_t) trackIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(delaySendSlider);

        auto& reverbSendSlider = reverbSendSliders[(size_t) trackIndex];
        configureRotarySlider(reverbSendSlider, 0.0, 1.0, 0.01, 0.0);
        reverbSendSlider.onValueChange = [this, trackIndex]
        {
            engine.setTrackReverbSend(trackIndex, (float) reverbSendSliders[(size_t) trackIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(reverbSendSlider);
    }

    configureRotarySlider(delayControlSliders[0], 0.0, (double) (TapeEngine::getNumDelaySyncOptions() - 1), 1.0, 2.0);
    configureRotarySlider(delayControlSliders[1], 0.0, 0.95, 0.01, 0.35);
    configureRotarySlider(delayControlSliders[2], 0.0, 1.0, 0.01, 0.25);
    delayControlSliders[0].onValueChange = [this]
    {
        if (engine.isDelaySyncEnabled())
            engine.setDelaySyncIndex(juce::roundToInt((float) delayControlSliders[0].getValue()));
        else
            engine.setDelayTimeMs((float) delayControlSliders[0].getValue());

        refreshFromEngine();
    };
    delayControlSliders[1].onValueChange = [this] { engine.setDelayFeedback((float) delayControlSliders[1].getValue()); contentComponent.repaint(); };
    delayControlSliders[2].onValueChange = [this] { engine.setDelayMix((float) delayControlSliders[2].getValue()); contentComponent.repaint(); };

    configureRotarySlider(reverbControlSliders[0], 0.0, 1.0, 0.01, 0.45);
    configureRotarySlider(reverbControlSliders[1], 0.0, 1.0, 0.01, 0.35);
    configureRotarySlider(reverbControlSliders[2], 0.0, 1.0, 0.01, 0.25);
    reverbControlSliders[0].onValueChange = [this] { engine.setReverbSize((float) reverbControlSliders[0].getValue()); contentComponent.repaint(); };
    reverbControlSliders[1].onValueChange = [this] { engine.setReverbDamping((float) reverbControlSliders[1].getValue()); contentComponent.repaint(); };
    reverbControlSliders[2].onValueChange = [this] { engine.setReverbMix((float) reverbControlSliders[2].getValue()); contentComponent.repaint(); };

    for (auto& slider : delayControlSliders)
        contentComponent.addAndMakeVisible(slider);

    for (auto& slider : reverbControlSliders)
        contentComponent.addAndMakeVisible(slider);

    delayTimeModeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    delayTimeModeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    delayTimeModeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    delayTimeModeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    delayTimeModeButton.onClick = [this]
    {
        engine.setDelaySyncEnabled(! engine.isDelaySyncEnabled());
        refreshFromEngine();
    };
    contentComponent.addAndMakeVisible(delayTimeModeButton);

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
    exportFormatBox.setSelectedId(1, juce::dontSendNotification);
    exportSampleRateBox.setSelectedId(1, juce::dontSendNotification);
    exportBitDepthBox.setSelectedId(2, juce::dontSendNotification);
    exportTailBox.setSelectedId(2, juce::dontSendNotification);
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

    updateColours();
    refreshFromEngine();
    startTimerHz(30);
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

void TrackMixerPanel::paintContent(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        const auto bounds = trackModuleBounds[(size_t) trackIndex];
        const auto colour = getTrackColour(trackIndex);
        const auto meterValue = juce::jlimit(0.0f, 1.0f, engine.getTrackMixerMeter(trackIndex));

        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 14.0f, 1.0f);

        const auto content = bounds.reduced(moduleInnerPadding, moduleVerticalInset);
        const auto top = content.getY();
        const auto meterBounds = juce::Rectangle<int>(content.getCentreX() - 18,
                                                      top + 6,
                                                      meterWidth,
                                                      gainSliderHeight);
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawLine((float) meterBounds.getCentreX(),
                   (float) meterBounds.getY() + 4.0f,
                   (float) meterBounds.getCentreX(),
                   (float) meterBounds.getBottom() - 4.0f,
                   2.0f);

        auto filledBounds = meterBounds.reduced(2);
        filledBounds.removeFromBottom(juce::roundToInt((1.0f - meterValue) * (float) filledBounds.getHeight()));
        g.setColour(colour);
        g.fillRoundedRectangle(filledBounds.toFloat(), 3.0f);

        drawVerticalSliderTrack(g, gainSliders[(size_t) trackIndex].getBounds());

        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(AppFonts::getFont(11.0f));
        g.drawText(formatDbValue((float) gainSliders[(size_t) trackIndex].getValue()),
                   bounds.getX(),
                   content.getY() + gainSliderHeight + 6,
                   bounds.getWidth(),
                   14,
                   juce::Justification::centred);
        g.drawText(formatPanValue((float) panSliders[(size_t) trackIndex].getValue()),
                   bounds.getX(),
                   content.getBottom() - 12,
                   bounds.getWidth(),
                   14,
                   juce::Justification::centred);
    }

    auto paintFxModule = [&] (juce::Rectangle<int> bounds,
                              const juce::String& title,
                              const std::array<juce::Slider, TapeEngine::numTracks>& sendSliders,
                              const std::array<juce::String, 3>& controlLabels,
                              const std::array<juce::String, 3>& controlValues)
    {
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 14.0f, 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(AppFonts::getFont(12.0f));
        g.drawText(title,
                   bounds.getX(),
                   bounds.getY() + 6,
                   bounds.getWidth(),
                   16,
                   juce::Justification::centred);

        auto content = bounds.reduced(moduleInnerPadding, moduleVerticalInset);
        content.removeFromTop(24);
        auto sendRow = content.removeFromTop(62);
        const auto sendCellWidth = sendRow.getWidth() / TapeEngine::numTracks;

        for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
        {
            auto cell = sendRow.removeFromLeft(trackIndex == TapeEngine::numTracks - 1 ? sendRow.getWidth() : sendCellWidth);
            const auto colour = getTrackColour(trackIndex);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.setFont(AppFonts::getFont(10.0f));
            g.drawText(juce::String(trackIndex + 1),
                       cell.getX(),
                       cell.getY(),
                       cell.getWidth(),
                       12,
                       juce::Justification::centred);
            g.setColour(colour);
            g.drawText(formatPercentValue((float) sendSliders[(size_t) trackIndex].getValue()),
                       cell.getX(),
                       cell.getBottom() - 14,
                       cell.getWidth(),
                       12,
                       juce::Justification::centred);
        }

        content.removeFromTop(8);
        const auto controlCellWidth = content.getWidth() / 3;

        for (int controlIndex = 0; controlIndex < 3; ++controlIndex)
        {
            auto cell = content.removeFromLeft(controlIndex == 2 ? content.getWidth() : controlCellWidth);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.setFont(AppFonts::getFont(10.0f));
            g.drawText(controlLabels[(size_t) controlIndex],
                       cell.getX(),
                       cell.getY() + 10,
                       cell.getWidth(),
                       12,
                       juce::Justification::centred);
            if (! (title == "DELAY" && controlIndex == 0))
                g.drawText(controlValues[(size_t) controlIndex],
                           cell.getX(),
                           cell.getBottom() - 14,
                           cell.getWidth(),
                           12,
                           juce::Justification::centred);
        }
    };

    paintFxModule(delayModuleBounds,
                  "DELAY",
                  delaySendSliders,
                  { "TIME", "FDBK", "MIX" },
                  { delayTimeModeButton.getButtonText(),
                    formatPercentValue((float) delayControlSliders[1].getValue()),
                    formatPercentValue((float) delayControlSliders[2].getValue()) });

    paintFxModule(reverbModuleBounds,
                  "REVERB",
                  reverbSendSliders,
                  { "SIZE", "DAMP", "MIX" },
                  { formatPercentValue((float) reverbControlSliders[0].getValue()),
                    formatPercentValue((float) reverbControlSliders[1].getValue()),
                    formatPercentValue((float) reverbControlSliders[2].getValue()) });

    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(exportModuleBounds.toFloat().reduced(0.5f), 14.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(12.0f));
    g.drawText("EXPORT",
               exportModuleBounds.getX(),
               exportModuleBounds.getY() + 6,
               exportModuleBounds.getWidth(),
               16,
               juce::Justification::centred);

    static const std::array<juce::String, 4> exportLabels { "FORMAT", "RATE", "DEPTH", "TAIL" };
    const std::array<juce::String, 4> exportValues
    {
        exportFormatBox.getText(),
        formatSampleRateValue(exportSampleRateBox.getSelectedId() == 2 ? 48000.0 : 44100.0),
        exportBitDepthBox.getText(),
        exportTailBox.getText()
    };

    auto exportContent = exportModuleBounds.reduced(moduleInnerPadding, moduleVerticalInset);
    exportContent.removeFromTop(28);
    const auto rowHeight = 56;
    auto topRow = exportContent.removeFromTop(rowHeight);
    auto bottomRow = exportContent.removeFromTop(rowHeight);
    const auto cellWidth = exportContent.getWidth() / 2;

    for (int index = 0; index < 4; ++index)
    {
        auto& row = index < 2 ? topRow : bottomRow;
        auto cell = row.removeFromLeft(index % 2 == 0 ? cellWidth : row.getWidth());
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(AppFonts::getFont(10.0f));
        g.drawText(exportLabels[(size_t) index],
                   cell.getX(),
                   cell.getY() + 2,
                   cell.getWidth(),
                   12,
                   juce::Justification::centred);
    }
}

void TrackMixerPanel::refreshFromEngine()
{
    updateColours();

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        gainSliders[(size_t) trackIndex].setValue(engine.getTrackMixerGainDb(trackIndex), juce::dontSendNotification);
        panSliders[(size_t) trackIndex].setValue(engine.getTrackMixerPan(trackIndex), juce::dontSendNotification);
        delaySendSliders[(size_t) trackIndex].setValue(engine.getTrackDelaySend(trackIndex), juce::dontSendNotification);
        reverbSendSliders[(size_t) trackIndex].setValue(engine.getTrackReverbSend(trackIndex), juce::dontSendNotification);
    }

    if (engine.isDelaySyncEnabled())
    {
        delayControlSliders[0].setRange(0.0, (double) (TapeEngine::getNumDelaySyncOptions() - 1), 1.0);
        delayControlSliders[0].setDoubleClickReturnValue(true, 2.0);
        delayControlSliders[0].setValue((double) engine.getDelaySyncIndex(), juce::dontSendNotification);
    }
    else
    {
        delayControlSliders[0].setRange(20.0, 2000.0, 1.0);
        delayControlSliders[0].setDoubleClickReturnValue(true, 380.0);
        delayControlSliders[0].setValue(engine.getDelayTimeMs(), juce::dontSendNotification);
    }

    delayControlSliders[1].setValue(engine.getDelayFeedback(), juce::dontSendNotification);
    delayControlSliders[2].setValue(engine.getDelayMix(), juce::dontSendNotification);
    reverbControlSliders[0].setValue(engine.getReverbSize(), juce::dontSendNotification);
    reverbControlSliders[1].setValue(engine.getReverbDamping(), juce::dontSendNotification);
    reverbControlSliders[2].setValue(engine.getReverbMix(), juce::dontSendNotification);
    delayTimeModeButton.setButtonText(engine.isDelaySyncEnabled() ? TapeEngine::getDelaySyncLabel(engine.getDelaySyncIndex())
                                                                  : formatTimeValue(engine.getDelayTimeMs()));
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
        delaySendSliders[(size_t) trackIndex].setColour(juce::Slider::rotarySliderFillColourId, colour);
        reverbSendSliders[(size_t) trackIndex].setColour(juce::Slider::rotarySliderFillColourId, colour);
    }

    for (auto& slider : delayControlSliders)
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white);

    for (auto& slider : reverbControlSliders)
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white);

    delayTimeModeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    delayTimeModeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

void TrackMixerPanel::layoutContent()
{
    const auto viewportBounds = modulesViewport.getBounds();
    const auto contentHeight = juce::jmax(1, viewportBounds.getHeight());

    auto x = 0;

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        trackModuleBounds[(size_t) trackIndex] = juce::Rectangle<int>(x, 0, trackModuleWidth, contentHeight);
        x += trackModuleWidth + moduleGap;
    }

    delayModuleBounds = juce::Rectangle<int>(x, 0, fxModuleWidth, contentHeight);
    x += fxModuleWidth + moduleGap;
    reverbModuleBounds = juce::Rectangle<int>(x, 0, fxModuleWidth, contentHeight);
    x += fxModuleWidth + moduleGap;
    exportModuleBounds = juce::Rectangle<int>(x, 0, exportModuleWidth, contentHeight);
    const auto requiredWidth = exportModuleBounds.getRight();
    contentComponent.setSize(juce::jmax(viewportBounds.getWidth(), requiredWidth), contentHeight);

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        auto content = trackModuleBounds[(size_t) trackIndex].reduced(moduleInnerPadding, moduleVerticalInset);
        const auto top = content.getY();
        const auto meterX = content.getCentreX() - 18;
        gainSliders[(size_t) trackIndex].setBounds(meterX + 14,
                                                   top + 6,
                                                   gainSliderWidth,
                                                   gainSliderHeight);
        panSliders[(size_t) trackIndex].setBounds(content.getCentreX() - (panKnobSize / 2),
                                                  content.getBottom() - panKnobSize - 4,
                                                  panKnobSize,
                                                  panKnobSize);
    }

    auto layoutFxModule = [&] (juce::Rectangle<int> bounds,
                               std::array<juce::Slider, TapeEngine::numTracks>& sendSliders,
                               std::array<juce::Slider, 3>& controlSliders)
    {
        auto content = bounds.reduced(moduleInnerPadding, moduleVerticalInset);
        content.removeFromTop(24);
        auto sendRow = content.removeFromTop(62);
        const auto sendCellWidth = sendRow.getWidth() / TapeEngine::numTracks;

        for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
        {
            auto cell = sendRow.removeFromLeft(trackIndex == TapeEngine::numTracks - 1 ? sendRow.getWidth() : sendCellWidth);
            sendSliders[(size_t) trackIndex].setBounds(cell.getCentreX() - (sendKnobSize / 2),
                                                       cell.getY() + 12,
                                                       sendKnobSize,
                                                       sendKnobSize);
        }

        content.removeFromTop(8);
        const auto controlCellWidth = content.getWidth() / 3;

        for (int controlIndex = 0; controlIndex < 3; ++controlIndex)
        {
            auto cell = content.removeFromLeft(controlIndex == 2 ? content.getWidth() : controlCellWidth);
            controlSliders[(size_t) controlIndex].setBounds(cell.getCentreX() - (fxKnobSize / 2),
                                                            cell.getCentreY() - (fxKnobSize / 2) + 6,
                                                            fxKnobSize,
                                                            fxKnobSize);

            if (&controlSliders == &delayControlSliders && controlIndex == 0)
            {
                const auto buttonWidth = cell.getWidth() / 2 + 8;
                delayTimeModeButton.setBounds(cell.getX() + (buttonWidth / 2) - 7, cell.getBottom() - 14, buttonWidth, 16);
            }
        }
    };

    layoutFxModule(delayModuleBounds, delaySendSliders, delayControlSliders);
    layoutFxModule(reverbModuleBounds, reverbSendSliders, reverbControlSliders);

    auto exportContent = exportModuleBounds.reduced(moduleInnerPadding, moduleVerticalInset);
    exportContent.removeFromTop(28);
    auto topRow = exportContent.removeFromTop(56);
    auto bottomRow = exportContent.removeFromTop(56);
    const auto cellGap = 8;
    const auto cellWidth = (topRow.getWidth() - cellGap) / 2;
    const auto boxHeight = 28;
    const auto buttonWidth = 94;
    auto placeBox = [&] (juce::Rectangle<int> row, int columnIndex)
    {
        const auto xOffset = columnIndex == 0 ? 0 : cellWidth + cellGap;
        return juce::Rectangle<int>(row.getX() + xOffset, row.getY() + 16, cellWidth, boxHeight);
    };

    exportFormatBox.setBounds(placeBox(topRow, 0));
    exportSampleRateBox.setBounds(placeBox(topRow, 1));
    exportBitDepthBox.setBounds(placeBox(bottomRow, 0));
    exportTailBox.setBounds(placeBox(bottomRow, 1));
    exportButton.setBounds(exportModuleBounds.getCentreX() - (buttonWidth / 2),
                           exportModuleBounds.getBottom() - moduleVerticalInset - 38,
                           buttonWidth,
                           28);
}

juce::Rectangle<int> TrackMixerPanel::getFrameBounds() const
{
    return getLocalBounds().reduced(6);
}

juce::Rectangle<int> TrackMixerPanel::getViewportBounds() const
{
    return getFrameBounds().reduced(8);
}
