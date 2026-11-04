#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>

class VUMisterKnobLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

class NumericEntrySlider final : public juce::Slider
{
public:
    std::function<void()> onNumericEntryRequested;
    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (onNumericEntryRequested)
            onNumericEntryRequested();
        else
            juce::Slider::mouseDoubleClick (event);
    }
};

class VUMisterNAMAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit VUMisterNAMAudioProcessorEditor (VUMisterNAMAudioProcessor&);
    ~VUMisterNAMAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void updateModelStatus();
    void styleSlider (juce::Slider&, const juce::String&);

    VUMisterNAMAudioProcessor& audioProcessor;
    // Live VU values are shown in the faceplate; MAX is peak-hold for this editor session.
    float currentInputVuDb = 0.0f, currentOutputVuDb = 0.0f;
    float maxInputVuDb = 0.0f, maxOutputVuDb = 0.0f;
    bool inputMaxHasSignal = false, outputMaxHasSignal = false;
    juce::Image backgroundImage;
    NumericEntrySlider inputSlider, outputSlider, mixSlider;
    juce::Label inputValueEditor, mixValueEditor, outputValueEditor;
    VUMisterKnobLookAndFeel knobLookAndFeel;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> inputAttachment, mixAttachment, outputAttachment;

    // Invisible hit target: unlike a JUCE Button it cannot paint a focus/checkbox
    // frame over the artwork in hosts such as Nuendo 15.
    class BypassHitTarget final : public juce::Component
    {
    public:
        std::function<void()> onClick;
        void mouseUp (const juce::MouseEvent& event) override
        {
            if (event.mouseWasClicked() && onClick)
                onClick();
        }
        void paint (juce::Graphics&) override {}
    } bypassButton;
    juce::TextButton loadButton { "LOAD NAM" };
    juce::ComboBox modelSelector;
    juce::Label modelLabel, statusLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMisterNAMAudioProcessorEditor)
};
