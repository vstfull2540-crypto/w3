#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>
#include <algorithm>
#include <iterator>

namespace
{
    const juce::Colour amber (0xffffa51f);
    const juce::Colour cream (0xffffe9b0);
    const juce::Colour panel (0xff151718);

    void drawMetalKnobWell (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Mask the baked-in knob and its surrounding scale marks so only the
        // interactive, unmarked live control remains visible.
        r = r.expanded (7.0f);
        g.setColour (juce::Colour (0xff030405));
        g.fillEllipse (r.expanded (2.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff25292a), r.getX(), r.getY(),
                                                 juce::Colour (0xff080a0b), r.getRight(), r.getBottom(), false));
        g.fillEllipse (r);
        g.setColour (juce::Colour (0xff65696a).withAlpha (0.45f));
        g.drawEllipse (r.reduced (1.2f), 0.9f);
    }

    // Pixel-calibrated needle endpoints aligned to the exact supplied faceplate artwork.
    // The artwork uses a flattened, non-circular scale, so interpolating the printed
    // tick positions is more accurate than drawing a generic circular VU face over it.
    struct VuPoint { float db; float x; float y; };
    constexpr VuPoint vuTipPoints[] = {
        { -20.0f, -190.0f, -72.0f },
        { -10.0f, -131.0f, -113.0f },
        {  -7.0f,  -99.0f, -126.0f },
        {  -5.0f,  -62.0f, -133.0f },
        {  -3.0f,  -11.0f, -122.0f },
        {  -1.0f,   43.0f, -122.0f },
        {   0.0f,   82.0f, -122.0f },
        {   1.0f,  116.0f, -120.0f },
        {   2.0f,  156.0f, -101.0f },
        {   3.0f,  195.0f,  -78.0f }
    };

    juce::Point<float> vuNeedleTipOffset (float db)
    {
        db = juce::jlimit (-20.0f, 3.0f, db);
        for (size_t i = 0; i + 1 < std::size (vuTipPoints); ++i)
        {
            const auto& a = vuTipPoints[i];
            const auto& b = vuTipPoints[i + 1];
            if (db <= b.db)
            {
                const float t = (db - a.db) / (b.db - a.db);
                return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
            }
        }
        const auto& last = vuTipPoints[std::size (vuTipPoints) - 1];
        return { last.x, last.y };
    }

}

void VUMisterKnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                   float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const float size = juce::jmin (static_cast<float> (width), static_cast<float> (height));
    const float radius = size * 0.30f;
    const juce::Point<float> centre (x + width * 0.5f, y + height * 0.5f);
    const auto outer = juce::Rectangle<float> (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    // Vintage broadcast dial: fixed metal bezel, fine ivory tick marks, and a tapered orange pointer.
    // The outer -24 dB / +24 dB and DRY / WET labels remain part of the reference artwork. No animated progress arc.
    g.setColour (juce::Colour (0x99000000));
    g.fillEllipse (outer.expanded (5.0f, 6.0f));

    const auto bezel = outer.expanded (3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2e7cc), bezel.getX(), bezel.getY(),
                                             juce::Colour (0xff5b554a), bezel.getRight(), bezel.getBottom(), false));
    g.fillEllipse (bezel);
    g.setColour (juce::Colour (0xff181818));
    g.drawEllipse (outer.expanded (0.5f), 1.1f);

    const auto face = outer.reduced (5.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343737), face.getX(), face.getY(),
                                             juce::Colour (0xff080909), face.getRight(), face.getBottom(), false));
    g.fillEllipse (face);
    g.setColour (juce::Colour (0xffb98243).withAlpha (0.95f));
    g.drawEllipse (face.reduced (1.0f), 1.0f);
    g.setColour (juce::Colour (0xff7a7b77).withAlpha (0.7f));
    g.drawEllipse (face.reduced (4.0f), 0.7f);

    const bool isMixControl = slider.getName() == "MIX";
    const int tickCount = 40;
    const int majorTickInterval = isMixControl ? 4 : 5;

    // Fine ivory tick marks with longer, brighter major divisions.
    for (int i = 0; i <= tickCount; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (tickCount);
        const float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const bool majorTick = (i % majorTickInterval == 0);
        const float r1 = radius * 1.18f;
        const float r2 = radius * (majorTick ? 1.36f : 1.28f);
        const juce::Point<float> p1 (centre.x + std::cos (angle) * r1, centre.y + std::sin (angle) * r1);
        const juce::Point<float> p2 (centre.x + std::cos (angle) * r2, centre.y + std::sin (angle) * r2);
        g.setColour (majorTick ? juce::Colour (0xffffe8bd) : juce::Colour (0xffc7c0ad).withAlpha (0.8f));
        g.drawLine (p1.x, p1.y, p2.x, p2.y, majorTick ? 1.8f : 0.85f);
    }

    // Numeric scales are drawn outside the tick ring. The reference artwork's
    // original endpoint labels were removed from the background image so these
    // labels can be positioned consistently at every supported editor size.
    const auto drawScaleLabel = [&g, &centre, &rotaryStartAngle, &rotaryEndAngle] (const juce::String& text, float t, float distance, float labelWidth, float verticalOffset)
    {
        const float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float labelX = centre.x + std::cos (angle) * distance;
        const float labelY = centre.y + std::sin (angle) * distance;
        g.setColour (juce::Colour (0xffffe6b7));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (text, juce::Rectangle<int> (juce::roundToInt (labelX - labelWidth * 0.5f),
                                                juce::roundToInt (labelY + verticalOffset - 7.0f),
                                                juce::roundToInt (labelWidth), 14),
                    juce::Justification::centred, false);
    };

    if (isMixControl)
    {
        const juce::String mixLabels[] { "0%", "25%", "50%", "75%", "100%" };
        for (int i = 0; i < 5; ++i)
            drawScaleLabel (mixLabels[i], static_cast<float> (i) / 4.0f, radius * 1.60f, 31.0f, i == 2 ? 4.0f : 0.0f);

        // Keep DRY/WET as clear endpoint legends, shifted outward from the dial.
        g.setColour (juce::Colour (0xffffe6b7));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("DRY", juce::Rectangle<int> (juce::roundToInt (centre.x - radius * 1.78f - 20.0f),
                                                  juce::roundToInt (centre.y + radius * 0.91f + 4.0f), 40, 14),
                    juce::Justification::centred, false);
        g.drawText ("WET", juce::Rectangle<int> (juce::roundToInt (centre.x + radius * 1.42f),
                                                  juce::roundToInt (centre.y + radius * 0.91f + 4.0f), 40, 14),
                    juce::Justification::centred, false);
    }
    else
    {
        const juce::String dbLabels[] { "-24 dB", "-18", "-12", "-6", "0", "+6", "+12", "+18", "+24 dB" };
        for (int i = 0; i < 9; ++i)
        {
            const bool endpoint = (i == 0 || i == 8);
            drawScaleLabel (dbLabels[i], static_cast<float> (i) / 8.0f,
                            radius * (endpoint ? 1.86f : 1.58f), endpoint ? 48.0f : 28.0f, 0.0f);
        }
    }

    const float valueAngle = rotaryStartAngle + juce::jlimit (0.0f, 1.0f, sliderPosProportional) * (rotaryEndAngle - rotaryStartAngle);

    // Short tapered orange pointer, matching the hardware pointer length in the reference image.
    const float pointerLength = radius * 0.82f;
    const juce::Point<float> direction (std::cos (valueAngle), std::sin (valueAngle));
    const juce::Point<float> normal (-direction.y, direction.x);
    const juce::Point<float> tip (centre.x + direction.x * pointerLength,
                                  centre.y + direction.y * pointerLength);
    const float baseHalfWidth = 2.0f;
    const juce::Point<float> leftBase (centre.x + normal.x * baseHalfWidth,
                                       centre.y + normal.y * baseHalfWidth);
    const juce::Point<float> rightBase (centre.x - normal.x * baseHalfWidth,
                                        centre.y - normal.y * baseHalfWidth);
    juce::Path pointerShadow;
    pointerShadow.startNewSubPath (leftBase.x + 1.0f, leftBase.y + 1.3f);
    pointerShadow.lineTo (tip.x + 1.0f, tip.y + 1.3f);
    pointerShadow.lineTo (rightBase.x + 1.0f, rightBase.y + 1.3f);
    pointerShadow.closeSubPath();
    g.setColour (juce::Colour (0xaa000000));
    g.fillPath (pointerShadow);

    juce::Path pointer;
    pointer.startNewSubPath (leftBase.x, leftBase.y);
    pointer.lineTo (tip.x, tip.y);
    pointer.lineTo (rightBase.x, rightBase.y);
    pointer.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb45e), centre.x, centre.y,
                                             juce::Colour (0xffe66c22), tip.x, tip.y, false));
    g.fillPath (pointer);
    g.setColour (juce::Colour (0xffffe3a6).withAlpha (0.95f));
    g.drawLine (centre.x, centre.y, tip.x, tip.y, 0.75f);

    const float hub = radius * 0.13f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe7d8b6), centre.x - hub, centre.y - hub,
                                             juce::Colour (0xff6e6759), centre.x + hub, centre.y + hub, false));
    g.fillEllipse (centre.x - hub, centre.y - hub, hub * 2.0f, hub * 2.0f);
    g.setColour (juce::Colour (0xff1b1b19));
    g.fillEllipse (centre.x - hub * 0.55f, centre.y - hub * 0.55f, hub * 1.1f, hub * 1.1f);
}

VUMisterNAMAudioProcessorEditor::VUMisterNAMAudioProcessorEditor (VUMisterNAMAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    backgroundImage = juce::ImageCache::getFromMemory (BinaryData::reference_full_interface_png,
                                                        BinaryData::reference_full_interface_pngSize);
    setSize (761, 416);
    setResizable (false, false);
    setResizeLimits (761, 416, 761, 416);
    getConstrainer()->setFixedAspectRatio (1522.0 / 833.0);

    auto setupSlider = [this] (juce::Slider& s, const juce::String& name)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 0.75f,
                               juce::MathConstants<float>::pi * 2.25f, true);
        s.setLookAndFeel (&knobLookAndFeel);
        s.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        s.setDoubleClickReturnValue (true, name == "INPUT" ? 0.0 : (name == "MIX" ? 50.0 : 0.0));
        // JUCE Slider has no setRotaryDragSensitivity() API. Keep the native
        // RotaryHorizontalVerticalDrag interaction for JUCE 8 compatibility.
        // The faceplate already contains the INPUT / MIX / OUTPUT legends.
        // A JUCE text box here overlaps those legends at common plugin sizes.
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::rotarySliderFillColourId, amber);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff646767));
        s.setColour (juce::Slider::thumbColourId, cream);
        s.setColour (juce::Slider::textBoxTextColourId, cream);
        s.setColour (juce::Slider::textBoxBackgroundColourId, panel);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff56595a));
        addAndMakeVisible (s);
    };
    setupSlider (inputSlider, "INPUT");
    setupSlider (mixSlider, "MIX");
    setupSlider (outputSlider, "OUTPUT");

    // Numeric values are hidden during normal use. Double-clicking a knob opens a
    // temporary entry field over its centre; committing the text updates the same
    // APVTS-attached slider. The field disappears immediately after editing.
    auto setupNumericEditor = [this] (juce::Label& label, NumericEntrySlider& slider,
                                      const juce::String& name)
    {
        label.setName (name + " numeric entry");
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, cream);
        // The temporary entry is text-only: no filled rectangle, border, or frame.
        label.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label.setBorderSize ({ 0, 0, 0, 0 });
        // JUCE Label::onEditorShow is a no-argument callback in the JUCE version
        // used by this project. Retrieve the active TextEditor from the Label.
        label.onEditorShow = [&label]
        {
            if (auto* editor = label.getCurrentTextEditor())
            {
                editor->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
                editor->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
                editor->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
                editor->setColour (juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
                editor->setBorder ({ 0, 0, 0, 0 });
                editor->setJustification (juce::Justification::centred);
            }
        };
        label.setEditable (false, true, false);
        label.setVisible (false);
        addAndMakeVisible (label);

        label.onEditorHide = [this, &label, &slider]
        {
            auto typed = label.getText().trim();
            typed = typed.replace ("dB", "").replace ("db", "").replace ("%", "").trim();
            if (typed.startsWithChar ('+'))
                typed = typed.substring (1).trim();

            bool valid = typed.isNotEmpty();
            bool seenDigit = false;
            bool seenDecimal = false;
            for (int i = 0; i < typed.length(); ++i)
            {
                const auto ch = typed[i];
                if (ch >= '0' && ch <= '9')
                    seenDigit = true;
                else if (ch == '.' && ! seenDecimal)
                    seenDecimal = true;
                else if (ch == '-' && i == 0)
                    ;
                else
                    valid = false;
            }
            valid = valid && seenDigit;
            if (valid)
                slider.setValue (typed.getDoubleValue(), juce::sendNotificationSync);

            label.setVisible (false);
            label.setText (juce::String(), juce::dontSendNotification);
            repaint();
        };

        slider.onNumericEntryRequested = [this, &label, &slider]
        {
            // Show only the numeric value while editing; units remain on the dial scale.
            const auto valueText = juce::String (slider.getValue(), 1);
            label.setText (valueText, juce::dontSendNotification);
            label.setVisible (true);
            label.toFront (true);
            label.showEditor();
        };
    };
    setupNumericEditor (inputValueEditor, inputSlider, "INPUT");
    setupNumericEditor (mixValueEditor, mixSlider, "MIX");
    setupNumericEditor (outputValueEditor, outputSlider, "OUTPUT");

    inputSlider.setRange (-24.0, 24.0, 0.1);
    outputSlider.setRange (-24.0, 24.0, 0.1);
    mixSlider.setRange (0.0, 100.0, 0.1);
    inputSlider.setTextValueSuffix (" dB");
    outputSlider.setTextValueSuffix (" dB");
    mixSlider.setTextValueSuffix (" %");
    // APVTS attachments keep the knobs bidirectionally connected to host automation,
    // saved presets and the DSP parameters. Tooltips expose precise gain values.
    inputAttachment  = std::make_unique<SliderAttachment> (p.parameters, "input", inputSlider);
    mixAttachment    = std::make_unique<SliderAttachment> (p.parameters, "mix", mixSlider);
    outputAttachment = std::make_unique<SliderAttachment> (p.parameters, "output", outputSlider);
    inputSlider.setTooltip ("Input gain: -24 dB to +24 dB. Default: 0 dB.");
    mixSlider.setTooltip ("Dry / wet mix: 0% to 100%. Default: 50%.");
    outputSlider.setTooltip ("Output gain: -24 dB to +24 dB. Default: 0 dB.");
    // Invisible hit targets keep host-specific button chrome away from the artwork.
    // Disable keyboard focus as well, so Nuendo cannot draw a focus rectangle.
    bypassButton.setWantsKeyboardFocus (false);
    bypassButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    // BypassHitTarget derives from Component, which has no setTooltip API.
    // Keep this invisible hit target free of unsupported tooltip calls.
    bypassButton.onClick = [this]
    {
        // The parameter value is 0 for normal processing (NO) and 1 for true bypass (OFF).
        // Check both pointers before use so an unexpected parameter-layout mismatch cannot crash the UI.
        auto* parameter = audioProcessor.parameters.getParameter ("bypass");
        const auto* bypassValue = audioProcessor.parameters.getRawParameterValue ("bypass");
        if (parameter != nullptr && bypassValue != nullptr)
        {
            const bool nextState = bypassValue->load() <= 0.5f;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (nextState ? 1.0f : 0.0f);
            parameter->endChangeGesture();
        }
        repaint();
    };
    addAndMakeVisible (bypassButton);

    // The LOAD NAM label and border are already painted into the reference artwork.
    // Keep a transparent clickable TextButton so its JUCE border/text never doubles them.
    loadButton.setButtonText (juce::String{});
    loadButton.setWantsKeyboardFocus (false);
    loadButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    loadButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    loadButton.setColour (juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
    loadButton.setColour (juce::TextButton::textColourOnId, juce::Colours::transparentBlack);
    // Clicking LOAD NAM opens the supported-file chooser directly. There is no
    // secondary menu; loading a model automatically clears any WAV loop source.
    loadButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Choose a NAM / NAM A2 model or WAV audio", juce::File{}, "*.nam;*.gen;*.wav");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& chooser)
            {
                const auto file = chooser.getResult();
                if (file != juce::File{})
                {
                    const auto extension = file.getFileExtension().toLowerCase();
                    if (extension == ".nam" || extension == ".gen")
                        audioProcessor.loadNamFile (file);
                    else if (extension == ".wav")
                        audioProcessor.loadAudioFile (file);
                    else
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                 "Unsupported file type",
                                                                 "Choose only .nam or .gen model files, or .wav audio files.");
                    updateModelStatus();
                }
            });
    };
    addAndMakeVisible (loadButton);

    // The full main interface is the only supported editor layout.


    // The faceplate already contains the selector well. Make the ComboBox fill and
    // outline transparent to avoid a second rectangle, while retaining live text/arrow.
    modelSelector.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff101213));
    modelSelector.setColour (juce::ComboBox::textColourId, cream);
    modelSelector.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    modelSelector.setColour (juce::ComboBox::arrowColourId, cream);
    modelSelector.setWantsKeyboardFocus (false);
    modelSelector.addItem ("UREI 1176-SF - Fast Control.nam", 1);
    modelSelector.setSelectedId (1, juce::dontSendNotification);
    modelSelector.onChange = [this]
    {
        const auto model = juce::File (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                                         .getChildFile ("VUMisterNAM/Models/UREI 1176-SF - Fast Control.nam"));
        if (model.existsAsFile()) audioProcessor.loadNamFile (model);
        updateModelStatus();
    };
    addAndMakeVisible (modelSelector);

    modelLabel.setColour (juce::Label::textColourId, cream);
    modelLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    modelLabel.setJustificationType (juce::Justification::centredLeft);
    modelLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff151718));
    modelLabel.setTooltip ("Currently loaded model or audio file");
    addAndMakeVisible (modelLabel);
    // The model selector displays the active model filename in the main layout.
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd6d0c3));
    statusLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    updateModelStatus();
    startTimerHz (30);
}

VUMisterNAMAudioProcessorEditor::~VUMisterNAMAudioProcessorEditor()
{
    inputSlider.setLookAndFeel (nullptr);
    mixSlider.setLookAndFeel (nullptr);
    outputSlider.setLookAndFeel (nullptr);
}

void VUMisterNAMAudioProcessorEditor::styleSlider (juce::Slider&, const juce::String&) {}

void VUMisterNAMAudioProcessorEditor::updateModelStatus()
{
    const auto path = audioProcessor.getNamPath();
    const auto audioPath = audioProcessor.getAudioPath();
    const auto activePath = audioPath.isNotEmpty() ? audioPath : path;
    const auto fileName = activePath.isNotEmpty() ? juce::File (activePath).getFileName()
                                                   : juce::String ("No NAM / WAV file loaded");
    modelLabel.setText (fileName, juce::dontSendNotification);
    modelLabel.setTooltip (activePath.isNotEmpty() ? activePath : "No NAM / WAV file loaded");
    statusLabel.setText (audioProcessor.getNamStatus(), juce::dontSendNotification);

    // Reflect the actual loaded path in the selector, including files chosen
    // outside the bundled Models folder. Do not trigger onChange while refreshing.
    modelSelector.clear (juce::dontSendNotification);
    modelSelector.addItem (fileName, 1);
    modelSelector.setSelectedId (1, juce::dontSendNotification);
    modelSelector.setTooltip (path.isNotEmpty() ? path : "No NAM model loaded");
    const auto audioSummary = audioPath.isNotEmpty()
        ? "Audio loop: " + juce::File (audioPath).getFileName() + " | " + audioProcessor.getAudioStatus()
        : audioProcessor.getAudioStatus();
    loadButton.setTooltip ((path.isNotEmpty() ? "NAM: " + path : "NAM: no model loaded")
                           + "\n" + audioSummary);
}

void VUMisterNAMAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (panel);
    g.fillAll (panel);
    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::stretchToFit);
    else
    {
        g.setColour (cream);
        g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
        g.drawText ("VUMISTER NAM", getLocalBounds(), juce::Justification::centred);
    }

    // Mask the static artwork knobs so the live rotary controls are visible and interactive.
    const float sx = (float) getWidth() / 1522.0f;
    const float sy = (float) getHeight() / 833.0f;
    for (const auto centre : { juce::Point<float> (212.0f * sx, 565.0f * sy),
                               juce::Point<float> (574.0f * sx, 565.0f * sy),
                               juce::Point<float> (942.0f * sx, 565.0f * sy) })
        drawMetalKnobWell (g, { centre.x - 91.0f * sx, centre.y - 91.0f * sy,
                                182.0f * sx, 182.0f * sy });

    // Keep the supplied VU face artwork completely intact. Only the live needles
    // are drawn over it; the scale labels and overload band come from the reference image.
    const float meterScale = juce::jmin (sx, sy);
    const juce::Point<float> inputVuPivot (332.0f * sx, 350.0f * sy);
    const juce::Point<float> outputVuPivot (1190.0f * sx, 350.0f * sy);

    // -18 dBFS RMS maps to the printed 0 VU mark. The tip coordinates are calibrated
    // against the supplied 1522 x 833 reference image, not a generic circular dial.
    constexpr float vuZeroReferenceDbfs = -18.0f;
    // Display-only VU offset: move both needles up by +3 dB without changing audio gain.
    constexpr float vuDisplayGainDb = 3.0f;
    const auto drawNeedle = [&g, meterScale] (float db, juce::Point<float> pivot)
    {
        const auto offset = vuNeedleTipOffset (db);
        const juce::Point<float> tip (pivot.x + offset.x * meterScale,
                                      pivot.y + offset.y * meterScale);
        const juce::Point<float> direction = tip - pivot;
        const float length = std::sqrt (direction.x * direction.x + direction.y * direction.y);
        if (length <= 1.0f) return;
        const juce::Point<float> normal (-direction.y / length, direction.x / length);
        const float baseHalfWidth = 2.1f * meterScale;
        const float tipHalfWidth = 0.55f * meterScale;

        juce::Path shadow;
        shadow.startNewSubPath (pivot.x + normal.x * baseHalfWidth + 1.4f * meterScale,
                                pivot.y + normal.y * baseHalfWidth + 1.4f * meterScale);
        shadow.lineTo (tip.x + normal.x * tipHalfWidth + 1.4f * meterScale,
                       tip.y + normal.y * tipHalfWidth + 1.4f * meterScale);
        shadow.lineTo (tip.x - normal.x * tipHalfWidth + 1.4f * meterScale,
                       tip.y - normal.y * tipHalfWidth + 1.4f * meterScale);
        shadow.lineTo (pivot.x - normal.x * baseHalfWidth + 1.4f * meterScale,
                       pivot.y - normal.y * baseHalfWidth + 1.4f * meterScale);
        shadow.closeSubPath();
        g.setColour (juce::Colour (0x99000000));
        g.fillPath (shadow);

        juce::Path needle;
        needle.startNewSubPath (pivot.x + normal.x * baseHalfWidth, pivot.y + normal.y * baseHalfWidth);
        needle.lineTo (tip.x + normal.x * tipHalfWidth, tip.y + normal.y * tipHalfWidth);
        needle.lineTo (tip.x - normal.x * tipHalfWidth, tip.y - normal.y * tipHalfWidth);
        needle.lineTo (pivot.x - normal.x * baseHalfWidth, pivot.y - normal.y * baseHalfWidth);
        needle.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb13b), pivot.x, pivot.y,
                                                 juce::Colour (0xffe45c18), tip.x, tip.y, false));
        g.fillPath (needle);
        g.setColour (juce::Colour (0xffffe1a0).withAlpha (0.9f));
        g.drawLine (pivot.x + normal.x * 0.5f * meterScale,
                    pivot.y + normal.y * 0.5f * meterScale,
                    tip.x, tip.y, 0.55f * meterScale);

        // A compact dark hub keeps the needle anchored without hiding the reference face.
        g.setColour (juce::Colour (0xff151718));
        g.fillEllipse (pivot.x - 5.0f * meterScale, pivot.y - 5.0f * meterScale,
                       10.0f * meterScale, 10.0f * meterScale);
        g.setColour (juce::Colour (0xffc6a05b));
        g.drawEllipse (pivot.x - 5.0f * meterScale, pivot.y - 5.0f * meterScale,
                       10.0f * meterScale, 10.0f * meterScale, 0.9f * meterScale);
        g.setColour (juce::Colour (0xffffe2a4));
        g.fillEllipse (pivot.x - 1.3f * meterScale, pivot.y - 1.3f * meterScale,
                       2.6f * meterScale, 2.6f * meterScale);
    };
    drawNeedle (audioProcessor.getInputMeterDb() - vuZeroReferenceDbfs + vuDisplayGainDb, inputVuPivot);
    drawNeedle (audioProcessor.getOutputMeterDb() - vuZeroReferenceDbfs + vuDisplayGainDb, outputVuPivot);

    // CURRENT and MAX use larger, bold live readouts in the unused lower strip of each
    // meter face. They use the same display-only +3 dB calibration as the needles.
    const auto formatVuValue = [] (float value)
    {
        return juce::String (value >= 0.0f ? "+" : "") + juce::String (value, 1);
    };
    const auto drawCurrentMax = [&g, sx, sy, meterScale, &formatVuValue]
                                (float xCurrent, float xMax, float current, float maximum)
    {
        g.setColour (juce::Colour (0xff30261a));
        g.setFont (juce::Font (juce::jmax (12.0f, 18.0f * meterScale), juce::Font::bold));
        g.drawText ("CURRENT " + formatVuValue (current),
                    juce::Rectangle<int> ((int) (xCurrent * sx), (int) (329.0f * sy),
                                          (int) (190.0f * sx), (int) (22.0f * sy)),
                    juce::Justification::centred, false);
        g.setColour (maximum >= 1.0f ? juce::Colour (0xffe53935) : juce::Colour (0xff30261a));
        g.drawText ("MAX " + formatVuValue (maximum),
                    juce::Rectangle<int> ((int) (xMax * sx), (int) (329.0f * sy),
                                          (int) (170.0f * sx), (int) (22.0f * sy)),
                    juce::Justification::centred, false);
    };
    drawCurrentMax (105.0f, 382.0f, currentInputVuDb, maxInputVuDb);
    drawCurrentMax (963.0f, 1240.0f, currentOutputVuDb, maxOutputVuDb);

    // The exact reference artwork already contains the physical bypass rocker,
    // OFF/ON labels and orange pilot lamp. Preserve it; extinguish only the lamp
    // when true bypass is active so the artwork and the live parameter agree.
    const auto* bypassValue = audioProcessor.parameters.getRawParameterValue ("bypass");
    const bool bypassOn = bypassValue != nullptr && bypassValue->load() > 0.5f;
    if (bypassOn)
    {
        g.setColour (juce::Colour (0xff151718));
        g.fillRoundedRectangle (juce::Rectangle<float> (1314.0f * sx, 573.0f * sy,
                                                         43.0f * sx, 12.0f * sy),
                                3.0f * meterScale);
    }

}

void VUMisterNAMAudioProcessorEditor::resized()
{
    const float sx = (float) getWidth() / 1522.0f;
    const float sy = (float) getHeight() / 833.0f;
    auto scaled = [sx, sy] (int x, int y, int w, int h) { return juce::Rectangle<int> ((int) (x * sx), (int) (y * sy), (int) (w * sx), (int) (h * sy)); };

    // Keep each interactive rotary inside its own faceplate well and aligned
    // to the artwork. The printed numeric scale around each knob remains visible.
    inputSlider.setBounds (scaled (82, 455, 260, 220));
    mixSlider.setBounds (scaled (444, 455, 260, 220));
    outputSlider.setBounds (scaled (812, 455, 260, 220));
    inputValueEditor.setBounds (scaled (172, 553, 80, 25));
    mixValueEditor.setBounds (scaled (534, 553, 80, 25));
    outputValueEditor.setBounds (scaled (902, 553, 80, 25));

    // Keep the hit target over the physical rocker only. Clicking it toggles the
    // host-automatable APVTS bypass parameter; the OFF/NO legends are masked in paint().
    bypassButton.setBounds (scaled (1290, 536, 94, 98));
    modelSelector.setBounds (scaled (220, 744, 555, 56));
    loadButton.setBounds (scaled (800, 744, 260, 56));
    // Status is available via tooltips; leave the bottom artwork row unobstructed.
    modelLabel.setBounds (0, 0, 0, 0);
    statusLabel.setBounds (0, 0, 0, 0);
}

void VUMisterNAMAudioProcessorEditor::mouseUp (const juce::MouseEvent& event)
{
    const auto p = event.getPosition().toFloat();

    // Full-interface hit areas follow the original reference artwork coordinates.
    const float sx = (float) getWidth() / 1522.0f;
    const float sy = (float) getHeight() / 833.0f;
    const auto inMaxHitArea = [&p, sx, sy] (float x)
    {
        return p.x >= x * sx && p.x < (x + 170.0f) * sx
            && p.y >= 329.0f * sy && p.y < 351.0f * sy;
    };
    if (inMaxHitArea (382.0f))
    {
        maxInputVuDb = 0.0f;
        inputMaxHasSignal = true;
        repaint();
    }
    else if (inMaxHitArea (1240.0f))
    {
        maxOutputVuDb = 0.0f;
        outputMaxHasSignal = true;
        repaint();
    }
}

void VUMisterNAMAudioProcessorEditor::timerCallback()
{
    // INPUT / MIX / OUTPUT numeric fields are hidden except during double-click entry.

    // CURRENT / MAX readouts use a -20 dB to +24 dB display range and start at 0 dB.
    // CURRENT follows the live signal; MAX captures the highest reading after signal
    // activity begins, and remains at its 0 dB startup value until then.
    constexpr float vuZeroReferenceDbfs = -18.0f;
    constexpr float vuDisplayGainDb = 3.0f;
    const float inputDbfs = audioProcessor.getInputMeterDb();
    const float outputDbfs = audioProcessor.getOutputMeterDb();
    currentInputVuDb = juce::jlimit (-20.0f, 24.0f,
        inputDbfs - vuZeroReferenceDbfs + vuDisplayGainDb);
    currentOutputVuDb = juce::jlimit (-20.0f, 24.0f,
        outputDbfs - vuZeroReferenceDbfs + vuDisplayGainDb);

    // Avoid treating digital silence as a real MAX peak. Once signal is present,
    // negative readings are valid peaks too and must be held accurately.
    constexpr float signalActivityFloorDbfs = -90.0f;
    if (inputDbfs > signalActivityFloorDbfs)
    {
        if (! inputMaxHasSignal)
        {
            maxInputVuDb = currentInputVuDb;
            inputMaxHasSignal = true;
        }
        else
            maxInputVuDb = juce::jmax (maxInputVuDb, currentInputVuDb);
    }
    if (outputDbfs > signalActivityFloorDbfs)
    {
        if (! outputMaxHasSignal)
        {
            maxOutputVuDb = currentOutputVuDb;
            outputMaxHasSignal = true;
        }
        else
            maxOutputVuDb = juce::jmax (maxOutputVuDb, currentOutputVuDb);
    }
    repaint();
}
