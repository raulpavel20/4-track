#pragma once

#include <JuceHeader.h>

#include <functional>

class TransportControls : public juce::Component
{
public:
    TransportControls();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setPlaying(bool shouldBePlaying);

    std::function<void()> onRewind;
    std::function<void()> onPlayStop;

private:
    juce::TextButton rewindButton { "Rewind" };
    juce::TextButton playStopButton { "Play" };
};
