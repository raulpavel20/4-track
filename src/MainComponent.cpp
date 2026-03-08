#include "MainComponent.h"

#include "AppFonts.h"

#include <cmath>

MainComponent::HelpButton::HelpButton()
    : juce::Button({})
{
}

MainComponent::SettingsButton::SettingsButton()
    : juce::Button({})
{
}

void MainComponent::HelpButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    auto background = juce::Colours::black;

    if (isButtonDown)
        background = juce::Colours::white.withAlpha(0.18f);
    else if (isMouseOverButton)
        background = juce::Colours::white.withAlpha(0.08f);

    g.setColour(background);
    g.fillEllipse(bounds);
    g.setColour(juce::Colours::white.withAlpha(0.26f));
    g.drawEllipse(bounds, 1.0f);
    g.setColour(juce::Colours::white);
    g.setFont(AppFonts::getFont(14.0f));
    g.drawText("?", getLocalBounds(), juce::Justification::centred, false);
}

void MainComponent::SettingsButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    auto background = juce::Colours::black;

    if (isButtonDown)
        background = juce::Colours::white.withAlpha(0.18f);
    else if (isMouseOverButton)
        background = juce::Colours::white.withAlpha(0.08f);

    g.setColour(background);
    g.fillEllipse(bounds);
    g.setColour(juce::Colours::white.withAlpha(0.26f));
    g.drawEllipse(bounds, 1.0f);

    const auto centre = bounds.getCentre();
    const auto innerRadius = 4.0f;
    const auto outerRadius = 5.5f;

    juce::Path gear;
    for (int tooth = 0; tooth < 8; ++tooth)
    {
        const auto angle = juce::MathConstants<float>::twoPi * (float) tooth / 8.0f;
        const auto inner = juce::Point<float>(centre.x + std::cos(angle) * innerRadius,
                                              centre.y + std::sin(angle) * innerRadius);
        const auto outer = juce::Point<float>(centre.x + std::cos(angle) * outerRadius,
                                              centre.y + std::sin(angle) * outerRadius);
        gear.startNewSubPath(inner);
        gear.lineTo(outer);
    }

    g.setColour(juce::Colours::white);
    g.strokePath(gear, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawEllipse(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f, 1.4f);
}

MainComponent::SettingsWindow::SettingsWindow(MainComponent& ownerToUse, std::unique_ptr<juce::Component> content)
    : juce::DialogWindow("Settings", juce::Colours::black, true, false),
      owner(ownerToUse)
{
    setUsingNativeTitleBar(false);
    setResizable(false, false);
    setOpaque(true);
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colours::black);
    setContentOwned(content.release(), true);
    if (auto* component = getContentComponent())
        centreAroundComponent(&owner, component->getWidth(), component->getHeight());
    addToDesktop(juce::ComponentPeer::windowAppearsOnTaskbar | juce::ComponentPeer::windowHasDropShadow);
    enterModalState(true, nullptr, false);
    setVisible(true);
    toFront(true);
}

void MainComponent::SettingsWindow::closeButtonPressed()
{
    owner.closeSettingsWindow();
}

MainComponent::MainComponent()
    : tapeView(tapeEngine),
      trackControlChain(tapeEngine),
      trackMixerPanel(tapeEngine)
{
    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    addAndMakeVisible(tapeView);
    addAndMakeVisible(trackControlChain);
    addChildComponent(trackMixerPanel);
    addAndMakeVisible(preTapeTabButton);
    addAndMakeVisible(mixerTabButton);
    addAndMakeVisible(settingsButton);
    addAndMakeVisible(shortcutsHelpButton);

    preTapeTabButton.onClick = [this] { setBottomPanelMode(BottomPanelMode::preTape); };
    mixerTabButton.onClick = [this] { setBottomPanelMode(BottomPanelMode::mixer); };
    settingsButton.onClick = [this]
    {
        showSettingsWindow();
        grabKeyboardFocus();
    };
    shortcutsHelpButton.onClick = [this]
    {
        showShortcutsHelp();
        grabKeyboardFocus();
    };

    tapeView.onSelectedTrackChanged = [this](int trackIndex)
    {
        trackControlChain.setSelectedTrack(trackIndex);
        refreshInputOptions();
    };
    trackControlChain.setSelectedTrack(0);
    setBottomPanelMode(BottomPanelMode::preTape);

    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
    {
        auto safeThis = juce::Component::SafePointer<MainComponent>(this);
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
                                          [safeThis] (bool)
                                          {
                                              if (safeThis != nullptr)
                                                  safeThis->initialiseAudio();
                                          });
    }
    else
    {
        initialiseAudio();
    }

    addKeyListenerRecursive(tapeView);
    addKeyListenerRecursive(trackControlChain);
    addKeyListenerRecursive(trackMixerPanel);
    addKeyListenerRecursive(preTapeTabButton);
    addKeyListenerRecursive(mixerTabButton);
    addKeyListenerRecursive(settingsButton);
    addKeyListenerRecursive(shortcutsHelpButton);

    setSize(1100, 920);
}

MainComponent::~MainComponent()
{
    closeSettingsWindow();
    removeKeyListenerRecursive(tapeView);
    removeKeyListenerRecursive(trackControlChain);
    removeKeyListenerRecursive(trackMixerPanel);
    removeKeyListenerRecursive(preTapeTabButton);
    removeKeyListenerRecursive(mixerTabButton);
    removeKeyListenerRecursive(settingsButton);
    removeKeyListenerRecursive(shortcutsHelpButton);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(14);
    auto panelBounds = bounds.removeFromBottom(236);
    auto tabArea = bounds.removeFromBottom(26);
    const auto buttonHeight = 24;
    const auto preTapeWidth = 98;
    const auto mixerWidth = 82;
    const auto tabGap = 8;
    const auto totalWidth = preTapeWidth + mixerWidth + tabGap;
    const auto helpButtonSize = 24;
    const auto settingsButtonSize = 24;
    const auto utilityGap = 8;
    auto selectorBounds = juce::Rectangle<int>(tabArea.getCentreX() - (totalWidth / 2),
                                               tabArea.getY() + ((tabArea.getHeight() - buttonHeight) / 2),
                                               totalWidth,
                                               buttonHeight);
    preTapeTabButton.setBounds(selectorBounds.removeFromLeft(preTapeWidth));
    selectorBounds.removeFromLeft(tabGap);
    mixerTabButton.setBounds(selectorBounds.removeFromLeft(mixerWidth));
    shortcutsHelpButton.setBounds(bounds.getRight() - helpButtonSize,
                                  tabArea.getY() + ((tabArea.getHeight() - helpButtonSize) / 2),
                                  helpButtonSize,
                                  helpButtonSize);
    settingsButton.setBounds(shortcutsHelpButton.getX() - utilityGap - settingsButtonSize,
                             shortcutsHelpButton.getY(),
                             settingsButtonSize,
                             settingsButtonSize);
    trackControlChain.setBounds(panelBounds);
    trackMixerPanel.setBounds(panelBounds);
    tapeView.setBounds(bounds);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    return MainComponent::keyPressed(key, this);
}

void MainComponent::initialiseAudio()
{
    audioDeviceController.initialise(tapeEngine, [this] { refreshInputOptions(); });
}

void MainComponent::refreshInputOptions()
{
    trackControlChain.setInputOptions(tapeEngine.getAvailableInputSources(audioDeviceController.getHardwareInputNames(),
                                                                         trackControlChain.getSelectedTrack()));
}

void MainComponent::setBottomPanelMode(BottomPanelMode newMode)
{
    bottomPanelMode = newMode;
    trackControlChain.setVisible(bottomPanelMode == BottomPanelMode::preTape);
    trackMixerPanel.setVisible(bottomPanelMode == BottomPanelMode::mixer);
    updateTabButtonStyles();
}

void MainComponent::updateTabButtonStyles()
{
    auto applyStyle = [] (juce::TextButton& button, bool isActive, juce::Button::ConnectedEdgeFlags edges)
    {
        button.setColour(juce::TextButton::buttonColourId, isActive ? juce::Colours::white : juce::Colours::black);
        button.setColour(juce::TextButton::buttonOnColourId, isActive ? juce::Colours::white : juce::Colours::black);
        button.setColour(juce::TextButton::textColourOffId, isActive ? juce::Colours::black : juce::Colours::white);
        button.setColour(juce::TextButton::textColourOnId, isActive ? juce::Colours::black : juce::Colours::white);
        button.setConnectedEdges(edges);
    };

    applyStyle(preTapeTabButton, bottomPanelMode == BottomPanelMode::preTape, juce::Button::ConnectedOnRight);
    applyStyle(mixerTabButton, bottomPanelMode == BottomPanelMode::mixer, juce::Button::ConnectedOnLeft);
    repaint();
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);

    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    const auto modifiers = key.getModifiers();
    const auto keyCode = key.getKeyCode();

    if (keyCode == juce::KeyPress::spaceKey)
    {
        togglePlayStop();
        return true;
    }

    if (keyCode == juce::KeyPress::leftKey)
    {
        if (modifiers.isShiftDown())
            jumpToAdjacentLoopMarker(false);
        else
            jumpToAdjacentBeat(false);

        return true;
    }

    if (keyCode == juce::KeyPress::rightKey)
    {
        if (modifiers.isShiftDown())
            jumpToAdjacentLoopMarker(true);
        else
            jumpToAdjacentBeat(true);

        return true;
    }

    if (keyCode == juce::KeyPress::upKey)
    {
        selectAdjacentTrack(-1);
        return true;
    }

    if (keyCode == juce::KeyPress::downKey)
    {
        selectAdjacentTrack(1);
        return true;
    }

    const auto character = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());

    if (character == 's')
    {
        toggleCurrentTrackSolo();
        return true;
    }

    if (character == 'm')
    {
        toggleCurrentTrackMute();
        return true;
    }

    if (character == 't' && ! modifiers.isShiftDown())
    {
        tapeEngine.setMetronomeEnabled(! tapeEngine.isMetronomeEnabled());
        repaint();
        return true;
    }

    if (character == 'l' && ! modifiers.isShiftDown())
    {
        tapeEngine.toggleLoopMarkerAtNearestBeat();
        repaint();
        return true;
    }

    if (character == 'r')
    {
        toggleCurrentTrackRecordMode(modifiers.isShiftDown() ? TrackRecordMode::replace
                                                             : TrackRecordMode::overdub);
        return true;
    }

    if (character == 'e' && ! modifiers.isShiftDown())
    {
        toggleCurrentTrackRecordMode(TrackRecordMode::erase);
        return true;
    }

    return false;
}

void MainComponent::addKeyListenerRecursive(juce::Component& component)
{
    component.addKeyListener(this);

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        addKeyListenerRecursive(*component.getChildComponent(childIndex));
}

void MainComponent::removeKeyListenerRecursive(juce::Component& component)
{
    component.removeKeyListener(this);

    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
        removeKeyListenerRecursive(*component.getChildComponent(childIndex));
}

void MainComponent::togglePlayStop()
{
    if (tapeEngine.isPlaying() || tapeEngine.isCountInActive())
        tapeEngine.stop();
    else
        tapeEngine.play();

    repaint();
}

void MainComponent::jumpToAdjacentBeat(bool forward)
{
    const auto sampleRate = juce::jmax(1.0, tapeEngine.getSampleRate());
    const auto samplesPerBeat = (sampleRate * 60.0) / juce::jmax(30.0, (double) tapeEngine.getBpm());
    const auto playhead = tapeEngine.getPlayheadSample();
    auto targetSample = 0.0;

    if (forward)
        targetSample = (std::floor(playhead / samplesPerBeat) + 1.0) * samplesPerBeat;
    else
        targetSample = juce::jmax(0.0, std::floor((playhead - 1.0) / samplesPerBeat) * samplesPerBeat);

    tapeEngine.setPlayheadSample(targetSample);
    repaint();
}

void MainComponent::jumpToAdjacentLoopMarker(bool forward)
{
    auto targetSample = 0.0;

    if (! tapeEngine.getAdjacentLoopMarkerSample(forward, tapeEngine.getPlayheadSample(), targetSample))
    {
        if (! forward)
            targetSample = 0.0;
        else
            return;
    }

    tapeEngine.setPlayheadSample(targetSample);
    repaint();
}

void MainComponent::selectAdjacentTrack(int delta)
{
    tapeView.setSelectedTrack(tapeView.getSelectedTrack() + delta);
    repaint();
}

void MainComponent::toggleCurrentTrackSolo()
{
    const auto trackIndex = tapeView.getSelectedTrack();
    tapeEngine.setTrackSolo(trackIndex, ! tapeEngine.isTrackSolo(trackIndex));
    repaint();
}

void MainComponent::toggleCurrentTrackMute()
{
    const auto trackIndex = tapeView.getSelectedTrack();
    tapeEngine.setTrackMuted(trackIndex, ! tapeEngine.isTrackMuted(trackIndex));
    repaint();
}

void MainComponent::toggleCurrentTrackRecordMode(TrackRecordMode mode)
{
    const auto trackIndex = tapeView.getSelectedTrack();
    const auto currentMode = tapeEngine.getTrackRecordMode(trackIndex);
    const auto pendingMode = tapeEngine.getPendingTrackRecordMode(trackIndex);
    const auto nextMode = pendingMode == mode ? TrackRecordMode::off
                                              : pendingMode != TrackRecordMode::off ? mode
                                                                                    : currentMode == mode ? TrackRecordMode::off
                                                                                                          : mode;
    tapeEngine.requestTrackRecordMode(trackIndex, nextMode);
    repaint();
}

void MainComponent::showSettingsWindow()
{
    if (settingsWindow != nullptr)
    {
        settingsWindow->toFront(true);
        return;
    }

    settingsWindow = std::make_unique<SettingsWindow>(*this,
                                                      std::make_unique<AppSettingsComponent>(audioDeviceController.getDeviceManager(),
                                                                                            tapeEngine));
}

void MainComponent::closeSettingsWindow()
{
    settingsWindow.reset();
}

void MainComponent::showShortcutsHelp()
{
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                           "Keyboard Shortcuts",
                                           "Space: Play / Stop\n"
                                           "Left / Right: Previous / Next beat\n"
                                           "Shift + Left / Right: Previous / Next loop marker\n"
                                           "T: Toggle metronome\n"
                                           "L: Add / Remove loop marker\n"
                                           "Up / Down: Select track above / below\n"
                                           "S: Solo selected track\n"
                                           "M: Mute selected track\n"
                                           "R: Arm selected track for overdub\n"
                                           "Shift + R: Arm selected track for replace\n"
                                           "E: Arm selected track for erase");
}
