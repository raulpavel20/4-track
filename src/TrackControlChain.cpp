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

        if (selectedId > 0 && isTrackTarget())
            engine.setTrackInputSource(selectedTrack, selectedId - 1);
    };
    contentComponent.addAndMakeVisible(inputSourceBox);

    configureRotarySlider(inputGainSlider, 0.0, 2.0, 0.01, 1.0);
    inputGainSlider.onValueChange = [this]
    {
        if (isTrackTarget())
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
        menu.addItem(5, "Delay");
        menu.addItem(6, "Reverb");
        menu.addItem(7, "Utility");
        menu.addItem(8, "Spectrum Analyzer");
        menu.addItem(9, "Phaser");
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
                                   moduleType = ChainModuleType::delay;
                               else if (result == 6)
                                   moduleType = ChainModuleType::reverb;
                               else if (result == 7)
                                   moduleType = ChainModuleType::gain;
                               else if (result == 8)
                                   moduleType = ChainModuleType::spectrumAnalyzer;
                               else if (result == 9)
                                   moduleType = ChainModuleType::phaser;

                               safeThis->addModule(moduleType);
                               safeThis->refreshFromEngine();
                           });
    };
    contentComponent.addAndMakeVisible(addModuleButton);

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        auto& bypassButton = bypassButtons[(size_t) moduleIndex];
        bypassButton.onClick = [this, moduleIndex]
        {
            setModuleBypassed(moduleIndex, bypassButtons[(size_t) moduleIndex].getToggleState());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(bypassButton);

        auto& removeButton = removeButtons[(size_t) moduleIndex];
        removeButton.onClick = [this, moduleIndex]
        {
            removeModule(moduleIndex);
            refreshFromEngine();
        };
        contentComponent.addAndMakeVisible(removeButton);

        auto& filterSlider = filterSliders[(size_t) moduleIndex];
        configureRotarySlider(filterSlider, -1.0, 1.0, 0.01, 0.0);
        filterSlider.onValueChange = [this, moduleIndex]
        {
            setFilterMorph(moduleIndex, (float) filterSliders[(size_t) moduleIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(filterSlider);

        for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
        {
            auto& gainSlider = eqGainSliders[(size_t) moduleIndex][(size_t) bandIndex];
            configureVerticalSlider(gainSlider, -18.0, 18.0, 0.1, 0.0);
            gainSlider.onValueChange = [this, moduleIndex, bandIndex]
            {
                setEqBandGainDb(moduleIndex,
                                bandIndex,
                                (float) eqGainSliders[(size_t) moduleIndex][(size_t) bandIndex].getValue());
                contentComponent.repaint();
            };
            contentComponent.addAndMakeVisible(gainSlider);

            auto& qSlider = eqQSliders[(size_t) moduleIndex][(size_t) bandIndex];
            configureRotarySlider(qSlider, 0.1, 10.0, 0.01, 1.0);
            qSlider.onValueChange = [this, moduleIndex, bandIndex]
            {
                setEqBandQ(moduleIndex,
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
                    setEqBandFrequency(moduleIndex, bandIndex, frequency);
                    frequencyEditor.setText(formatFrequencyValue(frequency), juce::dontSendNotification);
                }
                else
                {
                    frequencyEditor.setText(formatFrequencyValue(getEqBandFrequency(moduleIndex, bandIndex)),
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
                    setCompressorThresholdDb(moduleIndex, value);
                else if (controlIndex == 1)
                    setCompressorRatio(moduleIndex, value);
                else if (controlIndex == 2)
                    setCompressorAttackMs(moduleIndex, value);
                else if (controlIndex == 3)
                    setCompressorReleaseMs(moduleIndex, value);
                else
                    setCompressorMakeupGainDb(moduleIndex, value);

                contentComponent.repaint();
            };
            contentComponent.addAndMakeVisible(slider);
        }

        auto& saturationModeButton = saturationModeButtons[(size_t) moduleIndex];
        saturationModeButton.setClickingTogglesState(true);
        saturationModeButton.onClick = [this, moduleIndex]
        {
            setSaturationMode(moduleIndex,
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
            setSaturationAmount(moduleIndex, (float) saturationAmountSliders[(size_t) moduleIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(saturationAmountSlider);

        auto& delayTimeModeButton = delayTimeModeButtons[(size_t) moduleIndex];
        delayTimeModeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        delayTimeModeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        delayTimeModeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.72f));
        delayTimeModeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        delayTimeModeButton.onClick = [this, moduleIndex]
        {
            setDelaySyncEnabled(moduleIndex, ! isDelaySyncEnabled(moduleIndex));
            refreshFromEngine();
        };
        contentComponent.addAndMakeVisible(delayTimeModeButton);

        auto& phaserRateModeButton = phaserRateModeButtons[(size_t) moduleIndex];
        phaserRateModeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        phaserRateModeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        phaserRateModeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.72f));
        phaserRateModeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        phaserRateModeButton.onClick = [this, moduleIndex]
        {
            setPhaserSyncEnabled(moduleIndex, ! isPhaserSyncEnabled(moduleIndex));
            refreshFromEngine();
        };
        contentComponent.addAndMakeVisible(phaserRateModeButton);

        for (int controlIndex = 0; controlIndex < delayControlCount; ++controlIndex)
        {
            auto& slider = delaySliders[(size_t) moduleIndex][(size_t) controlIndex];

            if (controlIndex == 0)
                configureRotarySlider(slider, 20.0, 2000.0, 1.0, 380.0);
            else if (controlIndex == 1)
                configureRotarySlider(slider, 0.0, 0.95, 0.01, 0.35);
            else
                configureRotarySlider(slider, 0.0, 1.0, 0.01, 0.25);

            slider.onValueChange = [this, moduleIndex, controlIndex]
            {
                const auto value = (float) delaySliders[(size_t) moduleIndex][(size_t) controlIndex].getValue();

                if (controlIndex == 0)
                {
                    if (isDelaySyncEnabled(moduleIndex))
                        setDelaySyncIndex(moduleIndex, juce::roundToInt(value));
                    else
                        setDelayTimeMs(moduleIndex, value);

                    refreshFromEngine();
                    return;
                }
                else if (controlIndex == 1)
                    setDelayFeedback(moduleIndex, value);
                else
                    setDelayMix(moduleIndex, value);

                contentComponent.repaint();
            };
            contentComponent.addAndMakeVisible(slider);
        }

        for (int controlIndex = 0; controlIndex < reverbControlCount; ++controlIndex)
        {
            auto& slider = reverbSliders[(size_t) moduleIndex][(size_t) controlIndex];
            configureRotarySlider(slider,
                                  0.0,
                                  1.0,
                                  0.01,
                                  controlIndex == 0 ? 0.45 : controlIndex == 1 ? 0.35
                                                                                : 0.25);
            slider.onValueChange = [this, moduleIndex, controlIndex]
            {
                const auto value = (float) reverbSliders[(size_t) moduleIndex][(size_t) controlIndex].getValue();

                if (controlIndex == 0)
                    setReverbSize(moduleIndex, value);
                else if (controlIndex == 1)
                    setReverbDamping(moduleIndex, value);
                else
                    setReverbMix(moduleIndex, value);

                contentComponent.repaint();
            };
            contentComponent.addAndMakeVisible(slider);
        }

        for (int controlIndex = 0; controlIndex < phaserControlCount; ++controlIndex)
        {
            auto& slider = phaserSliders[(size_t) moduleIndex][(size_t) controlIndex];

            if (controlIndex == 0)
                configureRotarySlider(slider, 0.05, 10.0, 0.01, 0.5);
            else if (controlIndex == 1)
                configureRotarySlider(slider, 0.0, 1.0, 0.01, 0.8);
            else if (controlIndex == 2)
                configureRotarySlider(slider, 200.0, 2000.0, 1.0, 700.0);
            else if (controlIndex == 3)
                configureRotarySlider(slider, 0.0, 0.95, 0.01, 0.3);
            else
                configureRotarySlider(slider, 0.0, 1.0, 0.01, 0.5);

            slider.onValueChange = [this, moduleIndex, controlIndex]
            {
                const auto value = (float) phaserSliders[(size_t) moduleIndex][(size_t) controlIndex].getValue();

                if (controlIndex == 0)
                {
                    if (isPhaserSyncEnabled(moduleIndex))
                        setPhaserSyncIndex(moduleIndex, juce::roundToInt(value));
                    else
                        setPhaserRate(moduleIndex, value);

                    refreshFromEngine();
                    return;
                }
                else if (controlIndex == 1)
                    setPhaserDepth(moduleIndex, value);
                else if (controlIndex == 2)
                    setPhaserCentreFrequency(moduleIndex, value);
                else if (controlIndex == 3)
                    setPhaserFeedback(moduleIndex, value);
                else
                    setPhaserMix(moduleIndex, value);

                contentComponent.repaint();
            };
            contentComponent.addAndMakeVisible(slider);
        }

        auto& gainSlider = gainModuleSliders[(size_t) moduleIndex][0];
        configureRotarySlider(gainSlider, -24.0, 24.0, 0.1, 0.0);
        gainSlider.onValueChange = [this, moduleIndex]
        {
            setGainModuleGainDb(moduleIndex, (float) gainModuleSliders[(size_t) moduleIndex][0].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(gainSlider);

        auto& panSlider = gainModuleSliders[(size_t) moduleIndex][1];
        configureRotarySlider(panSlider, -1.0, 1.0, 0.01, 0.0);
        panSlider.onValueChange = [this, moduleIndex]
        {
            setGainModulePan(moduleIndex, (float) gainModuleSliders[(size_t) moduleIndex][1].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(panSlider);
    }

    startTimerHz(60);
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
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto deltaSeconds = lastAnimationTimestampMs > 0.0 ? juce::jlimit(0.0, 0.05, (nowMs - lastAnimationTimestampMs) * 0.001)
                                                             : (1.0 / 60.0);
    lastAnimationTimestampMs = nowMs;

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        if (getModuleType(moduleIndex) != ChainModuleType::phaser || isModuleBypassed(moduleIndex))
            continue;

                    const auto rate = juce::jlimit(0.05f, 10.0f, getResolvedPhaserRateHz(moduleIndex));
        const auto rateNorm = (float) ((std::log((double) rate) - std::log(0.05)) / (std::log(10.0) - std::log(0.05)));
        const auto targetVisualRate = juce::jmap(juce::jlimit(0.0f, 1.0f, rateNorm), 0.12f, 0.7f);
        auto& smoothedVisualRate = phaserVisualRates[(size_t) moduleIndex];
        smoothedVisualRate += (targetVisualRate - smoothedVisualRate) * 0.16f;
        phaserVisualPhases[(size_t) moduleIndex] += (float) (deltaSeconds * (double) juce::MathConstants<float>::twoPi * smoothedVisualRate);
    }

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
    targetType = TargetType::track;
    selectedTrack = juce::jlimit(0, TapeEngine::numTracks - 1, trackIndex);
    refreshFromEngine();
}

int TrackControlChain::getSelectedTrack() const noexcept
{
    return selectedTrack;
}

void TrackControlChain::setSendBusIndex(int sendIndex)
{
    targetType = TargetType::sendBus;
    selectedSendBus = juce::jlimit(0, TapeEngine::numSendBuses - 1, sendIndex);
    refreshFromEngine();
}

int TrackControlChain::getSendBusIndex() const noexcept
{
    return selectedSendBus;
}

bool TrackControlChain::showsInputModule() const noexcept
{
    return targetType == TargetType::track;
}

bool TrackControlChain::isTrackTarget() const noexcept
{
    return targetType == TargetType::track;
}

juce::Colour TrackControlChain::getAccentColour() const
{
    return isTrackTarget() ? getChainAccentColour(selectedTrack) : juce::Colours::white;
}

int TrackControlChain::addModule(ChainModuleType type)
{
    return isTrackTarget() ? engine.addTrackModule(selectedTrack, type)
                           : engine.addSendBusModule(selectedSendBus, type);
}

void TrackControlChain::removeModule(int moduleIndex)
{
    if (isTrackTarget())
        engine.removeTrackModule(selectedTrack, moduleIndex);
    else
        engine.removeSendBusModule(selectedSendBus, moduleIndex);
}

int TrackControlChain::getModuleCount() const noexcept
{
    return isTrackTarget() ? engine.getTrackModuleCount(selectedTrack)
                           : engine.getSendBusModuleCount(selectedSendBus);
}

ChainModuleType TrackControlChain::getModuleType(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackModuleType(selectedTrack, moduleIndex)
                           : engine.getSendBusModuleType(selectedSendBus, moduleIndex);
}

bool TrackControlChain::isModuleBypassed(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.isTrackModuleBypassed(selectedTrack, moduleIndex)
                           : engine.isSendBusModuleBypassed(selectedSendBus, moduleIndex);
}

void TrackControlChain::setModuleBypassed(int moduleIndex, bool shouldBeBypassed)
{
    if (isTrackTarget())
        engine.setTrackModuleBypassed(selectedTrack, moduleIndex, shouldBeBypassed);
    else
        engine.setSendBusModuleBypassed(selectedSendBus, moduleIndex, shouldBeBypassed);
}

float TrackControlChain::getFilterMorph(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackFilterMorph(selectedTrack, moduleIndex)
                           : engine.getSendBusFilterMorph(selectedSendBus, moduleIndex);
}

void TrackControlChain::setFilterMorph(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackFilterMorph(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusFilterMorph(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getEqBandGainDb(int moduleIndex, int bandIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackEqBandGainDb(selectedTrack, moduleIndex, bandIndex)
                           : engine.getSendBusEqBandGainDb(selectedSendBus, moduleIndex, bandIndex);
}

void TrackControlChain::setEqBandGainDb(int moduleIndex, int bandIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackEqBandGainDb(selectedTrack, moduleIndex, bandIndex, value);
    else
        engine.setSendBusEqBandGainDb(selectedSendBus, moduleIndex, bandIndex, value);
}

float TrackControlChain::getEqBandQ(int moduleIndex, int bandIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackEqBandQ(selectedTrack, moduleIndex, bandIndex)
                           : engine.getSendBusEqBandQ(selectedSendBus, moduleIndex, bandIndex);
}

void TrackControlChain::setEqBandQ(int moduleIndex, int bandIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackEqBandQ(selectedTrack, moduleIndex, bandIndex, value);
    else
        engine.setSendBusEqBandQ(selectedSendBus, moduleIndex, bandIndex, value);
}

float TrackControlChain::getEqBandFrequency(int moduleIndex, int bandIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackEqBandFrequency(selectedTrack, moduleIndex, bandIndex)
                           : engine.getSendBusEqBandFrequency(selectedSendBus, moduleIndex, bandIndex);
}

void TrackControlChain::setEqBandFrequency(int moduleIndex, int bandIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackEqBandFrequency(selectedTrack, moduleIndex, bandIndex, value);
    else
        engine.setSendBusEqBandFrequency(selectedSendBus, moduleIndex, bandIndex, value);
}

float TrackControlChain::getCompressorThresholdDb(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackCompressorThresholdDb(selectedTrack, moduleIndex)
                           : engine.getSendBusCompressorThresholdDb(selectedSendBus, moduleIndex);
}

void TrackControlChain::setCompressorThresholdDb(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackCompressorThresholdDb(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusCompressorThresholdDb(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getCompressorRatio(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackCompressorRatio(selectedTrack, moduleIndex)
                           : engine.getSendBusCompressorRatio(selectedSendBus, moduleIndex);
}

void TrackControlChain::setCompressorRatio(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackCompressorRatio(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusCompressorRatio(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getCompressorAttackMs(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackCompressorAttackMs(selectedTrack, moduleIndex)
                           : engine.getSendBusCompressorAttackMs(selectedSendBus, moduleIndex);
}

void TrackControlChain::setCompressorAttackMs(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackCompressorAttackMs(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusCompressorAttackMs(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getCompressorReleaseMs(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackCompressorReleaseMs(selectedTrack, moduleIndex)
                           : engine.getSendBusCompressorReleaseMs(selectedSendBus, moduleIndex);
}

void TrackControlChain::setCompressorReleaseMs(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackCompressorReleaseMs(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusCompressorReleaseMs(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getCompressorMakeupGainDb(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackCompressorMakeupGainDb(selectedTrack, moduleIndex)
                           : engine.getSendBusCompressorMakeupGainDb(selectedSendBus, moduleIndex);
}

void TrackControlChain::setCompressorMakeupGainDb(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackCompressorMakeupGainDb(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusCompressorMakeupGainDb(selectedSendBus, moduleIndex, value);
}

SaturationMode TrackControlChain::getSaturationMode(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackSaturationMode(selectedTrack, moduleIndex)
                           : engine.getSendBusSaturationMode(selectedSendBus, moduleIndex);
}

void TrackControlChain::setSaturationMode(int moduleIndex, SaturationMode mode)
{
    if (isTrackTarget())
        engine.setTrackSaturationMode(selectedTrack, moduleIndex, mode);
    else
        engine.setSendBusSaturationMode(selectedSendBus, moduleIndex, mode);
}

float TrackControlChain::getSaturationAmount(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackSaturationAmount(selectedTrack, moduleIndex)
                           : engine.getSendBusSaturationAmount(selectedSendBus, moduleIndex);
}

void TrackControlChain::setSaturationAmount(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackSaturationAmount(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusSaturationAmount(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getDelayTimeMs(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackDelayTimeMs(selectedTrack, moduleIndex)
                           : engine.getSendBusDelayTimeMs(selectedSendBus, moduleIndex);
}

void TrackControlChain::setDelayTimeMs(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackDelayTimeMs(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusDelayTimeMs(selectedSendBus, moduleIndex, value);
}

bool TrackControlChain::isDelaySyncEnabled(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.isTrackDelaySyncEnabled(selectedTrack, moduleIndex)
                           : engine.isSendBusDelaySyncEnabled(selectedSendBus, moduleIndex);
}

void TrackControlChain::setDelaySyncEnabled(int moduleIndex, bool shouldBeEnabled)
{
    if (isTrackTarget())
        engine.setTrackDelaySyncEnabled(selectedTrack, moduleIndex, shouldBeEnabled);
    else
        engine.setSendBusDelaySyncEnabled(selectedSendBus, moduleIndex, shouldBeEnabled);
}

int TrackControlChain::getDelaySyncIndex(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackDelaySyncIndex(selectedTrack, moduleIndex)
                           : engine.getSendBusDelaySyncIndex(selectedSendBus, moduleIndex);
}

void TrackControlChain::setDelaySyncIndex(int moduleIndex, int index)
{
    if (isTrackTarget())
        engine.setTrackDelaySyncIndex(selectedTrack, moduleIndex, index);
    else
        engine.setSendBusDelaySyncIndex(selectedSendBus, moduleIndex, index);
}

float TrackControlChain::getResolvedDelayTimeMs(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackResolvedDelayTimeMs(selectedTrack, moduleIndex)
                           : engine.getSendBusResolvedDelayTimeMs(selectedSendBus, moduleIndex);
}

float TrackControlChain::getDelayFeedback(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackDelayFeedback(selectedTrack, moduleIndex)
                           : engine.getSendBusDelayFeedback(selectedSendBus, moduleIndex);
}

void TrackControlChain::setDelayFeedback(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackDelayFeedback(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusDelayFeedback(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getDelayMix(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackDelayMix(selectedTrack, moduleIndex)
                           : engine.getSendBusDelayMix(selectedSendBus, moduleIndex);
}

void TrackControlChain::setDelayMix(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackDelayMix(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusDelayMix(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getReverbSize(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackReverbSize(selectedTrack, moduleIndex)
                           : engine.getSendBusReverbSize(selectedSendBus, moduleIndex);
}

void TrackControlChain::setReverbSize(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackReverbSize(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusReverbSize(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getReverbDamping(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackReverbDamping(selectedTrack, moduleIndex)
                           : engine.getSendBusReverbDamping(selectedSendBus, moduleIndex);
}

void TrackControlChain::setReverbDamping(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackReverbDamping(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusReverbDamping(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getReverbMix(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackReverbMix(selectedTrack, moduleIndex)
                           : engine.getSendBusReverbMix(selectedSendBus, moduleIndex);
}

void TrackControlChain::setReverbMix(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackReverbMix(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusReverbMix(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getPhaserRate(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackPhaserRate(selectedTrack, moduleIndex)
                           : engine.getSendBusPhaserRate(selectedSendBus, moduleIndex);
}

void TrackControlChain::setPhaserRate(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackPhaserRate(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusPhaserRate(selectedSendBus, moduleIndex, value);
}

bool TrackControlChain::isPhaserSyncEnabled(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.isTrackPhaserSyncEnabled(selectedTrack, moduleIndex)
                           : engine.isSendBusPhaserSyncEnabled(selectedSendBus, moduleIndex);
}

void TrackControlChain::setPhaserSyncEnabled(int moduleIndex, bool shouldBeEnabled)
{
    if (isTrackTarget())
        engine.setTrackPhaserSyncEnabled(selectedTrack, moduleIndex, shouldBeEnabled);
    else
        engine.setSendBusPhaserSyncEnabled(selectedSendBus, moduleIndex, shouldBeEnabled);
}

int TrackControlChain::getPhaserSyncIndex(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackPhaserSyncIndex(selectedTrack, moduleIndex)
                           : engine.getSendBusPhaserSyncIndex(selectedSendBus, moduleIndex);
}

void TrackControlChain::setPhaserSyncIndex(int moduleIndex, int index)
{
    if (isTrackTarget())
        engine.setTrackPhaserSyncIndex(selectedTrack, moduleIndex, index);
    else
        engine.setSendBusPhaserSyncIndex(selectedSendBus, moduleIndex, index);
}

float TrackControlChain::getResolvedPhaserRateHz(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackResolvedPhaserRateHz(selectedTrack, moduleIndex)
                           : engine.getSendBusResolvedPhaserRateHz(selectedSendBus, moduleIndex);
}

float TrackControlChain::getPhaserDepth(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackPhaserDepth(selectedTrack, moduleIndex)
                           : engine.getSendBusPhaserDepth(selectedSendBus, moduleIndex);
}

void TrackControlChain::setPhaserDepth(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackPhaserDepth(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusPhaserDepth(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getPhaserCentreFrequency(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackPhaserCentreFrequency(selectedTrack, moduleIndex)
                           : engine.getSendBusPhaserCentreFrequency(selectedSendBus, moduleIndex);
}

void TrackControlChain::setPhaserCentreFrequency(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackPhaserCentreFrequency(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusPhaserCentreFrequency(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getPhaserFeedback(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackPhaserFeedback(selectedTrack, moduleIndex)
                           : engine.getSendBusPhaserFeedback(selectedSendBus, moduleIndex);
}

void TrackControlChain::setPhaserFeedback(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackPhaserFeedback(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusPhaserFeedback(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getPhaserMix(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackPhaserMix(selectedTrack, moduleIndex)
                           : engine.getSendBusPhaserMix(selectedSendBus, moduleIndex);
}

void TrackControlChain::setPhaserMix(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackPhaserMix(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusPhaserMix(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getGainModuleGainDb(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackGainModuleGainDb(selectedTrack, moduleIndex)
                           : engine.getSendBusGainModuleGainDb(selectedSendBus, moduleIndex);
}

void TrackControlChain::setGainModuleGainDb(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackGainModuleGainDb(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusGainModuleGainDb(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getGainModulePan(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackGainModulePan(selectedTrack, moduleIndex)
                           : engine.getSendBusGainModulePan(selectedSendBus, moduleIndex);
}

void TrackControlChain::setGainModulePan(int moduleIndex, float value)
{
    if (isTrackTarget())
        engine.setTrackGainModulePan(selectedTrack, moduleIndex, value);
    else
        engine.setSendBusGainModulePan(selectedSendBus, moduleIndex, value);
}

float TrackControlChain::getModuleInputMeter(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackModuleInputMeter(selectedTrack, moduleIndex)
                           : engine.getSendBusModuleInputMeter(selectedSendBus, moduleIndex);
}

float TrackControlChain::getModuleOutputMeter(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackModuleOutputMeter(selectedTrack, moduleIndex)
                           : engine.getSendBusModuleOutputMeter(selectedSendBus, moduleIndex);
}

std::array<float, ModuleChain::spectrumAnalyzerBinCount> TrackControlChain::getSpectrumAnalyzerData(int moduleIndex) const noexcept
{
    return isTrackTarget() ? engine.getTrackSpectrumAnalyzerData(selectedTrack, moduleIndex)
                           : engine.getSendBusSpectrumAnalyzerData(selectedSendBus, moduleIndex);
}

void TrackControlChain::refreshFromEngine()
{
    updateAccentColours();

    if (isTrackTarget())
    {
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
    }

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        bypassButtons[(size_t) moduleIndex].setToggleState(isModuleBypassed(moduleIndex), juce::dontSendNotification);
        filterSliders[(size_t) moduleIndex].setValue(getFilterMorph(moduleIndex), juce::dontSendNotification);
        saturationAmountSliders[(size_t) moduleIndex].setValue(getSaturationAmount(moduleIndex), juce::dontSendNotification);
        saturationModeButtons[(size_t) moduleIndex].setToggleState(getSaturationMode(moduleIndex) == SaturationMode::heavy,
                                                                   juce::dontSendNotification);
        if (isDelaySyncEnabled(moduleIndex))
        {
            delaySliders[(size_t) moduleIndex][0].setRange(0.0, (double) (TapeEngine::getNumDelaySyncOptions() - 1), 1.0);
            delaySliders[(size_t) moduleIndex][0].setDoubleClickReturnValue(true, 2.0);
            delaySliders[(size_t) moduleIndex][0].setValue((double) getDelaySyncIndex(moduleIndex), juce::dontSendNotification);
            delayTimeModeButtons[(size_t) moduleIndex].setButtonText(TapeEngine::getDelaySyncLabel(getDelaySyncIndex(moduleIndex)));
        }
        else
        {
            delaySliders[(size_t) moduleIndex][0].setRange(20.0, 2000.0, 1.0);
            delaySliders[(size_t) moduleIndex][0].setDoubleClickReturnValue(true, 380.0);
            delaySliders[(size_t) moduleIndex][0].setValue(getDelayTimeMs(moduleIndex), juce::dontSendNotification);
            delayTimeModeButtons[(size_t) moduleIndex].setButtonText(juce::String((int) std::round(getDelayTimeMs(moduleIndex))) + "ms");
        }
        delaySliders[(size_t) moduleIndex][1].setValue(getDelayFeedback(moduleIndex), juce::dontSendNotification);
        delaySliders[(size_t) moduleIndex][2].setValue(getDelayMix(moduleIndex), juce::dontSendNotification);
        reverbSliders[(size_t) moduleIndex][0].setValue(getReverbSize(moduleIndex), juce::dontSendNotification);
        reverbSliders[(size_t) moduleIndex][1].setValue(getReverbDamping(moduleIndex), juce::dontSendNotification);
        reverbSliders[(size_t) moduleIndex][2].setValue(getReverbMix(moduleIndex), juce::dontSendNotification);
        if (isPhaserSyncEnabled(moduleIndex))
        {
            phaserSliders[(size_t) moduleIndex][0].setRange(0.0, (double) (TapeEngine::getNumDelaySyncOptions() - 1), 1.0);
            phaserSliders[(size_t) moduleIndex][0].setDoubleClickReturnValue(true, 2.0);
            phaserSliders[(size_t) moduleIndex][0].setValue((double) getPhaserSyncIndex(moduleIndex), juce::dontSendNotification);
            phaserRateModeButtons[(size_t) moduleIndex].setButtonText(TapeEngine::getDelaySyncLabel(getPhaserSyncIndex(moduleIndex)));
        }
        else
        {
            phaserSliders[(size_t) moduleIndex][0].setRange(0.05, 10.0, 0.01);
            phaserSliders[(size_t) moduleIndex][0].setDoubleClickReturnValue(true, 0.5);
            phaserSliders[(size_t) moduleIndex][0].setValue(getPhaserRate(moduleIndex), juce::dontSendNotification);
            phaserRateModeButtons[(size_t) moduleIndex].setButtonText(juce::String(getPhaserRate(moduleIndex), 2) + "Hz");
        }
        phaserSliders[(size_t) moduleIndex][1].setValue(getPhaserDepth(moduleIndex), juce::dontSendNotification);
        phaserSliders[(size_t) moduleIndex][2].setValue(getPhaserCentreFrequency(moduleIndex), juce::dontSendNotification);
        phaserSliders[(size_t) moduleIndex][3].setValue(getPhaserFeedback(moduleIndex), juce::dontSendNotification);
        phaserSliders[(size_t) moduleIndex][4].setValue(getPhaserMix(moduleIndex), juce::dontSendNotification);
        gainModuleSliders[(size_t) moduleIndex][0].setValue(getGainModuleGainDb(moduleIndex), juce::dontSendNotification);
        gainModuleSliders[(size_t) moduleIndex][1].setValue(getGainModulePan(moduleIndex), juce::dontSendNotification);

        compressorSliders[(size_t) moduleIndex][0].setValue(getCompressorThresholdDb(moduleIndex), juce::dontSendNotification);
        compressorSliders[(size_t) moduleIndex][1].setValue(getCompressorRatio(moduleIndex), juce::dontSendNotification);
        compressorSliders[(size_t) moduleIndex][2].setValue(getCompressorAttackMs(moduleIndex), juce::dontSendNotification);
        compressorSliders[(size_t) moduleIndex][3].setValue(getCompressorReleaseMs(moduleIndex), juce::dontSendNotification);
        compressorSliders[(size_t) moduleIndex][4].setValue(getCompressorMakeupGainDb(moduleIndex), juce::dontSendNotification);

        for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
        {
            eqGainSliders[(size_t) moduleIndex][(size_t) bandIndex].setValue(getEqBandGainDb(moduleIndex, bandIndex), juce::dontSendNotification);
            eqQSliders[(size_t) moduleIndex][(size_t) bandIndex].setValue(getEqBandQ(moduleIndex, bandIndex), juce::dontSendNotification);
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
    const auto accent = getAccentColour();
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

    for (auto& slot : delaySliders)
    {
        for (auto& slider : slot)
            slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    }

    for (auto& slot : reverbSliders)
    {
        for (auto& slider : slot)
            slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    }

    for (auto& slot : phaserSliders)
    {
        for (auto& slider : slot)
            slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    }

    for (auto& slot : gainModuleSliders)
    {
        for (auto& slider : slot)
            slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    }

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
        const auto type = getModuleType(moduleIndex);

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

            editor.setText(formatFrequencyValue(getEqBandFrequency(moduleIndex, bandIndex)), juce::dontSendNotification);
        }
    }
}

void TrackControlChain::updateModuleVisibility()
{
    const auto canAddModule = getModuleCount() < Track::maxChainModules;
    addModuleButton.setEnabled(canAddModule);
    addModuleButton.setVisible(true);
    inputSourceBox.setVisible(showsInputModule());
    inputGainSlider.setVisible(showsInputModule());

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
        const auto type = getModuleType(moduleIndex);
        const auto isVisible = type != ChainModuleType::none;

        bypassButtons[(size_t) moduleIndex].setVisible(isVisible);
        removeButtons[(size_t) moduleIndex].setVisible(isVisible);
        filterSliders[(size_t) moduleIndex].setVisible(type == ChainModuleType::filter);
        saturationModeButtons[(size_t) moduleIndex].setVisible(type == ChainModuleType::saturation);
        saturationAmountSliders[(size_t) moduleIndex].setVisible(type == ChainModuleType::saturation);
        delayTimeModeButtons[(size_t) moduleIndex].setVisible(type == ChainModuleType::delay);
        phaserRateModeButtons[(size_t) moduleIndex].setVisible(type == ChainModuleType::phaser);
        for (int controlIndex = 0; controlIndex < delayControlCount; ++controlIndex)
            delaySliders[(size_t) moduleIndex][(size_t) controlIndex].setVisible(type == ChainModuleType::delay);
        for (int controlIndex = 0; controlIndex < reverbControlCount; ++controlIndex)
            reverbSliders[(size_t) moduleIndex][(size_t) controlIndex].setVisible(type == ChainModuleType::reverb);
        for (int controlIndex = 0; controlIndex < phaserControlCount; ++controlIndex)
            phaserSliders[(size_t) moduleIndex][(size_t) controlIndex].setVisible(type == ChainModuleType::phaser);
        for (int controlIndex = 0; controlIndex < utilityControlCount; ++controlIndex)
            gainModuleSliders[(size_t) moduleIndex][(size_t) controlIndex].setVisible(type == ChainModuleType::gain);

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

