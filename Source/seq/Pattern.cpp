#include "Pattern.h"

#include <algorithm>

namespace acid
{

Gate Pattern::effectiveGate (int i) const
{
    for (int back = 0; back < length; ++back)
    {
        const auto g = (*this)[i - back].gate;
        if (g == Gate::Rest) return Gate::Rest;
        if (g == Gate::Note) return back == 0 ? Gate::Note : Gate::Tie;
    }
    return Gate::Rest; // all ties: nothing to continue
}

// Steps are stored compactly as "gate.semitone.octave.flags" tokens, where
// flags is a bitmask (1 accent, 2 slide, 4 locked).
juce::ValueTree Pattern::toValueTree() const
{
    juce::ValueTree v ("Pattern");
    v.setProperty ("length", length, nullptr);

    juce::StringArray tokens;
    for (const auto& s : steps)
    {
        const int flags = (s.accent ? 1 : 0) | (s.slide ? 2 : 0) | (s.locked ? 4 : 0);
        tokens.add (juce::String ((int) s.gate) + "." + juce::String ((int) s.semitone) + "."
                    + juce::String ((int) s.octave) + "." + juce::String (flags));
    }
    v.setProperty ("steps", tokens.joinIntoString (" "), nullptr);
    return v;
}

Pattern Pattern::fromValueTree (const juce::ValueTree& v)
{
    Pattern p;
    if (! v.hasType ("Pattern"))
        return p;

    p.length = std::clamp ((int) v.getProperty ("length", 16), 1, maxSteps);

    auto tokens = juce::StringArray::fromTokens (v.getProperty ("steps").toString(), " ", "");
    for (int i = 0; i < std::min (tokens.size(), maxSteps); ++i)
    {
        auto parts = juce::StringArray::fromTokens (tokens[i], ".", "");
        if (parts.size() < 4) continue;
        auto& s = p.steps[(size_t) i];
        s.gate = static_cast<Gate> (std::clamp (parts[0].getIntValue(), 0, 2));
        s.semitone = (int8_t) std::clamp (parts[1].getIntValue(), 0, 12);
        s.octave = (int8_t) std::clamp (parts[2].getIntValue(), minOctave, maxOctave);
        const int flags = parts[3].getIntValue();
        s.accent = (flags & 1) != 0;
        s.slide = (flags & 2) != 0;
        s.locked = (flags & 4) != 0;
    }
    return p;
}

} // namespace acid
