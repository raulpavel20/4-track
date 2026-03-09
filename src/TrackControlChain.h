#pragma once

#include <JuceHeader.h>

#include <array>

#include "TapeEngine.h"

class TrackControlChain : public juce::Component,
                          private juce::Timer,
                          private juce::ChangeListener
{
public:
    explicit TrackControlChain(TapeEngine& engineToUse);
    ~TrackControlChain() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelectedTrack(int trackIndex);
    int getSelectedTrack() const noexcept;
    void setSendBusIndex(int sendIndex);
    int getSendBusIndex() const noexcept;
    void setInputOptions(const juce::Array<TapeEngine::InputSourceOption>& options);

private:
    static constexpr int compressorControlCount = 5;
    static constexpr int noiseGateControlCount = 4;
    static constexpr int limiterControlCount = 2;
    static constexpr int delayControlCount = 3;
    static constexpr int reverbControlCount = 3;
    static constexpr int chorusControlCount = 5;
    static constexpr int phaserControlCount = 5;
    static constexpr int utilityControlCount = 2;

    enum class TargetType
    {
        track = 0,
        sendBus
    };

    class CloseButton : public juce::Button
    {
    public:
        CloseButton();
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;
    };

    class BypassButton : public juce::Button
    {
    public:
        BypassButton();
        void setAccentColour(juce::Colour newAccentColour);
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    private:
        juce::Colour accentColour { juce::Colours::white };
    };

    class SaturationModeButton : public juce::Button
    {
    public:
        SaturationModeButton();
        void setAccentColour(juce::Colour newAccentColour);
        void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

    private:
        juce::Colour accentColour { juce::Colours::white };
    };

    class ContentComponent : public juce::Component
    {
    public:
        explicit ContentComponent(TrackControlChain& ownerToUse);
        void paint(juce::Graphics& g) override;

    private:
        TrackControlChain& owner;
    };

    class InputSourceComboBox : public juce::ComboBox
    {
    public:
        explicit InputSourceComboBox(TrackControlChain& ownerToUse);
        void mouseDown(const juce::MouseEvent& event) override;

    private:
        TrackControlChain& owner;
    };

    TapeEngine& engine;
    TargetType targetType = TargetType::track;
    int selectedTrack = 0;
    int selectedSendBus = 0;
    juce::Array<TapeEngine::InputSourceOption> inputOptions;
    juce::Viewport modulesViewport;
    ContentComponent contentComponent;
    InputSourceComboBox inputSourceBox;
    juce::Slider inputGainSlider;
    juce::TextButton addModuleButton { "+" };
    std::array<BypassButton, Track::maxChainModules> bypassButtons;
    std::array<CloseButton, Track::maxChainModules> removeButtons;
    std::array<juce::Slider, Track::maxChainModules> filterSliders;
    std::array<std::array<juce::Slider, Track::maxEqBands>, Track::maxChainModules> eqGainSliders;
    std::array<std::array<juce::Slider, Track::maxEqBands>, Track::maxChainModules> eqQSliders;
    std::array<std::array<juce::TextEditor, Track::maxEqBands>, Track::maxChainModules> eqFrequencyEditors;
    std::array<std::array<juce::Slider, compressorControlCount>, Track::maxChainModules> compressorSliders;
    std::array<std::array<juce::Slider, noiseGateControlCount>, Track::maxChainModules> noiseGateSliders;
    std::array<std::array<juce::Slider, limiterControlCount>, Track::maxChainModules> limiterSliders;
    std::array<SaturationModeButton, Track::maxChainModules> saturationModeButtons;
    std::array<juce::Slider, Track::maxChainModules> saturationAmountSliders;
    std::array<std::array<juce::Slider, delayControlCount>, Track::maxChainModules> delaySliders;
    std::array<juce::TextButton, Track::maxChainModules> delayTimeModeButtons;
    std::array<std::array<juce::Slider, reverbControlCount>, Track::maxChainModules> reverbSliders;
    std::array<std::array<juce::Slider, chorusControlCount>, Track::maxChainModules> chorusSliders;
    std::array<juce::TextButton, Track::maxChainModules> chorusRateModeButtons;
    std::array<std::array<juce::Slider, phaserControlCount>, Track::maxChainModules> phaserSliders;
    std::array<juce::TextButton, Track::maxChainModules> phaserRateModeButtons;
    std::array<std::array<juce::Slider, utilityControlCount>, Track::maxChainModules> gainModuleSliders;
    std::array<int, Track::maxChainModules> visibleSlots {};
    std::array<ChainModuleType, Track::maxChainModules> visibleTypes {};
    std::array<juce::Rectangle<int>, Track::maxChainModules> moduleBounds {};
    std::array<float, Track::maxChainModules> chorusVisualPhases {};
    std::array<float, Track::maxChainModules> chorusVisualRates {};
    std::array<float, Track::maxChainModules> phaserVisualPhases {};
    std::array<float, Track::maxChainModules> phaserVisualRates {};
    int visibleModuleCount = 0;
    double lastAnimationTimestampMs = 0.0;

    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void paintContent(juce::Graphics& g);
    void refreshFromEngine();
    bool showsInputModule() const noexcept;
    bool isTrackTarget() const noexcept;
    juce::Colour getAccentColour() const;
    int addModule(ChainModuleType type);
    void removeModule(int moduleIndex);
    int getModuleCount() const noexcept;
    ChainModuleType getModuleType(int moduleIndex) const noexcept;
    bool isModuleBypassed(int moduleIndex) const noexcept;
    void setModuleBypassed(int moduleIndex, bool shouldBeBypassed);
    float getFilterMorph(int moduleIndex) const noexcept;
    void setFilterMorph(int moduleIndex, float value);
    float getEqBandGainDb(int moduleIndex, int bandIndex) const noexcept;
    void setEqBandGainDb(int moduleIndex, int bandIndex, float value);
    float getEqBandQ(int moduleIndex, int bandIndex) const noexcept;
    void setEqBandQ(int moduleIndex, int bandIndex, float value);
    float getEqBandFrequency(int moduleIndex, int bandIndex) const noexcept;
    void setEqBandFrequency(int moduleIndex, int bandIndex, float value);
    float getCompressorThresholdDb(int moduleIndex) const noexcept;
    void setCompressorThresholdDb(int moduleIndex, float value);
    float getCompressorRatio(int moduleIndex) const noexcept;
    void setCompressorRatio(int moduleIndex, float value);
    float getCompressorAttackMs(int moduleIndex) const noexcept;
    void setCompressorAttackMs(int moduleIndex, float value);
    float getCompressorReleaseMs(int moduleIndex) const noexcept;
    void setCompressorReleaseMs(int moduleIndex, float value);
    float getCompressorMakeupGainDb(int moduleIndex) const noexcept;
    void setCompressorMakeupGainDb(int moduleIndex, float value);
    float getNoiseGateThresholdDb(int moduleIndex) const noexcept;
    void setNoiseGateThresholdDb(int moduleIndex, float value);
    float getNoiseGateRatio(int moduleIndex) const noexcept;
    void setNoiseGateRatio(int moduleIndex, float value);
    float getNoiseGateAttackMs(int moduleIndex) const noexcept;
    void setNoiseGateAttackMs(int moduleIndex, float value);
    float getNoiseGateReleaseMs(int moduleIndex) const noexcept;
    void setNoiseGateReleaseMs(int moduleIndex, float value);
    float getLimiterThresholdDb(int moduleIndex) const noexcept;
    void setLimiterThresholdDb(int moduleIndex, float value);
    float getLimiterReleaseMs(int moduleIndex) const noexcept;
    void setLimiterReleaseMs(int moduleIndex, float value);
    SaturationMode getSaturationMode(int moduleIndex) const noexcept;
    void setSaturationMode(int moduleIndex, SaturationMode mode);
    float getSaturationAmount(int moduleIndex) const noexcept;
    void setSaturationAmount(int moduleIndex, float value);
    float getDelayTimeMs(int moduleIndex) const noexcept;
    void setDelayTimeMs(int moduleIndex, float value);
    bool isDelaySyncEnabled(int moduleIndex) const noexcept;
    void setDelaySyncEnabled(int moduleIndex, bool shouldBeEnabled);
    int getDelaySyncIndex(int moduleIndex) const noexcept;
    void setDelaySyncIndex(int moduleIndex, int index);
    float getResolvedDelayTimeMs(int moduleIndex) const noexcept;
    float getDelayFeedback(int moduleIndex) const noexcept;
    void setDelayFeedback(int moduleIndex, float value);
    float getDelayMix(int moduleIndex) const noexcept;
    void setDelayMix(int moduleIndex, float value);
    float getReverbSize(int moduleIndex) const noexcept;
    void setReverbSize(int moduleIndex, float value);
    float getReverbDamping(int moduleIndex) const noexcept;
    void setReverbDamping(int moduleIndex, float value);
    float getReverbMix(int moduleIndex) const noexcept;
    void setReverbMix(int moduleIndex, float value);
    float getChorusRate(int moduleIndex) const noexcept;
    void setChorusRate(int moduleIndex, float value);
    bool isChorusSyncEnabled(int moduleIndex) const noexcept;
    void setChorusSyncEnabled(int moduleIndex, bool shouldBeEnabled);
    int getChorusSyncIndex(int moduleIndex) const noexcept;
    void setChorusSyncIndex(int moduleIndex, int index);
    float getResolvedChorusRateHz(int moduleIndex) const noexcept;
    float getChorusDepth(int moduleIndex) const noexcept;
    void setChorusDepth(int moduleIndex, float value);
    float getChorusCentreFrequency(int moduleIndex) const noexcept;
    void setChorusCentreFrequency(int moduleIndex, float value);
    float getChorusFeedback(int moduleIndex) const noexcept;
    void setChorusFeedback(int moduleIndex, float value);
    float getChorusMix(int moduleIndex) const noexcept;
    void setChorusMix(int moduleIndex, float value);
    float getPhaserRate(int moduleIndex) const noexcept;
    void setPhaserRate(int moduleIndex, float value);
    bool isPhaserSyncEnabled(int moduleIndex) const noexcept;
    void setPhaserSyncEnabled(int moduleIndex, bool shouldBeEnabled);
    int getPhaserSyncIndex(int moduleIndex) const noexcept;
    void setPhaserSyncIndex(int moduleIndex, int index);
    float getResolvedPhaserRateHz(int moduleIndex) const noexcept;
    float getPhaserDepth(int moduleIndex) const noexcept;
    void setPhaserDepth(int moduleIndex, float value);
    float getPhaserCentreFrequency(int moduleIndex) const noexcept;
    void setPhaserCentreFrequency(int moduleIndex, float value);
    float getPhaserFeedback(int moduleIndex) const noexcept;
    void setPhaserFeedback(int moduleIndex, float value);
    float getPhaserMix(int moduleIndex) const noexcept;
    void setPhaserMix(int moduleIndex, float value);
    float getGainModuleGainDb(int moduleIndex) const noexcept;
    void setGainModuleGainDb(int moduleIndex, float value);
    float getGainModulePan(int moduleIndex) const noexcept;
    void setGainModulePan(int moduleIndex, float value);
    float getModuleInputMeter(int moduleIndex) const noexcept;
    float getModuleOutputMeter(int moduleIndex) const noexcept;
    std::array<float, ModuleChain::spectrumAnalyzerBinCount> getSpectrumAnalyzerData(int moduleIndex) const noexcept;
    void updateAccentColours();
    void updateVisibleModules();
    void updateModuleVisibility();
    void layoutContent();
    void layoutInputModule(juce::Rectangle<int> inputBounds);
    void layoutFilterModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutEqModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutCompressorModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutNoiseGateModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutLimiterModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutSaturationModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutDelayModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutReverbModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutChorusModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutPhaserModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutSpectrumAnalyzerModule(int slot, juce::Rectangle<int> moduleArea);
    void layoutGainModule(int slot, juce::Rectangle<int> moduleArea);
    void paintInputModule(juce::Graphics& g, juce::Colour accent, juce::Rectangle<int> inputBounds);
    void paintFilterModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintEqModule(juce::Graphics& g, int slot);
    void paintCompressorModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintNoiseGateModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintLimiterModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintSaturationModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintDelayModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintReverbModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintChorusModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintPhaserModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintSpectrumAnalyzerModule(juce::Graphics& g, juce::Colour accent, int slot, juce::Rectangle<int> bounds);
    void paintGainModule(juce::Graphics& g, int slot, juce::Rectangle<int> bounds);
    void syncFrequencyEditorsFromEngine(bool force);
    void showInputSourceMenu();
    juce::Rectangle<int> getFrameBounds() const;
    juce::Rectangle<int> getViewportBounds() const;
    juce::Rectangle<int> getInputModuleBounds() const;
    juce::Rectangle<int> getAddButtonBounds() const;
};
