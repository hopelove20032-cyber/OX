#include "PluginEditor.h"

namespace
{
constexpr float kDesignW = 1000.0f;
constexpr float kDesignH = 440.0f;
}

OxEditor::OxEditor (OxAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&laf);

    // ---- voice display ----
    addAndMakeVisible (display);
    display.onOpenMenu = [this] { showMenu (&display, -1); };
    display.onPrev     = [this] { stepVoice (-1); };
    display.onNext     = [this] { stepVoice (1); };

    // ---- category buttons ----
    int c = 0;
    for (const char* cat : ox::categories())
    {
        auto b = std::make_unique<oxui::LedButton> (cat);
        auto* raw = b.get();
        const int ci = c++;
        b->onClick = [this, raw, ci] { showMenu (raw, ci); };
        addAndMakeVisible (*b);
        catButtons.push_back (std::move (b));
    }

    // ---- knobs ----
    const char* ids[6]   = { "level", "tone", "attack", "release", "chorus", "reverb" };
    const char* names[6] = { "LEVEL", "TONE", "ATTACK", "RELEASE", "CHORUS", "REVERB" };

    for (int i = 0; i < 6; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                      juce::MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (k.slider);

        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.slider);

        if (auto* par = proc.apvts.getParameter (ids[i]))
            k.slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
    }

    // ---- wheels ----
    bendSlider.setSliderStyle (juce::Slider::LinearVertical);
    bendSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    bendSlider.setRange (-1.0, 1.0, 0.0);
    bendSlider.setValue (0.0, juce::dontSendNotification);
    bendSlider.onValueChange = [this] { proc.shared.bend.store ((float) bendSlider.getValue()); };
    bendSlider.onDragEnd     = [this] { bendSlider.setValue (0.0, juce::sendNotificationSync); };
    addAndMakeVisible (bendSlider);

    modSlider.setSliderStyle (juce::Slider::LinearVertical);
    modSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    modSlider.setRange (0.0, 1.0, 0.0);
    modSlider.setValue (0.0, juce::dontSendNotification);
    modSlider.onValueChange = [this] { proc.shared.mod.store ((float) modSlider.getValue()); };
    addAndMakeVisible (modSlider);

    // ---- keyboard ----
    keyboard.setAvailableRange (36, 96);   // C2 - C7, 61 keys
    keyboard.setLowestVisibleKey (36);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setBlackNoteLengthProportion (0.62f);
    keyboard.setVelocity (0.8f, true);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff5f2ea));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff131315));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff8a8a8a));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x20ff2a2a));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0x90e5242b));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff666666));
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (700, 308, 1600, 704);
    getConstrainer()->setFixedAspectRatio ((double) (kDesignW / kDesignH));
    setSize ((int) kDesignW, (int) kDesignH);

    timerCallback();
    startTimerHz (20);
}

OxEditor::~OxEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int OxEditor::currentVoice() const
{
    const auto* v = proc.apvts.getRawParameterValue ("voice");
    const int n = (int) ox::patches().size();
    return juce::jlimit (0, n - 1, (int) std::lround (v->load()));
}

void OxEditor::setVoice (int index)
{
    const int n = (int) ox::patches().size();
    index = juce::jlimit (0, n - 1, index);

    if (auto* par = proc.apvts.getParameter ("voice"))
    {
        par->beginChangeGesture();
        par->setValueNotifyingHost (par->convertTo0to1 ((float) index));
        par->endChangeGesture();
    }

    timerCallback();
}

void OxEditor::stepVoice (int delta)
{
    const int n = (int) ox::patches().size();
    setVoice ((currentVoice() + delta + n) % n);
}

void OxEditor::showMenu (juce::Component* target, int categoryIndex)
{
    const auto& list = ox::patches();
    const int cur = currentVoice();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&laf);

    if (categoryIndex < 0)
    {
        for (const char* cat : ox::categories())
        {
            juce::PopupMenu sub;
            sub.setLookAndFeel (&laf);
            for (int i = 0; i < (int) list.size(); ++i)
                if (std::strcmp (list[(size_t) i].category, cat) == 0)
                    sub.addItem (i + 1, list[(size_t) i].name, true, i == cur);
            menu.addSubMenu (cat, sub);
        }
    }
    else
    {
        const char* cat = ox::categories()[(size_t) categoryIndex];
        menu.addSectionHeader (cat);
        for (int i = 0; i < (int) list.size(); ++i)
            if (std::strcmp (list[(size_t) i].category, cat) == 0)
                menu.addItem (i + 1, list[(size_t) i].name, true, i == cur);
    }

    juce::Component::SafePointer<OxEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target).withMinimumWidth (190),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->setVoice (result - 1);
                        });
}

void OxEditor::updateCategoryLeds()
{
    const auto& list = ox::patches();
    const char* cat = list[(size_t) shownVoice].category;
    int c = 0;
    for (const char* name : ox::categories())
        catButtons[(size_t) c++]->setToggleState (std::strcmp (name, cat) == 0, juce::dontSendNotification);
}

void OxEditor::timerCallback()
{
    const int cur = currentVoice();
    if (cur != shownVoice)
    {
        shownVoice = cur;
        display.setVoice (cur);
        updateCategoryLeds();
    }
}

void OxEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / kDesignW;
    const auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8202e), 0.0f, 0.0f,
                                             juce::Colour (0xff9e0f1b), 0.0f, b.getHeight(), false));
    g.fillAll();

    // control panel
    const juce::Rectangle<float> panel (16.0f * s, 16.0f * s, 968.0f * s, 250.0f * s);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e34), panel.getX(), panel.getY(),
                                             juce::Colour (0xff1b1b20), panel.getX(), panel.getBottom(), false));
    g.fillRoundedRectangle (panel, 10.0f * s);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (panel, 10.0f * s, 1.5f);

    // logo
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (88.0f * s, juce::Font::bold)));
    g.drawText ("OX", juce::Rectangle<float> (30.0f * s, 14.0f * s, 220.0f * s, 100.0f * s).toNearestInt(),
                juce::Justification::centredLeft);
    g.setColour (oxui::kRed);
    g.fillRoundedRectangle (34.0f * s, 112.0f * s, 190.0f * s, 5.0f * s, 2.0f * s);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (10.5f * s, juce::Font::bold)));
    g.drawText ("VIRTUAL STAGE INSTRUMENT",
                juce::Rectangle<float> (34.0f * s, 120.0f * s, 220.0f * s, 14.0f * s).toNearestInt(),
                juce::Justification::centredLeft);

    // section labels
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (10.0f * s, juce::Font::bold)));
    g.drawText ("VOICE CATEGORY", juce::Rectangle<float> (28.0f * s, 146.0f * s, 200.0f * s, 14.0f * s).toNearestInt(),
                juce::Justification::centredLeft);
    g.drawText ("SOUND SHAPING / EFFECTS", juce::Rectangle<float> (665.0f * s, 146.0f * s, 300.0f * s, 14.0f * s).toNearestInt(),
                juce::Justification::centredLeft);

    // key bed
    g.setColour (juce::Colour (0xff0d0d0f));
    g.fillRoundedRectangle (16.0f * s, 272.0f * s, 968.0f * s, 154.0f * s, 8.0f * s);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (9.5f * s, juce::Font::bold)));
    g.drawText ("PITCH", juce::Rectangle<float> (30.0f * s, 274.0f * s, 46.0f * s, 12.0f * s).toNearestInt(), juce::Justification::centred);
    g.drawText ("MOD",   juce::Rectangle<float> (74.0f * s, 274.0f * s, 46.0f * s, 12.0f * s).toNearestInt(), juce::Justification::centred);
}

void OxEditor::resized()
{
    const float s = (float) getWidth() / kDesignW;
    auto R = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * s), juce::roundToInt (y * s),
                                     juce::roundToInt (w * s), juce::roundToInt (h * s));
    };

    display.setBounds (R (270, 30, 460, 100));

    // top-right knobs: level, tone
    for (int i = 0; i < 2; ++i)
    {
        const float x = 760.0f + (float) i * 110.0f;
        knobs[(size_t) i].slider.setBounds (R (x, 24, 100, 84));
        knobs[(size_t) i].label.setBounds (R (x, 108, 100, 16));
        knobs[(size_t) i].label.setFont (juce::Font (juce::FontOptions (11.0f * s, juce::Font::bold)));
    }

    // category buttons
    for (size_t c = 0; c < catButtons.size(); ++c)
        catButtons[c]->setBounds (R (28.0f + (float) c * 78.0f, 168, 72, 56));

    // bottom-right knobs: attack, release, chorus, reverb
    for (int i = 2; i < 6; ++i)
    {
        const float x = 665.0f + (float) (i - 2) * 78.0f;
        knobs[(size_t) i].slider.setBounds (R (x, 160, 78, 66));
        knobs[(size_t) i].label.setBounds (R (x, 228, 78, 16));
        knobs[(size_t) i].label.setFont (juce::Font (juce::FontOptions (11.0f * s, juce::Font::bold)));
    }

    bendSlider.setBounds (R (40, 290, 34, 130));
    modSlider.setBounds (R (84, 290, 34, 130));

    keyboard.setBounds (R (136, 282, 836, 142));
    keyboard.setKeyWidth (keyboard.getWidth() / 36.0f);
}
