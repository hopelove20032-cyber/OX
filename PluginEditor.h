#pragma once

#include "PluginProcessor.h"
#include <array>
#include <cstring>
#include <functional>
#include <memory>

namespace oxui
{
inline const juce::Colour kRed (0xffe5242b);
inline const juce::Colour kLed (0xffff2a2a);

//==============================================================================
class OxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OxLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1c20));
        setColour (juce::PopupMenu::textColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, kRed);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kRed);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float thick = juce::jmax (2.0f, radius * 0.09f);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff3a3a40));
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour (kRed);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float kr = radius * 0.72f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5e5e66), centre.x - kr * 0.5f, centre.y - kr * 0.7f,
                                                 juce::Colour (0xff1a1a1e), centre.x + kr * 0.6f, centre.y + kr * 0.8f, false));
        g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.2f);

        juce::Path p;
        p.addRoundedRectangle (-1.8f, -kr + 2.0f, 3.6f, kr * 0.55f, 1.4f);
        g.setColour (juce::Colours::white);
        g.fillPath (p, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                           float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        auto track = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        g.setColour (juce::Colour (0xff060607));
        g.fillRoundedRectangle (track, 6.0f);
        g.setColour (juce::Colour (0xff2a2a2e));
        g.drawRoundedRectangle (track.reduced (0.5f), 6.0f, 1.0f);

        const float th = juce::jmin (40.0f, (float) h * 0.3f);
        const float ty = juce::jlimit ((float) y, (float) (y + h) - th, sliderPos - th * 0.5f);
        auto thumb = juce::Rectangle<float> ((float) x + 2.0f, ty, (float) w - 4.0f, th);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff70707a), thumb.getX(), thumb.getY(),
                                                 juce::Colour (0xff2a2a2f), thumb.getX(), thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 5.0f);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        for (int i = 1; i < 6; ++i)
        {
            const float yy = thumb.getY() + (float) i * th / 6.0f;
            g.drawLine (thumb.getX() + 3.0f, yy, thumb.getRight() - 3.0f, yy, 1.0f);
        }
        g.setColour (kRed);
        g.fillRect (thumb.getX() + 3.0f, thumb.getCentreY() - 1.0f, thumb.getWidth() - 6.0f, 2.0f);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 20; }
};

//==============================================================================
// Red LED style voice display. Click = open voice menu, arrows / wheel = step.
class VoiceDisplay : public juce::Component
{
public:
    std::function<void()> onOpenMenu, onPrev, onNext;

    void setVoice (int idx)
    {
        index = idx;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float s = b.getHeight() / 100.0f;

        g.setColour (juce::Colour (0xff050507));
        g.fillRoundedRectangle (b, 8.0f * s);
        g.setColour (juce::Colour (0xff2b2b31));
        g.drawRoundedRectangle (b.reduced (1.0f), 8.0f * s, 2.0f);

        const auto& list = ox::patches();
        const auto& p = list[(size_t) juce::jlimit (0, (int) list.size() - 1, index)];

        auto arrow = [&] (juce::Rectangle<float> r, bool left)
        {
            juce::Path t;
            if (left) t.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY());
            else      t.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
            g.setColour (kLed.withAlpha (0.85f));
            g.fillPath (t);
        };

        const float aw = 14.0f * s, ah = 22.0f * s;
        arrow ({ b.getX() + 14.0f * s, b.getCentreY() - ah * 0.5f, aw, ah }, true);
        arrow ({ b.getRight() - 14.0f * s - aw, b.getCentreY() - ah * 0.5f, aw, ah }, false);

        auto glowText = [&] (const juce::String& txt, juce::Rectangle<int> area, juce::Font f, juce::Justification j)
        {
            g.setFont (f);
            g.setColour (kLed.withAlpha (0.16f));
            for (int dx = -1; dx <= 1; dx += 2)
                for (int dy = -1; dy <= 1; dy += 2)
                    g.drawFittedText (txt, area.translated (dx * 2, dy * 2), j, 1, 0.6f);
            g.setColour (kLed);
            g.drawFittedText (txt, area, j, 1, 0.6f);
        };

        const auto mono = juce::Font::getDefaultMonospacedFontName();
        auto fontBig   = juce::Font (juce::FontOptions (mono, 40.0f * s, juce::Font::bold));
        auto fontName  = juce::Font (juce::FontOptions (mono, 30.0f * s, juce::Font::bold));
        auto fontSmall = juce::Font (juce::FontOptions (mono, 12.0f * s, juce::Font::plain));

        const auto bi = b.toNearestInt();
        glowText (juce::String (index + 1).paddedLeft ('0', 2),
                  { bi.getX() + (int) (44 * s), bi.getY() + (int) (28 * s), (int) (70 * s), (int) (50 * s) },
                  fontBig, juce::Justification::centred);

        glowText (juce::String (p.name).toUpperCase(),
                  { bi.getX() + (int) (122 * s), bi.getY() + (int) (30 * s), (int) (290 * s), (int) (44 * s) },
                  fontName, juce::Justification::centredLeft);

        glowText (juce::String (p.category).toUpperCase(),
                  { bi.getX() + (int) (122 * s), bi.getY() + (int) (10 * s), (int) (200 * s), (int) (18 * s) },
                  fontSmall, juce::Justification::centredLeft);

        g.setFont (fontSmall);
        g.setColour (kLed.withAlpha (0.35f));
        g.drawText ("CLICK TO SELECT VOICE", bi.withTrimmedTop ((int) (80 * s)).withTrimmedRight ((int) (44 * s)),
                    juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float x = (float) e.x / (float) juce::jmax (1, getWidth());
        if (x < 0.1f)       { if (onPrev) onPrev(); }
        else if (x > 0.9f)  { if (onNext) onNext(); }
        else                { if (onOpenMenu) onOpenMenu(); }
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY > 0.0f) { if (onPrev) onPrev(); }
        else if (w.deltaY < 0.0f) { if (onNext) onNext(); }
    }

    juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::PointingHandCursor; }

private:
    int index = 0;
};

//==============================================================================
class LedButton : public juce::Button
{
public:
    explicit LedButton (const juce::String& n) : juce::Button (n) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        const float s = b.getHeight() / 56.0f;

        g.setColour (down ? juce::Colour (0xff1c1c20) : (over ? juce::Colour (0xff35353c) : juce::Colour (0xff2a2a30)));
        g.fillRoundedRectangle (b, 6.0f * s);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (b, 6.0f * s, 1.0f);

        const bool on = getToggleState();
        const float r = 4.5f * s;
        const juce::Point<float> c (b.getCentreX(), b.getY() + 14.0f * s);
        if (on)
        {
            g.setColour (kLed.withAlpha (0.25f));
            g.fillEllipse (c.x - r * 2.0f, c.y - r * 2.0f, r * 4.0f, r * 4.0f);
        }
        g.setColour (on ? kLed : juce::Colour (0xff4a1014));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        g.setColour (juce::Colours::white.withAlpha (on ? 0.95f : 0.7f));
        g.setFont (juce::Font (juce::FontOptions (11.5f * s, juce::Font::bold)));
        g.drawText (getButtonText().toUpperCase(), b.withTrimmedTop (26.0f * s).toNearestInt(), juce::Justification::centred);
    }
};
} // namespace oxui

//==============================================================================
class OxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OxEditor (OxAudioProcessor&);
    ~OxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    int  currentVoice() const;
    void setVoice (int index);
    void stepVoice (int delta);
    void showMenu (juce::Component* target, int categoryIndex);
    void updateCategoryLeds();

    OxAudioProcessor& proc;
    oxui::OxLookAndFeel laf;
    oxui::VoiceDisplay display;
    std::vector<std::unique_ptr<oxui::LedButton>> catButtons;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::array<Knob, 6> knobs;

    juce::Slider bendSlider, modSlider;
    juce::MidiKeyboardComponent keyboard;
    int shownVoice = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OxEditor)
};
