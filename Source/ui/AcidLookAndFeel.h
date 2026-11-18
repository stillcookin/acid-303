#pragma once

#include "Theme.h"

namespace acid::ui
{

class AcidLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AcidLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& bg,
                               bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return theme::font (juce::jmin (12.0f, (float) h * 0.62f), true); }
    juce::Font getLabelFont (juce::Label&) override { return theme::font (12.0f); }
};

} // namespace acid::ui
