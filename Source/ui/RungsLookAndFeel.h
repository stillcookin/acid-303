#pragma once

#include "Theme.h"

namespace acid::ui
{

/** Pixel-art look: bevelled panels and buttons, 14×14 pixel knobs, pixel
    fonts. Holds the palette of its window's world; MainView calls setWorld(). */
class RungsLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RungsLookAndFeel();

    void setWorld (World w);
    World getWorld() const { return world; }
    const theme::Palette& getPalette() const { return *palette; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& bg,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::labelFont (17.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return theme::labelFont (juce::jmin (17.0f, (float) h * 0.8f)); }
    juce::Font getLabelFont (juce::Label&) override { return theme::labelFont (17.0f); }
    juce::Font getPopupMenuFont() override { return theme::labelFont (18.0f); }

private:
    void applyColours();

    World world = World::Classic;
    const theme::Palette* palette = &theme::palette (World::Classic);
};

} // namespace acid::ui
