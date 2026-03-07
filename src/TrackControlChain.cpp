#include "TrackControlChain.h"

#include "AppFonts.h"

namespace
{
constexpr int moduleGap = 10;
constexpr int inputModuleWidth = 154;
constexpr int filterModuleWidth = 166;
constexpr int moduleVerticalInset = 10;
constexpr int moduleInnerPadding = 10;
constexpr int inputKnobSize = 80;
constexpr int filterKnobSize = 72;
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

juce::String formatGainValue(float value)
{
    return juce::String(value, 2) + "x";
}

juce::String formatFilterValue(float value)
{
    if (std::abs(value) < 0.02f)
        return "BP";

    return value < 0.0f ? "LP" : "HP";
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

    gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setRange(0.0, 2.0, 0.01);
    gainSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                   juce::MathConstants<float>::pi * 2.8f,
                                   true);
    gainSlider.setDoubleClickReturnValue(true, 1.0);
    gainSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha(0.18f));
    gainSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    gainSlider.onValueChange = [this]
    {
        engine.setTrackInputGain(selectedTrack, (float) gainSlider.getValue());
        contentComponent.repaint();
    };
    contentComponent.addAndMakeVisible(gainSlider);

    addModuleButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    addModuleButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addModuleButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::black);
    addModuleButton.onClick = [safeThis = juce::Component::SafePointer<TrackControlChain>(this)]
    {
        if (safeThis == nullptr)
            return;

        juce::PopupMenu menu;
        menu.addItem(1, "Filter");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&safeThis->addModuleButton),
                           [safeThis](int result)
                           {
                               if (safeThis == nullptr || result != 1)
                                   return;

                               safeThis->engine.addTrackFilterModule(safeThis->selectedTrack);
                               safeThis->refreshFromEngine();
                           });
    };
    contentComponent.addAndMakeVisible(addModuleButton);

    for (int moduleIndex = 0; moduleIndex < Track::maxFilterModules; ++moduleIndex)
    {
        auto& slider = filterSliders[(size_t) moduleIndex];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(-1.0, 1.0, 0.01);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                   juce::MathConstants<float>::pi * 2.8f,
                                   true);
        slider.setDoubleClickReturnValue(true, 0.0);
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha(0.18f));
        slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        slider.onValueChange = [this, moduleIndex]
        {
            engine.setTrackFilterMorph(selectedTrack, moduleIndex, (float) filterSliders[(size_t) moduleIndex].getValue());
            contentComponent.repaint();
        };
        contentComponent.addAndMakeVisible(slider);

        auto& button = removeButtons[(size_t) moduleIndex];
        button.onClick = [this, moduleIndex]
        {
            engine.removeTrackFilterModule(selectedTrack, moduleIndex);
            refreshFromEngine();
        };
        contentComponent.addAndMakeVisible(button);
    }

    updateSliderColours();
    refreshFromEngine();
}

void TrackControlChain::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(getFrameBounds().toFloat(), 18.0f, 1.0f);
}

void TrackControlChain::paintContent(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    const auto accent = getChainAccentColour(selectedTrack);
    const auto inputModule = getInputModuleBounds();

    g.setColour(accent);
    g.drawRoundedRectangle(inputModule.toFloat(), 14.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(16.0f));
    g.drawText("Input",
               inputModule.withTrimmedTop(10).removeFromTop(18).reduced(12, 0),
               juce::Justification::centredLeft,
               false);

    g.setFont(AppFonts::getFont(13.0f));
    g.setColour(juce::Colours::white.withAlpha(0.72f));
    g.drawText(formatGainValue((float) gainSlider.getValue()),
               juce::Rectangle<int>(inputModule.getX() + 12,
                                    gainSlider.getBounds().getBottom() - 8,
                                    inputModule.getWidth() - 24,
                                    16),
               juce::Justification::centred,
               false);

    const auto activeFilterCount = getActiveFilterModuleCount();

    for (int visibleIndex = 0; visibleIndex < activeFilterCount; ++visibleIndex)
    {
        const auto slot = getActiveFilterSlot(visibleIndex);

        if (slot < 0)
            continue;

        const auto filterModule = getFilterModuleBounds(visibleIndex);
        g.setColour(accent);
        g.drawRoundedRectangle(filterModule.toFloat(), 14.0f, 2.0f);

        g.setColour(juce::Colours::white);
        g.setFont(AppFonts::getFont(16.0f));
        auto titleBounds = filterModule.withTrimmedTop(10).removeFromTop(18).reduced(12, 0);
        titleBounds.removeFromRight(closeButtonSize + 8);
        g.drawText("Filter",
                   titleBounds,
                   juce::Justification::centredLeft,
                   false);

        g.setFont(AppFonts::getFont(13.0f));
        g.setColour(juce::Colours::white.withAlpha(0.72f));
        g.drawText(formatFilterValue((float) filterSliders[(size_t) slot].getValue()),
                   juce::Rectangle<int>(filterModule.getX() + 12,
                                        filterSliders[(size_t) slot].getBounds().getBottom() - 8,
                                        filterModule.getWidth() - 24,
                                        16),
                   juce::Justification::centred,
                   false);

        auto visualizerBounds = filterModule.reduced(moduleInnerPadding);
        visualizerBounds.removeFromTop(24);
        visualizerBounds.setHeight((int) std::round((float) filterModule.getHeight() * 0.33f));

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

    const auto addBounds = getAddButtonBounds().toFloat();
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawRoundedRectangle(addBounds, 10.0f, 1.0f);
}

void TrackControlChain::resized()
{
    modulesViewport.setBounds(getViewportBounds());
    layoutContent();
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
    updateSliderColours();
    gainSlider.setValue(engine.getTrackInputGain(selectedTrack), juce::dontSendNotification);

    for (int moduleIndex = 0; moduleIndex < Track::maxFilterModules; ++moduleIndex)
        filterSliders[(size_t) moduleIndex].setValue(engine.getTrackFilterMorph(selectedTrack, moduleIndex), juce::dontSendNotification);

    if (inputSourceBox.getNumItems() > 0)
    {
        const auto selectedSource = juce::jlimit(0, inputSourceBox.getNumItems() - 1, engine.getTrackInputSource(selectedTrack));
        inputSourceBox.setSelectedId(selectedSource + 1, juce::dontSendNotification);
    }

    updateModuleVisibility();
    layoutContent();
    repaint();
    contentComponent.repaint();
}

void TrackControlChain::updateSliderColours()
{
    const auto accent = getChainAccentColour(selectedTrack);
    gainSlider.setColour(juce::Slider::rotarySliderFillColourId, accent);

    for (auto& slider : filterSliders)
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
}

void TrackControlChain::updateModuleVisibility()
{
    const auto canAddModule = engine.getTrackFilterModuleCount(selectedTrack) < Track::maxFilterModules;
    addModuleButton.setEnabled(canAddModule);
    addModuleButton.setVisible(true);

    for (int moduleIndex = 0; moduleIndex < Track::maxFilterModules; ++moduleIndex)
    {
        const auto isEnabled = engine.isTrackFilterModuleEnabled(selectedTrack, moduleIndex);
        filterSliders[(size_t) moduleIndex].setVisible(isEnabled);
        removeButtons[(size_t) moduleIndex].setVisible(isEnabled);
    }
}

void TrackControlChain::layoutContent()
{
    const auto viewportBounds = modulesViewport.getBounds();
    const auto contentHeight = juce::jmax(1, viewportBounds.getHeight());
    contentComponent.setSize(juce::jmax(1, viewportBounds.getWidth()), contentHeight);

    const auto requiredWidth = getAddButtonBounds().getRight();
    contentComponent.setSize(juce::jmax(viewportBounds.getWidth(), requiredWidth), contentHeight);

    auto inputModule = getInputModuleBounds().reduced(moduleInnerPadding, moduleVerticalInset);
    inputModule.removeFromTop(18);
    inputSourceBox.setBounds(inputModule.removeFromTop(28));
    inputModule.removeFromTop(6);
    gainSlider.setBounds(juce::Rectangle<int>(inputModule.getCentreX() - (inputKnobSize / 2),
                                              inputModule.getY() + 16,
                                              inputKnobSize,
                                              inputKnobSize));

    for (int moduleIndex = 0; moduleIndex < Track::maxFilterModules; ++moduleIndex)
    {
        const auto visibleIndex = getVisibleIndexForSlot(moduleIndex);

        if (visibleIndex < 0)
            continue;

        auto filterModule = getFilterModuleBounds(visibleIndex).reduced(moduleInnerPadding, moduleVerticalInset);
        removeButtons[(size_t) moduleIndex].setBounds(filterModule.getRight() - closeButtonSize,
                                                      filterModule.getY() + 1,
                                                      closeButtonSize,
                                                      closeButtonSize);
        filterModule.removeFromTop(18);
        const auto visualizerHeight = (int) std::round((float) filterModule.getHeight() * 0.33f);
        filterModule.removeFromTop(visualizerHeight + 10);
        filterSliders[(size_t) moduleIndex].setBounds(juce::Rectangle<int>(filterModule.getCentreX() - (filterKnobSize / 2),
                                                                           filterModule.getBottom() - filterKnobSize - 8,
                                                                           filterKnobSize,
                                                                           filterKnobSize));
    }

    addModuleButton.setBounds(getAddButtonBounds());
}

int TrackControlChain::getActiveFilterModuleCount() const noexcept
{
    return engine.getTrackFilterModuleCount(selectedTrack);
}

int TrackControlChain::getActiveFilterSlot(int visibleIndex) const noexcept
{
    auto currentVisibleIndex = 0;

    for (int slot = 0; slot < Track::maxFilterModules; ++slot)
    {
        if (! engine.isTrackFilterModuleEnabled(selectedTrack, slot))
            continue;

        if (currentVisibleIndex == visibleIndex)
            return slot;

        ++currentVisibleIndex;
    }

    return -1;
}

int TrackControlChain::getVisibleIndexForSlot(int slot) const noexcept
{
    auto visibleIndex = 0;

    for (int currentSlot = 0; currentSlot < Track::maxFilterModules; ++currentSlot)
    {
        if (! engine.isTrackFilterModuleEnabled(selectedTrack, currentSlot))
            continue;

        if (currentSlot == slot)
            return visibleIndex;

        ++visibleIndex;
    }

    return -1;
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

juce::Rectangle<int> TrackControlChain::getFilterModuleBounds(int visibleIndex) const
{
    const auto x = inputModuleWidth + moduleGap + (visibleIndex * (filterModuleWidth + moduleGap));
    return juce::Rectangle<int>(x, 0, filterModuleWidth, contentComponent.getHeight());
}

juce::Rectangle<int> TrackControlChain::getAddButtonBounds() const
{
    const auto x = inputModuleWidth + moduleGap + (getActiveFilterModuleCount() * (filterModuleWidth + moduleGap));
    const auto y = (contentComponent.getHeight() - addButtonSize) / 2;
    return juce::Rectangle<int>(x, y, addButtonSize, addButtonSize);
}
