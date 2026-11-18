#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <cstdint>

namespace acid
{

enum class Gate : uint8_t { Rest = 0, Note = 1, Tie = 2 };

/** One 16th-note step, 303 style.
    - semitone: 0..12 within the octave (12 = the C above)
    - octave:   -1..+2 relative to the base octave
    - slide:    glide from THIS step into the next one (gate held, no retrigger)
    - tie:      expressed as gate == Tie: continue the previous note
    - locked:   generator/mutate leave this step untouched */
struct Step
{
    int8_t semitone = 0;
    int8_t octave = 0;
    Gate gate = Gate::Rest;
    bool accent = false;
    bool slide = false;
    bool locked = false;

    bool operator== (const Step&) const = default;
};

struct Pattern
{
    static constexpr int maxSteps = 32;
    static constexpr int minOctave = -1, maxOctave = 2;
    static constexpr int baseNote = 36; ///< MIDI note for semitone 0, octave 0 ("C1" in Live)

    std::array<Step, maxSteps> steps {};
    int length = 16;

    Step& operator[] (int i) { return steps[(size_t) wrap (i)]; }
    const Step& operator[] (int i) const { return steps[(size_t) wrap (i)]; }

    int wrap (int i) const
    {
        const int m = i % length;
        return m < 0 ? m + length : m;
    }

    /** MIDI note of step i (ignores gate). */
    int midiNoteOf (int i) const
    {
        const auto& s = (*this)[i];
        return baseNote + s.octave * 12 + s.semitone;
    }

    /** Resolved gate: a Tie only sounds if it continues a Note. */
    Gate effectiveGate (int i) const;

    bool operator== (const Pattern&) const = default;

    juce::ValueTree toValueTree() const;
    static Pattern fromValueTree (const juce::ValueTree&);
};

struct PatternBank
{
    static constexpr int numSlots = 16;
    std::array<Pattern, numSlots> slots {};
};

} // namespace acid
