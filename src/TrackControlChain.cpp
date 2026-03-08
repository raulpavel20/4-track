#include "TrackControlChain.h"

#include "AppSettings.h"
#include "AppFonts.h"

#include <cmath>

namespace
{
juce::Colour getChainAccentColour(int trackIndex)
{
    return AppSettings::getInstance().getTrackColour(juce::jlimit(0, TapeEngine::numTracks - 1, trackIndex));
}

juce::String formatFrequencyValue(float value)
{
    const auto clamped = juce::jlimit(20.0f, 20000.0f, value);

    if (clamped >= 1000.0f)
    {
        const auto kilo = clamped / 1000.0f;
        const auto decimals = std::abs(kilo - std::round(kilo)) < 0.05f ? 0 : 1;
        return juce::String(kilo, decimals) + "k";
    }

    return juce::String((int) std::round(clamped));
}

bool parseFrequencyValue(const juce::String& text, float& value)
{
    auto cleaned = text.trim().toLowerCase();
    cleaned = cleaned.removeCharacters(" ");
    cleaned = cleaned.replaceCharacter(',', '.');

    if (cleaned.endsWith("hz"))
        cleaned = cleaned.dropLastCharacters(2);

    auto multiplier = 1.0f;

    if (cleaned.endsWith("k"))
    {
        multiplier = 1000.0f;
        cleaned = cleaned.dropLastCharacters(1);
    }

    if (cleaned.isEmpty())
        return false;

    const auto parsed = cleaned.getFloatValue();

    if (parsed <= 0.0f)
        return false;

    value = juce::jlimit(20.0f, 20000.0f, parsed * multiplier);
    return true;
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
}

TrackControlChain::TrackControlChain(TapeEngine& engineToUse)
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

    inputSourceBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    inputSourceBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    inputSourceBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    inputSourceBox.setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
    inputSourceBox.onChange = [this]
    {
        const auto selectedId = inputSourceBox.getSelectedId();

        if (selectedId > 0)
            engine.setTrackInputSource(selectedTrack, selectedId - 1);
    };
    contentComponent.addAndMakeVisible(inputSourceBox);

    configureRotarySlider(inputGainSlider, 0.0, 2.0, 0.01, 1.0);
    inputGainSlider.onValueChange = [this]
    {
        engine.setTrackInputGain(selectedTrack, (float) inputGainSlider.getValue());
        contentComponent.repaint();
    };
    contentComponent.addAndMakeVisible(inputGainSlider);

    addModuleButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    addModuleButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addModuleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::black);
    addModuleButton.onClick = [safeThis = juce::Component::SafePointer<TrackControlChain>(this)]
    {
        if (safeThis == nullptr)
            return;

        juce::PopupMenu menu;
        menu.addItem(1, "Filter");
        menu.addItem(2, "EQ");
        menu.addItem(3, "Compressor");
        menu.addItem(4, "Saturation");
        menu.addItem(5, "Gain");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&safeThis->addModuleButton),
                           [safeThis](int result)
                           {
                               if (safeThis == nullptr || result <= 0)
                                   return;

                               auto moduleType = ChainModuleType::none;

                               if (result == 1)
                                   moduleType = ChainModuleType::filter;
                               else if (result == 2)
                                   moduleType = ChainModuleType::eq;
                               else if (result == 3)
                                   moduleType = ChainModuleType::compressor;
                               else if (result == 4)
                                   moduleType = ChainModuleType::saturation;
                               else if (result == 5)
                                   moduleType = ChainModuleType::gain;

                               safeThis->engine.addTrackModule(safeThis->selectedTrack, moduleType);
                               safeThis->refreshFromEngine();
                           });
    };
    contentComponent.addAndMakeVisible(addModuleButton);

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        auto& bypassButton = bypassButtons[(size_t) moduleIndex];
        bypassButton.onClick = [this, moduleIndex]
        {
            engine.setTrackModuleBypassed(selectedTrack,
                                         moduleIndex,
                                         bypassButtons[(size_t) moduleIndex].getToggleState());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(bypassButton);

        auto& removeButton = removeButtons[(size_t) moduleIndex];
        removeButton.onClick = [this, moduleIndex]
        {
            engine.removeTrackModule(selectedTrack, moduleIndex);
            refreshFromEngine();
        };
        contentComponent.addAndMakeVisible(removeButton);

        auto& filterSlider = filterSliders[(size_t) moduleIndex];
        configureRotarySlider(filterSlider, -1.0, 1.0, 0.01, 0.0);
        filterSlider.onValueChange = [this, moduleIndex]
        {
            engine.setTrackFilterMorph(selectedTrack, moduleIndex, (float) filterSliders[(size_t) moduleIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(filterSlider);

        for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
        {
            auto& gainSlider = eqGainSliders[(size_t) moduleIndex][(size_t) bandIndex];
            configureVerticalSlider(gainSlider, -18.0, 18.0, 0.1, 0.0);
            gainSlider.onValueChange = [this, moduleIndex, bandIndex]
            {
                engine.setTrackEqBandGainDb(selectedTrack,
                                            moduleIndex,
                                            bandIndex,
                                            (float) eqGainSliders[(size_t) moduleIndex][(size_t) bandIndex].getValue());
                contentComponent.repaint();
            };
            contentComponent.addAndMakeVisible(gainSlider);

            auto& qSlider = eqQSliders[(size_t) moduleIndex][(size_t) bandIndex];
            configureRotarySlider(qSlider, 0.1, 10.0, 0.01, 1.0);
            qSlider.onValueChange = [this, moduleIndex, bandIndex]
            {
                engine.setTrackEqBandQ(selectedTrack,
                                       moduleIndex,
                                       bandIndex,
                                       (float) eqQSliders[(size_t) moduleIndex][(size_t) bandIndex].getValue());
            };
            contentComponent.addAndMakeVisible(qSlider);

            auto& editor = eqFrequencyEditors[(size_t) moduleIndex][(size_t) bandIndex];
            editor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
            editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.18f));
            editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            editor.setColour(juce::CaretComponent::caretColourId, juce::Colours::white);
            editor.setJustification(juce::Justification::centred);
            editor.onReturnKey = [this, moduleIndex, bandIndex]
            {
                float frequency = 0.0f;
                auto& frequencyEditor = eqFrequencyEditors[(size_t) moduleIndex][(size_t) bandIndex];

                if (parseFrequencyValue(frequencyEditor.getText(), frequency))
                {
                    engine.setTrackEqBandFrequency(selectedTrack, moduleIndex, bandIndex, frequency);
                    frequencyEditor.setText(formatFrequencyValue(frequency), juce::dontSendNotification);
                }
                else
                {
                    frequencyEditor.setText(formatFrequencyValue(engine.getTrackEqBandFrequency(selectedTrack, moduleIndex, bandIndex)),
                                            juce::dontSendNotification);
                }
            };
            editor.onFocusLost = editor.onReturnKey;
            contentComponent.addAndMakeVisible(editor);
        }

        for (int controlIndex = 0; controlIndex < compressorControlCount; ++controlIndex)
        {
            auto& slider = compressorSliders[(size_t) moduleIndex][(size_t) controlIndex];

            if (controlIndex == 0)
                configureRotarySlider(slider, -60.0, 0.0, 0.1, -18.0);
            else if (controlIndex == 1)
                configureRotarySlider(slider, 1.0, 20.0, 0.1, 4.0);
            else if (controlIndex == 2)
                configureRotarySlider(slider, 0.1, 100.0, 0.1, 10.0);
            else if (controlIndex == 3)
                configureRotarySlider(slider, 10.0, 1000.0, 1.0, 120.0);
            else
                configureVerticalSlider(slider, 0.0, 24.0, 0.1, 0.0);

            slider.onValueChange = [this, moduleIndex, controlIndex]
            {
                const auto value = (float) compressorSliders[(size_t) moduleIndex][(size_t) controlIndex].getValue();

                if (controlIndex == 0)
                    engine.setTrackCompressorThresholdDb(selectedTrack, moduleIndex, value);
                else if (controlIndex == 1)
                    engine.setTrackCompressorRatio(selectedTrack, moduleIndex, value);
                else if (controlIndex == 2)
                    engine.setTrackCompressorAttackMs(selectedTrack, moduleIndex, value);
                else if (controlIndex == 3)
                    engine.setTrackCompressorReleaseMs(selectedTrack, moduleIndex, value);
                else
                    engine.setTrackCompressorMakeupGainDb(selectedTrack, moduleIndex, value);

                contentComponent.repaint();
            };
            contentComponent.addAndMakeVisible(slider);
        }

        auto& saturationModeButton = saturationModeButtons[(size_t) moduleIndex];
        saturationModeButton.setClickingTogglesState(true);
        saturationModeButton.onClick = [this, moduleIndex]
        {
            engine.setTrackSaturationMode(selectedTrack,
                                          moduleIndex,
                                          saturationModeButtons[(size_t) moduleIndex].getToggleState()
                                              ? SaturationMode::heavy
                                              : SaturationMode::light);
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(saturationModeButton);

        auto& saturationAmountSlider = saturationAmountSliders[(size_t) moduleIndex];
        configureRotarySlider(saturationAmountSlider, 0.0, 1.0, 0.01, 0.35);
        saturationAmountSlider.onValueChange = [this, moduleIndex]
        {
            engine.setTrackSaturationAmount(selectedTrack, moduleIndex, (float) saturationAmountSliders[(size_t) moduleIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(saturationAmountSlider);

        auto& gainSlider = gainModuleSliders[(size_t) moduleIndex];
        configureVerticalSlider(gainSlider, -24.0, 24.0, 0.1, 0.0);
        gainSlider.onValueChange = [this, moduleIndex]
        {
            engine.setTrackGainModuleGainDb(selectedTrack, moduleIndex, (float) gainModuleSliders[(size_t) moduleIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(gainSlider);
    }

    startTimerHz(30);
    updateAccentColours();
    refreshFromEngine();
}

TrackControlChain::~TrackControlChain()
{
    AppSettings::getInstance().removeChangeListener(this);
}

void TrackControlChain::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(getFrameBounds().toFloat(), 18.0f, 1.0f);
}

void TrackControlChain::resized()
{
    modulesViewport.setBounds(getViewportBounds());
    layoutContent();
}

void TrackControlChain::timerCallback()
{
    contentComponent.repaint();
}

void TrackControlChain::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &AppSettings::getInstance())
        return;

    updateAccentColours();
    repaint();
    contentComponent.repaint();
}

void TrackControlChain::setInputOptions(const juce::Array<TapeEngine::InputSourceOption>& options)
{
    inputOptions = options;
    inputSourceBox.clear(juce::dontSendNotification);

    for (const auto& option : inputOptions)
        inputSourceBox.addItem(option.label, option.sourceId + 1);

    inputSourceBox.setEnabled(inputOptions.isEmpty() == false);
    refreshFromEngine();
}

void TrackControlChain::setSelectedTrack(int trackIndex)
{
    selectedTrack = juce::jlimit(0, TapeEngine::numTracks - 1, trackIndex);
    refreshFromEngine();
}

int TrackControlChain::getSelectedTrack() const noexcept
{
    return selectedTrack;
}

void TrackControlChain::refreshFromEngine()
{
    updateAccentColours();
    inputGainSlider.setValue(engine.getTrackInputGain(selectedTrack), juce::dontSendNotification);

    if (inputSourceBox.getNumItems() > 0)
    {
        const auto selectedSourceId = engine.getTrackInputSource(selectedTrack);
        inputSourceBox.setSelectedId(selectedSourceId + 1, juce::dontSendNotification);

        if (inputSourceBox.getSelectedId() == 0 && inputOptions.isEmpty() == false)
        {
            inputSourceBox.setSelectedId(inputOptions[0].sourceId + 1, juce::dontSendNotification);
            engine.setTrackInputSource(selectedTrack, inputOptions[0].sourceId);
        }
    }

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        bypassButtons[(size_t) moduleIndex].setToggleState(engine.isTrackModuleBypassed(selectedTrack, moduleIndex),
                                                           juce::dontSendNotification);
        filterSliders[(size_t) moduleIndex].setValue(engine.getTrackFilterMorph(selectedTrack, moduleIndex), juce::dontSendNotification);
        saturationAmountSliders[(size_t) moduleIndex].setValue(engine.getTrackSaturationAmount(selectedTrack, moduleIndex), juce::dontSendNotification);
        saturationModeButtons[(size_t) moduleIndex].setToggleState(engine.getTrackSaturationMode(selectedTrack, moduleIndex) == SaturationMode::heavy,
                                                                   juce::dontSendNotification);
        gainModuleSliders[(size_t) moduleIndex].setValue(engine.getTrackGainModuleGainDb(selectedTrack, moduleIndex), juce::dontSendNotification);

        compressorSliders[(size_t) moduleIndex][0].setValue(engine.getTrackCompressorThresholdDb(selectedTrack, moduleIndex), juce::dontSendNotification);
        compressorSliders[(size_t) moduleIndex][1].setValue(engine.getTrackCompressorRatio(selectedTrack, moduleIndex), juce::dontSendNotification);
        compressorSliders[(size_t) moduleIndex][2].setValue(engine.getTrackCompressorAttackMs(selectedTrack, moduleIndex), juce::dontSendNotification);
        compressorSliders[(size_t) moduleIndex][3].setValue(engine.getTrackCompressorReleaseMs(selectedTrack, moduleIndex), juce::dontSendNotification);
        compressorSliders[(size_t) moduleIndex][4].setValue(engine.getTrackCompressorMakeupGainDb(selectedTrack, moduleIndex), juce::dontSendNotification);

        for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
        {
            eqGainSliders[(size_t) moduleIndex][(size_t) bandIndex].setValue(engine.getTrackEqBandGainDb(selectedTrack, moduleIndex, bandIndex),
                                                                             juce::dontSendNotification);
            eqQSliders[(size_t) moduleIndex][(size_t) bandIndex].setValue(engine.getTrackEqBandQ(selectedTrack, moduleIndex, bandIndex),
                                                                          juce::dontSendNotification);
        }
    }

    updateVisibleModules();
    syncFrequencyEditorsFromEngine(false);
    updateModuleVisibility();
    layoutContent();
    repaint();
    contentComponent.repaint();
}

void TrackControlChain::updateAccentColours()
{
    const auto accent = getChainAccentColour(selectedTrack);
    inputGainSlider.setColour(juce::Slider::rotarySliderFillColourId, accent);

    for (auto& slider : filterSliders)
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);

    for (auto& slot : eqGainSliders)
    {
        for (auto& slider : slot)
            slider.setColour(juce::Slider::thumbColourId, accent);
    }

    for (auto& slot : eqQSliders)
    {
        for (auto& slider : slot)
            slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    }

    for (auto& slot : compressorSliders)
    {
        for (int controlIndex = 0; controlIndex < compressorControlCount; ++controlIndex)
        {
            auto& slider = slot[(size_t) controlIndex];
            slider.setColour(juce::Slider::thumbColourId, accent);

            if (controlIndex == 4)
            {
                slider.setColour(juce::Slider::trackColourId, accent);
                slider.setColour(juce::Slider::backgroundColourId, juce::Colours::white.withAlpha(0.18f));
                slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
            }
        }
    }

    for (auto& slider : saturationAmountSliders)
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);

    for (auto& slider : gainModuleSliders)
        slider.setColour(juce::Slider::thumbColourId, accent);

    for (auto& button : saturationModeButtons)
        button.setAccentColour(accent);

    for (auto& button : bypassButtons)
        button.setAccentColour(accent);
}

void TrackControlChain::updateVisibleModules()
{
    visibleModuleCount = 0;

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        const auto type = engine.getTrackModuleType(selectedTrack, moduleIndex);

        if (type == ChainModuleType::none)
            continue;

        visibleSlots[(size_t) visibleModuleCount] = moduleIndex;
        visibleTypes[(size_t) visibleModuleCount] = type;
        ++visibleModuleCount;
    }
}

void TrackControlChain::syncFrequencyEditorsFromEngine(bool force)
{
    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
        {
            auto& editor = eqFrequencyEditors[(size_t) moduleIndex][(size_t) bandIndex];

            if (! force && editor.hasKeyboardFocus(true))
                continue;

            editor.setText(formatFrequencyValue(engine.getTrackEqBandFrequency(selectedTrack, moduleIndex, bandIndex)),
                           juce::dontSendNotification);
        }
    }
}

void TrackControlChain::updateModuleVisibility()
{
    const auto canAddModule = engine.getTrackModuleCount(selectedTrack) < Track::maxChainModules;
    addModuleButton.setEnabled(canAddModule);
    addModuleButton.setVisible(true);

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        const auto type = engine.getTrackModuleType(selectedTrack, moduleIndex);
        const auto isVisible = type != ChainModuleType::none;

        bypassButtons[(size_t) moduleIndex].setVisible(isVisible);
        removeButtons[(size_t) moduleIndex].setVisible(isVisible);
        filterSliders[(size_t) moduleIndex].setVisible(type == ChainModuleType::filter);
        saturationModeButtons[(size_t) moduleIndex].setVisible(type == ChainModuleType::saturation);
        saturationAmountSliders[(size_t) moduleIndex].setVisible(type == ChainModuleType::saturation);
        gainModuleSliders[(size_t) moduleIndex].setVisible(type == ChainModuleType::gain);

        for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
        {
            const auto showEq = type == ChainModuleType::eq;
            eqGainSliders[(size_t) moduleIndex][(size_t) bandIndex].setVisible(showEq);
            eqQSliders[(size_t) moduleIndex][(size_t) bandIndex].setVisible(showEq);
            eqFrequencyEditors[(size_t) moduleIndex][(size_t) bandIndex].setVisible(showEq);
        }

        for (int controlIndex = 0; controlIndex < compressorControlCount; ++controlIndex)
            compressorSliders[(size_t) moduleIndex][(size_t) controlIndex].setVisible(type == ChainModuleType::compressor);
    }
}

