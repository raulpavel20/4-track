#include "TrackControlChain.h"

#include "AppFonts.h"

#include <cmath>

namespace
{
constexpr int moduleGap = 10;
constexpr int inputModuleWidth = 120;
constexpr int filterModuleWidth = 166;
constexpr int eqModuleWidth = 280;
constexpr int compressorModuleWidth = 260;
constexpr int saturationModuleWidth = 140;
constexpr int gainModuleWidth = 78;
constexpr int moduleVerticalInset = 10;
constexpr int moduleInnerPadding = 10;
constexpr int inputKnobSize = 80;
constexpr int filterKnobSize = 72;
constexpr int eqGainSliderHeight = 80;
constexpr int eqGainSliderWidth = 18;
constexpr int eqQKnobSize = 38;
constexpr int eqFrequencyEditorHeight = 20;
constexpr int eqFrequencyEditorWidth = 38;
constexpr int compressorKnobSize = 60;
constexpr int saturationAmountKnobSize = 70;
constexpr int gainSliderHeight = 138;
constexpr int gainSliderWidth = 18;
constexpr int meterWidth = 10;
constexpr int addButtonSize = 32;
constexpr int closeButtonSize = 18;

juce::Colour getChainAccentColour(int trackIndex)
{
    static const std::array<juce::Colour, TapeEngine::numTracks> colours
    {
        juce::Colour::fromRGB(255, 92, 92),
        juce::Colour::fromRGB(255, 184, 77),
        juce::Colour::fromRGB(94, 233, 196),
        juce::Colour::fromRGB(94, 146, 255)
    };

    return colours[(size_t) juce::jlimit(0, TapeEngine::numTracks - 1, trackIndex)];
}

juce::String formatInputGainValue(float value)
{
    return juce::String(value, 2) + "x";
}

juce::String formatFilterValue(float value)
{
    if (std::abs(value) < 0.02f)
        return "BP";

    return value < 0.0f ? "LP" : "HP";
}

juce::String formatDbValue(float value)
{
    return juce::String(value, 1) + " dB";
}

juce::String formatRatioValue(float value)
{
    return juce::String(value, 1) + ":1";
}

juce::String formatMsValue(float value)
{
    return juce::String(value, value < 10.0f ? 1 : 0) + " ms";
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

float smoothStep(float edge0, float edge1, float value)
{
    const auto t = juce::jlimit(0.0f, 1.0f, (value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - (2.0f * t));
}

float getFilterVisualizerResponse(float frequency, float morph)
{
    const auto lowpassResponse = 0.12f + (0.76f * (1.0f - smoothStep(0.48f, 0.78f, frequency)));
    const auto highpassResponse = 0.12f + (0.76f * smoothStep(0.22f, 0.52f, frequency));
    const auto bandRise = smoothStep(0.18f, 0.38f, frequency);
    const auto bandFall = 1.0f - smoothStep(0.62f, 0.82f, frequency);
    const auto bandpassResponse = 0.12f + (0.76f * (bandRise * bandFall));

    if (morph < 0.0f)
        return juce::jmap(morph + 1.0f, lowpassResponse, bandpassResponse);

    return juce::jmap(morph, bandpassResponse, highpassResponse);
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

int getModuleWidth(ChainModuleType type)
{
    switch (type)
    {
        case ChainModuleType::filter:
            return filterModuleWidth;
        case ChainModuleType::eq:
            return eqModuleWidth;
        case ChainModuleType::compressor:
            return compressorModuleWidth;
        case ChainModuleType::saturation:
            return saturationModuleWidth;
        case ChainModuleType::gain:
            return gainModuleWidth;
        case ChainModuleType::none:
            break;
    }

    return filterModuleWidth;
}

juce::String getModuleTitle(ChainModuleType type)
{
    switch (type)
    {
        case ChainModuleType::filter:
            return "Filter";
        case ChainModuleType::eq:
            return "EQ";
        case ChainModuleType::compressor:
            return "Compressor";
        case ChainModuleType::saturation:
            return "Saturation";
        case ChainModuleType::gain:
            return "Gain";
        case ChainModuleType::none:
            break;
    }

    return {};
}

void drawVerticalSliderTrack(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto x = (float) bounds.getCentreX();
    const auto top = (float) bounds.getY() + 4.0f;
    const auto bottom = (float) bounds.getBottom() - 4.0f;
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawLine(x, top, x, bottom, 2.0f);
}
}

TrackControlChain::CloseButton::CloseButton()
    : juce::Button({})
{
}

void TrackControlChain::CloseButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat();
    auto background = juce::Colours::black;

    if (isButtonDown)
        background = juce::Colours::white.withAlpha(0.18f);
    else if (isMouseOverButton)
        background = juce::Colours::white.withAlpha(0.08f);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colours::white.withAlpha(0.26f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto iconBounds = bounds.reduced(5.0f);
    juce::Path cross;
    cross.startNewSubPath(iconBounds.getTopLeft());
    cross.lineTo(iconBounds.getBottomRight());
    cross.startNewSubPath(iconBounds.getTopRight());
    cross.lineTo(iconBounds.getBottomLeft());

    g.setColour(juce::Colours::white);
    g.strokePath(cross, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

TrackControlChain::SaturationModeButton::SaturationModeButton()
    : juce::Button({})
{
}

void TrackControlChain::SaturationModeButton::setAccentColour(juce::Colour newAccentColour)
{
    accentColour = newAccentColour;
    repaint();
}

void TrackControlChain::SaturationModeButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();
    const auto isOn = getToggleState();
    auto background = isOn ? accentColour : juce::Colours::black;

    if (isButtonDown)
        background = background.brighter(0.08f);
    else if (isMouseOverButton)
        background = background.brighter(0.04f);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(isOn ? juce::Colours::black : juce::Colours::white.withAlpha(0.24f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
    g.setColour(isOn ? juce::Colours::black : juce::Colours::white);
    g.setFont(isOn ? AppFonts::getFont(12.0f).boldened() : AppFonts::getFont(12.0f));
    g.drawText("Heavy", getLocalBounds(), juce::Justification::centred, false);
}

TrackControlChain::ContentComponent::ContentComponent(TrackControlChain& ownerToUse)
    : owner(ownerToUse)
{
}

void TrackControlChain::ContentComponent::paint(juce::Graphics& g)
{
    owner.paintContent(g);
}

TrackControlChain::TrackControlChain(TapeEngine& engineToUse)
    : engine(engineToUse),
      contentComponent(*this)
{
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

void TrackControlChain::setInputOptions(const juce::StringArray& options)
{
    inputOptions = options;
    inputSourceBox.clear(juce::dontSendNotification);

    for (int index = 0; index < inputOptions.size(); ++index)
        inputSourceBox.addItem(inputOptions[index], index + 1);

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
        const auto selectedSource = juce::jlimit(0, inputSourceBox.getNumItems() - 1, engine.getTrackInputSource(selectedTrack));
        inputSourceBox.setSelectedId(selectedSource + 1, juce::dontSendNotification);
    }

    for (int moduleIndex = 0; moduleIndex < Track::maxChainModules; ++moduleIndex)
    {
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

void TrackControlChain::layoutContent()
{
    const auto viewportBounds = modulesViewport.getBounds();
    const auto contentHeight = juce::jmax(1, viewportBounds.getHeight());

    auto x = 0;
    const auto inputBounds = juce::Rectangle<int>(x, 0, inputModuleWidth, contentHeight);
    x += inputBounds.getWidth() + moduleGap;

    for (int visibleIndex = 0; visibleIndex < visibleModuleCount; ++visibleIndex)
    {
        const auto width = getModuleWidth(visibleTypes[(size_t) visibleIndex]);
        moduleBounds[(size_t) visibleIndex] = juce::Rectangle<int>(x, 0, width, contentHeight);
        x += width + moduleGap;
    }

    const auto addBounds = juce::Rectangle<int>(x, (contentHeight - addButtonSize) / 2, addButtonSize, addButtonSize);
    const auto requiredWidth = addBounds.getRight();
    contentComponent.setSize(juce::jmax(viewportBounds.getWidth(), requiredWidth), contentHeight);

    auto inputLayout = inputBounds.reduced(moduleInnerPadding, moduleVerticalInset);
    inputLayout.removeFromTop(26);
    inputSourceBox.setBounds(inputLayout.removeFromTop(28));
    inputLayout.removeFromTop(6);
    inputGainSlider.setBounds(inputLayout.getCentreX() - (inputKnobSize / 2),
                              inputLayout.getY() + 16,
                              inputKnobSize,
                              inputKnobSize);

    for (int visibleIndex = 0; visibleIndex < visibleModuleCount; ++visibleIndex)
    {
        const auto slot = visibleSlots[(size_t) visibleIndex];
        const auto type = visibleTypes[(size_t) visibleIndex];
        auto moduleArea = moduleBounds[(size_t) visibleIndex].reduced(moduleInnerPadding, moduleVerticalInset);

        removeButtons[(size_t) slot].setBounds(moduleArea.getRight() - closeButtonSize,
                                               moduleArea.getY() + 1,
                                               closeButtonSize,
                                               closeButtonSize);

        if (type == ChainModuleType::filter)
        {
            auto content = moduleArea;
            content.removeFromTop(18);
            const auto visualizerHeight = (int) std::round((float) content.getHeight() * 0.33f);
            content.removeFromTop(visualizerHeight + 10);
            filterSliders[(size_t) slot].setBounds(content.getCentreX() - (filterKnobSize / 2),
                                                   content.getBottom() - filterKnobSize - 8,
                                                   filterKnobSize,
                                                   filterKnobSize);
        }
        else if (type == ChainModuleType::eq)
        {
            auto content = moduleArea;
            content.removeFromTop(16);
            const auto columnWidth = content.getWidth() / Track::maxEqBands;

            for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
            {
                auto column = content.removeFromLeft(columnWidth);
                auto sliderBounds = juce::Rectangle<int>(column.getCentreX() - (eqGainSliderWidth / 2),
                                                         column.getY() + 18,
                                                         eqGainSliderWidth,
                                                         eqGainSliderHeight);
                eqGainSliders[(size_t) slot][(size_t) bandIndex].setBounds(sliderBounds);

                auto qBounds = juce::Rectangle<int>(column.getCentreX() - (eqQKnobSize / 2),
                                                    sliderBounds.getBottom() + 16,
                                                    eqQKnobSize,
                                                    eqQKnobSize);
                eqQSliders[(size_t) slot][(size_t) bandIndex].setBounds(qBounds);

                eqFrequencyEditors[(size_t) slot][(size_t) bandIndex].setBounds(column.getCentreX() - (eqFrequencyEditorWidth / 2),
                                                                                qBounds.getBottom() - 2,
                                                                                eqFrequencyEditorWidth,
                                                                                eqFrequencyEditorHeight);
            }
        }
        else if (type == ChainModuleType::compressor)
        {
            auto content = moduleArea;
            content.removeFromTop(34);
            const auto inputMeterSpace = meterWidth + 14;
            content.removeFromLeft(inputMeterSpace);
            auto makeupColumn = content.removeFromRight(meterWidth + 20);
            const auto makeupSliderX = makeupColumn.getX() - 18;
            auto knobArea = content;
            const auto rowHeight = compressorKnobSize + 12;
            auto topRow = knobArea.removeFromTop(rowHeight);
            knobArea.removeFromTop(6);
            auto bottomRow = knobArea.removeFromTop(rowHeight);
            const auto knobColumnWidth = knobArea.getWidth() / 2;
            const auto topOffset = knobColumnWidth / 4;
            const auto bottomOffset = knobColumnWidth / 2 + 3;
            constexpr int inwardNudge = 8;
            constexpr int rightKnobLeftShift = 20;

            compressorSliders[(size_t) slot][0].setBounds(topRow.getX() + topOffset - (compressorKnobSize / 2),
                                                          topRow.getY(),
                                                          compressorKnobSize,
                                                          compressorKnobSize);
            compressorSliders[(size_t) slot][1].setBounds(topRow.getX() + topOffset + knobColumnWidth - (compressorKnobSize / 2) - inwardNudge - rightKnobLeftShift,
                                                          topRow.getY(),
                                                          compressorKnobSize,
                                                          compressorKnobSize);
            compressorSliders[(size_t) slot][2].setBounds(bottomRow.getX() + bottomOffset - (compressorKnobSize / 2) + inwardNudge,
                                                          bottomRow.getY(),
                                                          compressorKnobSize,
                                                          compressorKnobSize);
            compressorSliders[(size_t) slot][3].setBounds(bottomRow.getX() + bottomOffset + knobColumnWidth - (compressorKnobSize / 2) - rightKnobLeftShift,
                                                          bottomRow.getY(),
                                                          compressorKnobSize,
                                                          compressorKnobSize);
            compressorSliders[(size_t) slot][4].setBounds(makeupSliderX,
                                                          content.getCentreY() - 72,
                                                          18,
                                                          128);
        }
        else if (type == ChainModuleType::saturation)
        {
            auto content = moduleArea;
            content.removeFromTop(34);
            content.removeFromLeft(meterWidth + 14);
            content.removeFromRight(meterWidth + 14);
            auto centerArea = content.reduced(6, 4);
            saturationModeButtons[(size_t) slot].setBounds(centerArea.getCentreX() - 34,
                                                           centerArea.getY() + 6,
                                                           68,
                                                           20);
            centerArea.removeFromTop(32);
            saturationAmountSliders[(size_t) slot].setBounds(centerArea.getCentreX() - (saturationAmountKnobSize / 2),
                                                             centerArea.getY() + 16,
                                                             saturationAmountKnobSize,
                                                             saturationAmountKnobSize);
        }
        else if (type == ChainModuleType::gain)
        {
            auto content = moduleArea;
            content.removeFromTop(24);
            gainModuleSliders[(size_t) slot].setBounds(content.getCentreX() - (gainSliderWidth / 2),
                                                       content.getY() + 6,
                                                       gainSliderWidth,
                                                       gainSliderHeight);
        }
    }

    addModuleButton.setBounds(addBounds);
}

void TrackControlChain::paintContent(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    const auto accent = getChainAccentColour(selectedTrack);
    const auto inputBounds = getInputModuleBounds();

    g.setColour(accent);
    g.drawRoundedRectangle(inputBounds.toFloat(), 14.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(16.0f));
    g.drawText("Input",
               inputBounds.withTrimmedTop(10).removeFromTop(18).reduced(12, 0),
               juce::Justification::centredLeft,
               false);

    g.setFont(AppFonts::getFont(13.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));
    g.drawText(formatInputGainValue((float) inputGainSlider.getValue()),
               juce::Rectangle<int>(inputBounds.getX() + 12,
                                    inputGainSlider.getBounds().getBottom() - 8,
                                    inputBounds.getWidth() - 24,
                                    16),
               juce::Justification::centred,
               false);

    static const std::array<juce::String, compressorControlCount> compressorLabels { "Thr", "Rat", "Atk", "Rel", "Make" };

    for (int visibleIndex = 0; visibleIndex < visibleModuleCount; ++visibleIndex)
    {
        const auto slot = visibleSlots[(size_t) visibleIndex];
        const auto type = visibleTypes[(size_t) visibleIndex];
        const auto bounds = moduleBounds[(size_t) visibleIndex];
        auto titleBounds = bounds.withTrimmedTop(10).removeFromTop(18).reduced(12, 0);
        titleBounds.removeFromRight(closeButtonSize + 8);

        g.setColour(accent);
        g.drawRoundedRectangle(bounds.toFloat(), 14.0f, 2.0f);

        g.setColour(juce::Colours::white);
        g.setFont(AppFonts::getFont(16.0f));
        g.drawText(getModuleTitle(type), titleBounds, juce::Justification::centredLeft, false);

        if (type == ChainModuleType::filter)
        {
            g.setFont(AppFonts::getFont(13.0f));
            g.setColour(juce::Colours::white.withAlpha(0.72f));
            g.drawText(formatFilterValue((float) filterSliders[(size_t) slot].getValue()),
                       juce::Rectangle<int>(bounds.getX() + 12,
                                            filterSliders[(size_t) slot].getBounds().getBottom() - 8,
                                            bounds.getWidth() - 24,
                                            16),
                       juce::Justification::centred,
                       false);

            auto visualizerBounds = bounds.reduced(moduleInnerPadding);
            visualizerBounds.removeFromTop(24);
            visualizerBounds.setHeight((int) std::round((float) bounds.getHeight() * 0.33f));
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.drawRoundedRectangle(visualizerBounds.toFloat(), 10.0f, 1.0f);

            juce::Path responsePath;
            const auto morph = engine.getTrackFilterMorph(selectedTrack, slot);

            for (int x = 0; x < visualizerBounds.getWidth(); ++x)
            {
                const auto t = visualizerBounds.getWidth() > 1 ? (float) x / (float) (visualizerBounds.getWidth() - 1) : 0.0f;
                const auto response = getFilterVisualizerResponse(t, morph);
                const auto point = juce::Point<float>((float) visualizerBounds.getX() + (float) x,
                                                      (float) visualizerBounds.getBottom()
                                                          - ((float) visualizerBounds.getHeight() * response));

                if (x == 0)
                    responsePath.startNewSubPath(point);
                else
                    responsePath.lineTo(point);
            }

            g.setColour(accent);
            g.strokePath(responsePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (type == ChainModuleType::eq)
        {
            g.setFont(AppFonts::getFont(11.0f));
            g.setColour(juce::Colours::white.withAlpha(0.7f));

            for (int bandIndex = 0; bandIndex < Track::maxEqBands; ++bandIndex)
            {
                const auto sliderBounds = eqGainSliders[(size_t) slot][(size_t) bandIndex].getBounds();
                drawVerticalSliderTrack(g, sliderBounds);
                g.drawText(formatDbValue((float) eqGainSliders[(size_t) slot][(size_t) bandIndex].getValue()),
                           juce::Rectangle<int>(sliderBounds.getX() - 12, sliderBounds.getBottom() + 2, sliderBounds.getWidth() + 24, 14),
                           juce::Justification::centred,
                           false);
            }
        }
        else if (type == ChainModuleType::compressor)
        {
            auto moduleContent = bounds.reduced(moduleInnerPadding);
            moduleContent.removeFromTop(26);
            const auto meterHeight = 140;
            const auto inputMeterBounds = juce::Rectangle<int>(moduleContent.getX(), moduleContent.getY() + 8, meterWidth, meterHeight);
            const auto outputMeterBounds = juce::Rectangle<int>(bounds.getRight() - moduleInnerPadding - meterWidth,
                                                                moduleContent.getY() + 8,
                                                                meterWidth,
                                                                meterHeight);

            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.drawRoundedRectangle(inputMeterBounds.toFloat(), 5.0f, 1.0f);
            g.drawRoundedRectangle(outputMeterBounds.toFloat(), 5.0f, 1.0f);
            const auto inputLevel = juce::jlimit(0.0f, 1.0f, engine.getTrackModuleInputMeter(selectedTrack, slot));
            const auto outputLevel = juce::jlimit(0.0f, 1.0f, engine.getTrackModuleOutputMeter(selectedTrack, slot));
            g.setColour(accent);
            g.fillRoundedRectangle(juce::Rectangle<float>((float) inputMeterBounds.getX() + 2.0f,
                                                          (float) inputMeterBounds.getBottom() - 2.0f - ((float) (inputMeterBounds.getHeight() - 4) * inputLevel),
                                                          (float) inputMeterBounds.getWidth() - 4.0f,
                                                          (float) (inputMeterBounds.getHeight() - 4) * inputLevel),
                                   3.0f);
            g.fillRoundedRectangle(juce::Rectangle<float>((float) outputMeterBounds.getX() + 2.0f,
                                                          (float) outputMeterBounds.getBottom() - 2.0f - ((float) (outputMeterBounds.getHeight() - 4) * outputLevel),
                                                          (float) outputMeterBounds.getWidth() - 4.0f,
                                                          (float) (outputMeterBounds.getHeight() - 4) * outputLevel),
                                   3.0f);

            g.setFont(AppFonts::getFont(11.0f));
            g.setColour(juce::Colours::white.withAlpha(0.7f));

            for (int controlIndex = 0; controlIndex < compressorControlCount; ++controlIndex)
            {
                const auto boundsForSlider = compressorSliders[(size_t) slot][(size_t) controlIndex].getBounds();
                auto valueText = juce::String();

                if (controlIndex == 0 || controlIndex == 4)
                    valueText = formatDbValue((float) compressorSliders[(size_t) slot][(size_t) controlIndex].getValue());
                else if (controlIndex == 1)
                    valueText = formatRatioValue((float) compressorSliders[(size_t) slot][(size_t) controlIndex].getValue());
                else
                    valueText = formatMsValue((float) compressorSliders[(size_t) slot][(size_t) controlIndex].getValue());

                if (controlIndex == 4)
                {
                    drawVerticalSliderTrack(g, boundsForSlider);
                    g.drawText(compressorLabels[(size_t) controlIndex],
                               juce::Rectangle<int>(boundsForSlider.getX() - 18, boundsForSlider.getY() - 10, boundsForSlider.getWidth() + 36, 12),
                               juce::Justification::centred,
                               false);
                    g.drawText(valueText,
                               juce::Rectangle<int>(boundsForSlider.getX() - 24, boundsForSlider.getBottom() + 2, boundsForSlider.getWidth() + 48, 14),
                               juce::Justification::centred,
                               false);
                }
                else
                {
                    g.drawText(compressorLabels[(size_t) controlIndex],
                               juce::Rectangle<int>(boundsForSlider.getX() - 12, boundsForSlider.getY() - 8, boundsForSlider.getWidth() + 24, 12),
                               juce::Justification::centred,
                               false);
                    g.drawText(valueText,
                               juce::Rectangle<int>(boundsForSlider.getX() - 18, boundsForSlider.getBottom() - 6, boundsForSlider.getWidth() + 36, 14),
                               juce::Justification::centred,
                               false);
                }
            }
        }
        else if (type == ChainModuleType::saturation)
        {
            const auto meterHeight = 140;
            const auto inputMeterBounds = juce::Rectangle<int>(bounds.getX() + moduleInnerPadding,
                                                               bounds.getY() + 44,
                                                               meterWidth,
                                                               meterHeight);
            const auto outputMeterBounds = juce::Rectangle<int>(bounds.getRight() - moduleInnerPadding - meterWidth,
                                                                bounds.getY() + 44,
                                                                meterWidth,
                                                                meterHeight);
            const auto inputLevel = juce::jlimit(0.0f, 1.0f, engine.getTrackModuleInputMeter(selectedTrack, slot));
            const auto outputLevel = juce::jlimit(0.0f, 1.0f, engine.getTrackModuleOutputMeter(selectedTrack, slot));

            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.drawRoundedRectangle(inputMeterBounds.toFloat(), 5.0f, 1.0f);
            g.drawRoundedRectangle(outputMeterBounds.toFloat(), 5.0f, 1.0f);

            g.setColour(accent);
            g.fillRoundedRectangle(juce::Rectangle<float>((float) inputMeterBounds.getX() + 2.0f,
                                                          (float) inputMeterBounds.getBottom() - 2.0f - ((float) (inputMeterBounds.getHeight() - 4) * inputLevel),
                                                          (float) inputMeterBounds.getWidth() - 4.0f,
                                                          (float) (inputMeterBounds.getHeight() - 4) * inputLevel),
                                   3.0f);
            g.fillRoundedRectangle(juce::Rectangle<float>((float) outputMeterBounds.getX() + 2.0f,
                                                          (float) outputMeterBounds.getBottom() - 2.0f - ((float) (outputMeterBounds.getHeight() - 4) * outputLevel),
                                                          (float) outputMeterBounds.getWidth() - 4.0f,
                                                          (float) (outputMeterBounds.getHeight() - 4) * outputLevel),
                                   3.0f);

            g.setFont(AppFonts::getFont(12.0f));
            g.setColour(juce::Colours::white.withAlpha(0.72f));
            g.drawText(juce::String((float) saturationAmountSliders[(size_t) slot].getValue(), 2),
                       juce::Rectangle<int>(bounds.getX() + 28,
                                            saturationAmountSliders[(size_t) slot].getBounds().getBottom() - 6,
                                            bounds.getWidth() - 56,
                                            16),
                       juce::Justification::centred,
                       false);
        }
        else if (type == ChainModuleType::gain)
        {
            drawVerticalSliderTrack(g, gainModuleSliders[(size_t) slot].getBounds());
            g.setFont(AppFonts::getFont(12.0f));
            g.setColour(juce::Colours::white.withAlpha(0.72f));
            g.drawText(formatDbValue((float) gainModuleSliders[(size_t) slot].getValue()),
                       juce::Rectangle<int>(bounds.getX(),
                                            gainModuleSliders[(size_t) slot].getBounds().getBottom() + 4,
                                            bounds.getWidth(),
                                            16),
                       juce::Justification::centred,
                       false);
        }
    }

    const auto addBounds = getAddButtonBounds().toFloat();
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(addBounds, 10.0f, 1.0f);
}

juce::Rectangle<int> TrackControlChain::getFrameBounds() const
{
    return getLocalBounds().reduced(6);
}

juce::Rectangle<int> TrackControlChain::getViewportBounds() const
{
    return getFrameBounds().reduced(8);
}

juce::Rectangle<int> TrackControlChain::getInputModuleBounds() const
{
    return juce::Rectangle<int>(0, 0, inputModuleWidth, contentComponent.getHeight());
}

juce::Rectangle<int> TrackControlChain::getAddButtonBounds() const
{
    auto x = inputModuleWidth + moduleGap;

    for (int visibleIndex = 0; visibleIndex < visibleModuleCount; ++visibleIndex)
        x += getModuleWidth(visibleTypes[(size_t) visibleIndex]) + moduleGap;

    return juce::Rectangle<int>(x, (contentComponent.getHeight() - addButtonSize) / 2, addButtonSize, addButtonSize);
}
