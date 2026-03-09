#pragma once

#include <JuceHeader.h>

#include "AppFonts.h"

class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AppLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.18f));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::arrowColourId, juce::Colours::white);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colours::black);
        setColour(juce::PopupMenu::textColourId, juce::Colours::white);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::white.withAlpha(0.12f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::AlertWindow::backgroundColourId, juce::Colours::black);
        setColour(juce::AlertWindow::textColourId, juce::Colours::white);
        setColour(juce::AlertWindow::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override
    {
        return AppFonts::getTypeface();
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return AppFonts::getFont(14.0f);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(12, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont(AppFonts::getFont(14.0f));
    }

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool,
                      int,
                      int,
                      int,
                      int,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRect(bounds);
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRect(bounds, 1.0f);

        juce::Path arrow;
        const auto centreX = (float) width - 16.0f;
        const auto centreY = (float) height * 0.5f;
        arrow.startNewSubPath(centreX - 4.0f, centreY - 2.0f);
        arrow.lineTo(centreX, centreY + 2.5f);
        arrow.lineTo(centreX + 4.0f, centreY - 2.0f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getPopupMenuFont() override
    {
        return AppFonts::getFont(13.0f);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        auto bounds = juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
        g.setColour(findColour(juce::PopupMenu::backgroundColourId));
        g.fillRect(bounds);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawRect(bounds, 1.0f);
    }

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override
    {
        juce::ignoreUnused(isTicked, shortcutKeyText, icon);

        if (isSeparator)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawLine((float) area.getX() + 8.0f,
                       (float) area.getCentreY(),
                       (float) area.getRight() - 8.0f,
                       (float) area.getCentreY(),
                       1.0f);
            return;
        }

        auto itemArea = area.reduced(4, 1).toFloat();

        if (isHighlighted && isActive)
        {
            g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect(itemArea);
        }

        auto colour = textColour != nullptr ? *textColour
                                            : (isHighlighted ? findColour(juce::PopupMenu::highlightedTextColourId)
                                                             : findColour(juce::PopupMenu::textColourId));

        if (! isActive)
            colour = colour.withAlpha(0.32f);

        g.setColour(colour);
        g.setFont(AppFonts::getFont(14.0f));
        g.drawText(text,
                   area.reduced(12, 0),
                   juce::Justification::centredLeft,
                   true);

        if (hasSubMenu)
        {
            juce::Path arrow;
            const auto centreX = (float) area.getRight() - 12.0f;
            const auto centreY = (float) area.getCentreY();
            arrow.startNewSubPath(centreX - 2.0f, centreY - 4.0f);
            arrow.lineTo(centreX + 2.5f, centreY);
            arrow.lineTo(centreX - 2.0f, centreY + 4.0f);
            g.strokePath(arrow, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    juce::AlertWindow* createAlertWindow(const juce::String& title,
                                         const juce::String& message,
                                         const juce::String& button1,
                                         const juce::String& button2,
                                         const juce::String& button3,
                                         juce::MessageBoxIconType iconType,
                                         int numButtons,
                                         juce::Component* associatedComponent) override
    {
        auto* window = juce::LookAndFeel_V4::createAlertWindow(title,
                                                               message,
                                                               button1,
                                                               button2,
                                                               button3,
                                                               iconType,
                                                               numButtons,
                                                               associatedComponent);

        window->setColour(juce::AlertWindow::backgroundColourId, findColour(juce::AlertWindow::backgroundColourId));
        window->setColour(juce::AlertWindow::textColourId, findColour(juce::AlertWindow::textColourId));
        window->setColour(juce::AlertWindow::outlineColourId, findColour(juce::AlertWindow::outlineColourId));

        for (int index = 0; index < window->getNumButtons(); ++index)
        {
            if (auto* button = dynamic_cast<juce::TextButton*>(window->getButton(index)))
            {
                button->setColour(juce::TextButton::buttonColourId, juce::Colours::black);
                button->setColour(juce::TextButton::buttonOnColourId, juce::Colours::black);
                button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
                button->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            }
        }

        for (int childIndex = 0; childIndex < window->getNumChildComponents(); ++childIndex)
        {
            if (auto* textEditor = dynamic_cast<juce::TextEditor*>(window->getChildComponent(childIndex)))
            {
                textEditor->setJustification(juce::Justification::centred);
                textEditor->setIndents(0, 0);
                textEditor->setScrollbarsShown(false);
            }
        }

        return window;
    }

    void drawAlertBox(juce::Graphics& g,
                      juce::AlertWindow& alert,
                      const juce::Rectangle<int>& textArea,
                      juce::TextLayout& textLayout) override
    {
        auto bounds = alert.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(alert.findColour(juce::AlertWindow::backgroundColourId));
        g.fillRoundedRectangle(bounds, 10.0f);
        g.setColour(alert.findColour(juce::AlertWindow::outlineColourId));
        g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
        g.setColour(alert.findColour(juce::AlertWindow::textColourId));
        textLayout.draw(g, textArea.toFloat());
    }

    int getAlertWindowButtonHeight() override
    {
        return 24;
    }

    juce::Font getAlertWindowTitleFont() override
    {
        return AppFonts::getFont(22.0f);
    }

    juce::Font getAlertWindowMessageFont() override
    {
        return AppFonts::getFont(16.0f);
    }

    juce::Font getAlertWindowFont() override
    {
        return AppFonts::getFont(16.0f);
    }
};
