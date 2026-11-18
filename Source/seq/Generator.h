#pragma once

#include "Pattern.h"

#include <cstdint>
#include <vector>

namespace acid
{

struct Scale
{
    const char* name;
    std::vector<int> intervals; ///< semitones from the root, ascending, within one octave
};

const std::vector<Scale>& scales();
const std::vector<const char*>& rootNames();

/** User-facing knobs of the pattern generator (not host-automatable; stored with the plugin state). */
struct GeneratorSettings
{
    int scale = 0;        ///< index into scales()
    int root = 0;         ///< 0 = C .. 11 = B
    float density = 0.75f;
    float accent = 0.3f;
    float slide = 0.2f;
    float octave = 0.2f;  ///< probability of an octave-flagged step
    int range = 1;        ///< melodic span in octaves: 1 or 2
    float mutateAmount = 0.25f;
    uint32_t seed = 303;  ///< seed of the last generate()

    bool operator== (const GeneratorSettings&) const = default;

    juce::ValueTree toValueTree() const;
    static GeneratorSettings fromValueTree (const juce::ValueTree&);
};

/** Pure, deterministic pattern operations. Locked steps are never changed.
    All functions keep the input pattern's length. */
namespace Generator
{
    Pattern generate (const Pattern& current, const GeneratorSettings&, uint32_t seed);
    Pattern mutate (const Pattern& current, const GeneratorSettings&, uint32_t seed);
    Pattern randomizeAccents (const Pattern& current, const GeneratorSettings&, uint32_t seed);
    Pattern randomizeSlides (const Pattern& current, const GeneratorSettings&, uint32_t seed);
    Pattern randomizeOctaves (const Pattern& current, const GeneratorSettings&, uint32_t seed);
    Pattern shift (const Pattern& current, int direction); ///< rotate by ±1 step (ignores locks)
    Pattern reverse (const Pattern& current);              ///< ignores locks

    /** Removes slides that lead into rests and ties that follow rests. */
    void tidy (Pattern&);

    /** True if (root + semitone) is in the scale. */
    bool isInScale (int semitone, const GeneratorSettings&);
}

} // namespace acid
