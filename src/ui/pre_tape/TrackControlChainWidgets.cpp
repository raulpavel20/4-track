#include "../../TrackControlChain.h"

#include "../../AppFonts.h"

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

TrackControlChain::BypassButton::BypassButton()
    : juce::Button({})
{
    setClickingTogglesState(true);
}

void TrackControlChain::BypassButton::setAccentColour(juce::Colour newAccentColour)
{
    accentColour = newAccentColour;
    repaint();
}

void TrackControlChain::BypassButton::paintButton(juce::Graphics& g, bool, bool)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto bypassed = getToggleState();
    const auto ellipse = bounds.reduced(0.5f);

    if (bypassed)
    {
        g.setColour(juce::Colours::black);
        g.fillEllipse(ellipse);
        g.setColour(accentColour);
        g.drawEllipse(ellipse, 1.0f);
    }
    else
    {
        g.setColour(accentColour);
        g.fillEllipse(ellipse);
    }
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

TrackControlChain::ModuleDragHandle::ModuleDragHandle(TrackControlChain&, int slotIndex)
    : slot(slotIndex)
{
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

void TrackControlChain::ModuleDragHandle::mouseDown(const juce::MouseEvent& event)
{
    dragStartPosition = event.getPosition();
}

void TrackControlChain::ModuleDragHandle::mouseDrag(const juce::MouseEvent& event)
{
    if (event.getDistanceFromDragStart() < 4)
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);

    if (container != nullptr)
        container->startDragging(juce::var(slot), this, juce::ScaledImage(), true, nullptr, &event.source);
}

TrackControlChain::ContentComponent::ContentComponent(TrackControlChain& ownerToUse)
    : owner(ownerToUse)
{
}

void TrackControlChain::ContentComponent::paint(juce::Graphics& g)
{
    owner.paintContent(g);

    const auto insertIndex = getDragOverInsertIndex();

    if (insertIndex >= 0)
    {
        const auto x = owner.getInsertionLineX(insertIndex);
        g.setColour(juce::Colours::white);
        g.drawLine((float) x, 0.0f, (float) x, (float) getHeight(), 2.0f);
    }
}

bool TrackControlChain::ContentComponent::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    if (! dragSourceDetails.description.isInt())
        return false;

    const auto slotIndex = (int) dragSourceDetails.description;
    return slotIndex >= 0 && slotIndex < Track::maxChainModules;
}

void TrackControlChain::ContentComponent::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    const auto x = dragSourceDetails.localPosition.getX();
    dragOverInsertIndex = owner.computeInsertIndexFromX(x);
    repaint();
}

void TrackControlChain::ContentComponent::itemDragMove(const SourceDetails& dragSourceDetails)
{
    const auto x = dragSourceDetails.localPosition.getX();
    const auto newIndex = owner.computeInsertIndexFromX(x);

    if (newIndex != dragOverInsertIndex)
    {
        dragOverInsertIndex = newIndex;
        repaint();
    }
}

void TrackControlChain::ContentComponent::itemDragExit(const SourceDetails&)
{
    dragOverInsertIndex = -1;
    repaint();
}

void TrackControlChain::ContentComponent::itemDropped(const SourceDetails& dragSourceDetails)
{
    const auto fromSlot = (int) dragSourceDetails.description;
    const auto x = dragSourceDetails.localPosition.getX();
    const auto moduleCount = owner.getModuleCount();

    if (moduleCount == 0)
    {
        dragOverInsertIndex = -1;
        repaint();
        return;
    }

    auto sourceVisibleIndex = -1;

    for (int visibleIndex = 0; visibleIndex < moduleCount; ++visibleIndex)
    {
        if (owner.getVisibleSlot(visibleIndex) == fromSlot)
        {
            sourceVisibleIndex = visibleIndex;
            break;
        }
    }

    if (sourceVisibleIndex < 0)
    {
        dragOverInsertIndex = -1;
        repaint();
        return;
    }

    auto targetVisibleIndex = owner.computeInsertIndexFromX(x);

    if (targetVisibleIndex > sourceVisibleIndex)
        --targetVisibleIndex;

    targetVisibleIndex = juce::jlimit(0, moduleCount - 1, targetVisibleIndex);

    const auto toSlot = owner.getVisibleSlot(targetVisibleIndex);
    owner.reorderModule(fromSlot, toSlot);
    owner.refreshFromEngine();
    dragOverInsertIndex = -1;
    repaint();
}
