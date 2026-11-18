#pragma once

#include "Pattern.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace acid::MidiExport
{

constexpr int ticksPerQuarter = 960;
constexpr int ticksPerStep = ticksPerQuarter / 4;

/** Converts a pattern to a single-track MIDI file, repeated `repeats` times.
    Accents → velocity 127 (others 90). Slides/ties → notes that overlap the next
    note by a 32nd, which Live (and this plugin's MIDI mode) plays legato. */
juce::MidiFile toMidiFile (const Pattern&, int repeats = 1, double gate = 0.55, int transpose = 0);

bool writeToFile (const Pattern&, const juce::File&, int repeats = 1, double gate = 0.55, int transpose = 0);

} // namespace acid::MidiExport
