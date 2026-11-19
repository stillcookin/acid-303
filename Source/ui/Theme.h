#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "seq/World.h"

/** Visual design tokens. Every world has its own palette; one plugin window
    uses the palette of its own world through its look-and-feel (pal()), so
    two instances in different worlds never fight over colours.
    Restyling means editing Theme.cpp (palettes) and RungsLookAndFeel.cpp. */
namespace acid::ui::theme
{
    struct Palette
    {
        // window + panels (2 px bevels: hi on top/left, lo on bottom/right, edge around)
        juce::Colour background, panel, panelHi, panelLo, edge;
        juce::Colour raised, raisedHi, raisedLo;          // buttons, combos
        juce::Colour plate, plateText, plateDim;          // header plates and the second header row
        juce::Colour text, textDim, title;
        juce::Colour acid, acidHi, acidLo, onAcid;        // primary: notes, active
        juce::Colour accent, slide, tie, lock;
        // piano roll
        juce::Colour rollBg, rowWhite, rowWhiteAlt, rowBlack, rowBlackAlt, rollLine, playhead,
                     keyWhite, keyWhiteText, laneOff, laneEdge, rollDim, rollText;
        // pixel knobs
        juce::Colour knobTrack, knobBody, knobHi, knobPointer;
    };

    const Palette& palette (World);

    /** The palette of the look-and-feel this component paints with. */
    const Palette& pal (const juce::Component&);

    // metrics (design units; the editor scales the whole view)
    inline constexpr int baseWidth = 1000;
    inline constexpr int baseHeight = 680;
    inline constexpr int gap = 8;
    inline constexpr int bevel = 2;

    /** Press Start 2P: titles, the RUNGS wordmark. Pixel-exact at multiples of 8. */
    juce::Font titleFont (float height);
    /** VT323: labels, buttons, numbers. */
    juce::Font labelFont (float height);
    /** General UI text (VT323), sized so `height` matches the old sans sizes. */
    inline juce::Font font (float height, bool /*bold*/ = false) { return labelFont (height * 1.45f); }

    // ---- pixel drawing helpers ------------------------------------------
    /** A bevelled game-UI panel with a 2 px notched outline. */
    void pixelPanel (juce::Graphics&, juce::Rectangle<int>, juce::Colour fill, juce::Colour hi, juce::Colour lo, juce::Colour edge);
    /** A 14×14-cell pixel knob, value 0..1, scaled to fit `area`. */
    void pixelKnob (juce::Graphics&, juce::Rectangle<float> area, float value01, const Palette&, bool enabled = true);
}
