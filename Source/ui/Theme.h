#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** All visual design tokens in one place. Restyling the plugin should mostly
    mean editing this file (and AcidLookAndFeel for control shapes). */
namespace acid::ui::theme
{
    // surfaces
    inline const juce::Colour background   { 0xff16171a };
    inline const juce::Colour panel        { 0xff212328 };
    inline const juce::Colour panelRaised  { 0xff2a2d33 };
    inline const juce::Colour outline      { 0xff3a3e46 };
    inline const juce::Colour gridLine     { 0xff2c2f35 };
    inline const juce::Colour gridBeat     { 0xff1c1e22 };
    inline const juce::Colour blackKeyRow  { 0xff1a1c20 };

    // text
    inline const juce::Colour text         { 0xffe6e6e6 };
    inline const juce::Colour textDim      { 0xff8a8f98 };

    // accents
    inline const juce::Colour acid         { 0xffc8f03c }; // primary (notes, active)
    inline const juce::Colour accent       { 0xffff6a3d }; // accented notes
    inline const juce::Colour slide        { 0xff47c6ff }; // slides
    inline const juce::Colour tie          { 0xff8fb52b };
    inline const juce::Colour lock         { 0xffb48cff };
    inline const juce::Colour playhead     { 0x33ffffff };

    // metrics
    inline constexpr int baseWidth = 1000;
    inline constexpr int baseHeight = 680;
    inline constexpr float corner = 4.0f;
    inline constexpr int gap = 8;

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
}
