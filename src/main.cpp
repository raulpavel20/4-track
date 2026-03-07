#include <JuceHeader.h>

#include "AppLookAndFeel.h"
#include "MainComponent.h"

class FourTrackTapeApplication : public juce::JUCEApplication
{
public:
    FourTrackTapeApplication() = default;

    const juce::String getApplicationName() override
    {
        return "4-Track Tape";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }

    bool moreThanOneInstanceAllowed() override
    {
        return true;
    }

    void initialise(const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override
    {
    }

private:
    AppLookAndFeel lookAndFeel;

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : juce::DocumentWindow(name,
                                   juce::Desktop::getInstance().getDefaultLookAndFeel()
                                       .findColour(juce::ResizableWindow::backgroundColourId),
                                   juce::DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(FourTrackTapeApplication)
