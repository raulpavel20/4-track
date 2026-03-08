#include "UserSettingsSection.h"

#include "../../../AppFonts.h"
#include "../../../AppSettings.h"

UserSettingsSection::ColourButton::ColourButton()
    : juce::Button({})
{
}

void UserSettingsSection::ColourButton::setButtonColour(juce::Colour newColour)
{
    colour = newColour;
    repaint();
}

juce::Colour UserSettingsSection::ColourButton::getButtonColour() const noexcept
{
    return colour;
}

void UserSettingsSection::ColourButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
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

UserSettingsSection::ToggleButton::ToggleButton(const juce::String& textToUse)
    : juce::Button({}),
      text(textToUse)
{
    setClickingTogglesState(true);
}

void UserSettingsSection::ToggleButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
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

UserSettingsSection::UserSettingsSection(TapeEngine& engineToUse)
    : engine(engineToUse)
{
    AppSettings::getInstance().addChangeListener(this);

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

    refreshFromSettings();
}

UserSettingsSection::~UserSettingsSection()
{
    AppSettings::getInstance().removeChangeListener(this);
}

void UserSettingsSection::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 14.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(17.0f));
    g.drawText("User", juce::Rectangle<int>(20, 12, getWidth() - 40, 20), juce::Justification::centredLeft, false);

    g.setFont(AppFonts::getFont(13.0f));
    g.setColour(juce::Colours::white.withAlpha(0.62f));
    const auto leftPadding = 24;
    const auto labelX = leftPadding;
    const auto labelWidth = 128;
    const auto controlsX = 176;
    const auto rightColumnWidth = 204;
    const auto rightColumnX = getWidth() - leftPadding - rightColumnWidth;
    g.drawText("Track Colors", juce::Rectangle<int>(labelX, 58, labelWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Default BPM", juce::Rectangle<int>(labelX, 140, labelWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Beats Per Bar", juce::Rectangle<int>(labelX, 200, labelWidth, 16), juce::Justification::centredLeft, false);
    g.drawText("Startup Defaults", juce::Rectangle<int>(rightColumnX, 58, rightColumnWidth, 16), juce::Justification::centredLeft, false);

    const auto trackLabelWidth = 56;
    const auto gap = 14;
    const auto labelY = 58;

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        g.drawText("Track " + juce::String(trackIndex + 1),
                   controlsX + (trackIndex * (trackLabelWidth + gap)),
                   labelY,
                   trackLabelWidth,
                   12,
                   juce::Justification::centred,
                   false);
    }
}

void UserSettingsSection::resized()
{
    const auto leftPadding = 24;
    const auto controlsX = 176;
    const auto colourButtonWidth = 56;
    const auto colourGap = 14;
    const auto buttonY = 80;
    const auto rightColumnWidth = 204;
    const auto rightColumnX = getWidth() - leftPadding - rightColumnWidth;

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
    {
        trackColourButtons[(size_t) trackIndex].setBounds(controlsX + (trackIndex * (colourButtonWidth + colourGap)),
                                                          buttonY,
                                                          colourButtonWidth,
                                                          30);
    }

    bpmEditor.setBounds(controlsX, 134, 92, 34);
    beatsPerBarEditor.setBounds(controlsX, 194, 92, 34);
    metronomeDefaultButton.setBounds(rightColumnX, 104, rightColumnWidth, 38);
    countInDefaultButton.setBounds(rightColumnX, 154, rightColumnWidth, 38);
}

void UserSettingsSection::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &AppSettings::getInstance())
        refreshFromSettings();
}

void UserSettingsSection::configureLabelEditor(juce::Label& editor, int maxCharacters)
{
    editor.setEditable(true, true, false);
    editor.setJustificationType(juce::Justification::centred);
    editor.setFont(AppFonts::getFont(15.0f));
    editor.setColour(juce::Label::textColourId, juce::Colours::white);
    editor.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    editor.setColour(juce::Label::outlineColourId, juce::Colours::white.withAlpha(0.18f));
    editor.setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
    editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha(0.22f));
    editor.onEditorShow = [&editor, maxCharacters]
    {
        if (auto* textEditor = editor.getCurrentTextEditor())
            textEditor->setInputRestrictions(maxCharacters, "0123456789");
    };
}

void UserSettingsSection::configureToggleButton(ToggleButton& button)
{
    button.setTriggeredOnMouseDown(false);
}

void UserSettingsSection::refreshFromSettings()
{
    auto& settings = AppSettings::getInstance();

    for (int trackIndex = 0; trackIndex < TapeEngine::numTracks; ++trackIndex)
        trackColourButtons[(size_t) trackIndex].setButtonColour(settings.getTrackColour(trackIndex));

    bpmEditor.setText(juce::String(settings.getDefaultBpm()), juce::dontSendNotification);
    beatsPerBarEditor.setText(juce::String(settings.getDefaultBeatsPerBar()), juce::dontSendNotification);
    metronomeDefaultButton.setToggleState(settings.getDefaultMetronomeEnabled(), juce::dontSendNotification);
    countInDefaultButton.setToggleState(settings.getDefaultCountInEnabled(), juce::dontSendNotification);
    repaint();
}

void UserSettingsSection::openColourSelector(int trackIndex)
{
    auto selector = std::make_unique<juce::ColourSelector>();
    selector->setName("Track Colour");
    selector->setCurrentColour(AppSettings::getInstance().getTrackColour(trackIndex));
    selector->setColour(juce::ColourSelector::backgroundColourId, juce::Colours::black);
    selector->setSize(300, 400);

    auto* selectorPtr = selector.get();
    activeColourTrackIndex = trackIndex;
    activeColourSelector = selectorPtr;
    selectorPtr->addChangeListener(this);

    juce::CallOutBox::launchAsynchronously(std::move(selector),
                                           trackColourButtons[(size_t) trackIndex].getScreenBounds(),
                                           nullptr);
}
