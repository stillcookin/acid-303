#include "Generator.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace acid
{

const std::vector<Scale>& scales()
{
    static const std::vector<Scale> s {
        { "Minor",          { 0, 2, 3, 5, 7, 8, 10 } },
        { "Phrygian",       { 0, 1, 3, 5, 7, 8, 10 } },
        { "Dorian",         { 0, 2, 3, 5, 7, 9, 10 } },
        { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
        { "Minor Pent",     { 0, 3, 5, 7, 10 } },
        { "Blues",          { 0, 3, 5, 6, 7, 10 } },
        { "Major",          { 0, 2, 4, 5, 7, 9, 11 } },
        { "Chromatic",      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    return s;
}

const std::vector<const char*>& rootNames()
{
    static const std::vector<const char*> n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

juce::ValueTree GeneratorSettings::toValueTree() const
{
    juce::ValueTree v ("Generator");
    v.setProperty ("scale", scale, nullptr);
    v.setProperty ("root", root, nullptr);
    v.setProperty ("density", density, nullptr);
    v.setProperty ("accent", accent, nullptr);
    v.setProperty ("slide", slide, nullptr);
    v.setProperty ("octave", octave, nullptr);
    v.setProperty ("range", range, nullptr);
    v.setProperty ("mutate", mutateAmount, nullptr);
    v.setProperty ("seed", (juce::int64) seed, nullptr);
    return v;
}

GeneratorSettings GeneratorSettings::fromValueTree (const juce::ValueTree& v)
{
    GeneratorSettings g;
    if (! v.hasType ("Generator"))
        return g;
    auto f01 = [&] (const char* id, float def) { return std::clamp ((float) (double) v.getProperty (id, def), 0.0f, 1.0f); };
    g.scale = std::clamp ((int) v.getProperty ("scale", 0), 0, (int) scales().size() - 1);
    g.root = std::clamp ((int) v.getProperty ("root", 0), 0, 11);
    g.density = f01 ("density", g.density);
    g.accent = f01 ("accent", g.accent);
    g.slide = f01 ("slide", g.slide);
    g.octave = f01 ("octave", g.octave);
    g.range = std::clamp ((int) v.getProperty ("range", 1), 1, 2);
    g.mutateAmount = f01 ("mutate", g.mutateAmount);
    g.seed = (uint32_t) (juce::int64) v.getProperty ("seed", 303);
    return g;
}

namespace
{
    struct Rng
    {
        explicit Rng (uint32_t seed) : eng (seed) {}
        float uni() { return std::uniform_real_distribution<float> (0.0f, 1.0f) (eng); }
        bool chance (float p) { return uni() < p; }
        int range (int lo, int hiInclusive) { return std::uniform_int_distribution<int> (lo, hiInclusive) (eng); }

        template <typename T>
        T weighted (const std::vector<std::pair<T, float>>& items)
        {
            float total = 0.0f;
            for (auto& it : items) total += it.second;
            float r = uni() * total;
            for (auto& it : items)
            {
                if ((r -= it.second) <= 0.0f)
                    return it.first;
            }
            return items.back().first;
        }

        std::mt19937 eng;
    };

    const Scale& scaleOf (const GeneratorSettings& s)
    {
        return scales()[(size_t) std::clamp (s.scale, 0, (int) scales().size() - 1)];
    }

    /** Scale degree (may be negative or exceed one octave) → (semitone 0..11, octave). */
    void degreeToStep (int degree, const GeneratorSettings& gs, Step& step)
    {
        const auto& iv = scaleOf (gs).intervals;
        const int n = (int) iv.size();
        const int oct = (int) std::floor ((double) degree / n);
        const int idx = degree - oct * n;
        const int abs = gs.root + iv[(size_t) idx] + 12 * oct;
        const int o = (int) std::floor (abs / 12.0);
        step.semitone = (int8_t) (abs - 12 * o);
        step.octave = (int8_t) std::clamp (o, Pattern::minOctave, Pattern::maxOctave);
    }

    /** Nearest scale degree for an existing step (relative to the base octave). */
    int stepToDegree (const Step& step, const GeneratorSettings& gs)
    {
        const auto& iv = scaleOf (gs).intervals;
        const int n = (int) iv.size();
        const int abs = step.semitone + 12 * step.octave - gs.root;
        int best = 0, bestDist = 1000;
        for (int oct = -2; oct <= 3; ++oct)
            for (int i = 0; i < n; ++i)
            {
                const int d = std::abs (iv[(size_t) i] + 12 * oct - abs);
                if (d < bestDist) { bestDist = d; best = i + oct * n; }
            }
        return best;
    }

    int fifthDegree (const GeneratorSettings& gs)
    {
        const auto& iv = scaleOf (gs).intervals;
        for (size_t i = 0; i < iv.size(); ++i)
            if (iv[i] == 7) return (int) i;
        return (int) iv.size() / 2;
    }

    int walk (Rng& rng, int degree, const GeneratorSettings& gs)
    {
        const int n = (int) scaleOf (gs).intervals.size();
        const int delta = rng.weighted<int> ({ { -3, 0.4f }, { -2, 1.0f }, { -1, 3.0f }, { 0, 1.6f },
                                               { 1, 3.0f }, { 2, 1.0f }, { 3, 0.4f }, { n, 0.35f }, { -n, 0.2f } });
        const int lo = 0, hi = n * gs.range;
        int d = degree + delta;
        if (d < lo) d = lo + (lo - d) % (n + 1);
        if (d > hi) d = hi - (d - hi) % (n + 1);
        return std::clamp (d, lo, hi);
    }

    void applyOctaveFlag (Rng& rng, Step& s, float prob)
    {
        if (rng.chance (prob))
            s.octave = (int8_t) std::clamp (s.octave + (rng.chance (0.7f) ? 1 : -1), Pattern::minOctave, Pattern::maxOctave);
    }

    /** Copies locked steps of `from` into `into`. */
    void restoreLocked (const Pattern& from, Pattern& into)
    {
        for (int i = 0; i < Pattern::maxSteps; ++i)
            if (from.steps[(size_t) i].locked)
                into.steps[(size_t) i] = from.steps[(size_t) i];
    }

    void rollAccents (Rng& rng, Pattern& p, const GeneratorSettings& gs, const Pattern& locks)
    {
        int run = 0;
        for (int i = 0; i < p.length; ++i)
        {
            auto& s = p.steps[(size_t) i];
            if (locks.steps[(size_t) i].locked) { run = s.accent ? run + 1 : 0; continue; }
            const bool downbeat = (i % 4) == 0;
            const float prob = std::min (1.0f, gs.accent * (downbeat ? 1.5f : 0.85f));
            s.accent = s.gate == Gate::Note && run < 2 && rng.chance (prob);
            run = s.accent ? run + 1 : 0;
        }
    }

    void rollSlides (Rng& rng, Pattern& p, const GeneratorSettings& gs, const Pattern& locks)
    {
        for (int i = 0; i < p.length; ++i)
        {
            if (locks.steps[(size_t) i].locked) continue;
            auto& s = p.steps[(size_t) i];
            s.slide = s.gate != Gate::Rest && rng.chance (gs.slide);
        }
    }
}

namespace Generator
{

bool isInScale (int semitone, const GeneratorSettings& gs)
{
    const int rel = ((semitone - gs.root) % 12 + 12) % 12;
    const auto& iv = scaleOf (gs).intervals;
    return std::find (iv.begin(), iv.end(), rel) != iv.end();
}

void tidy (Pattern& p)
{
    for (int i = 0; i < p.length; ++i)
    {
        auto& s = p.steps[(size_t) i];
        if (s.gate == Gate::Rest)
        {
            s.slide = false;
            s.accent = false;
        }
        if (s.gate == Gate::Tie && p.effectiveGate (i) == Gate::Rest)
            s.gate = Gate::Rest;
    }
    for (int i = 0; i < p.length; ++i)
    {
        auto& s = p.steps[(size_t) i];
        if (s.slide && p[i + 1].gate == Gate::Rest && ! s.locked)
            s.slide = false;
    }
}

Pattern generate (const Pattern& current, const GeneratorSettings& gs, uint32_t seed)
{
    Rng rng (seed);
    Pattern p;
    p.length = current.length;

    // 1. a 4-step motif (call), then variations (response)
    constexpr int motifLen = 4;
    std::array<Step, motifLen> motif {};
    std::array<int, motifLen> motifDeg {};
    int deg = 0;
    const int fifth = fifthDegree (gs);

    for (int j = 0; j < motifLen; ++j)
    {
        float gateProb = gs.density;
        if (j == 0) gateProb = std::min (1.0f, gs.density + 0.3f);
        if (j == 3) gateProb *= 0.9f;

        auto& s = motif[(size_t) j];
        if (rng.chance (gateProb))
            s.gate = (j > 0 && motif[(size_t) j - 1].gate != Gate::Rest && rng.chance (0.08f)) ? Gate::Tie : Gate::Note;

        if (j == 0)
            deg = rng.weighted<int> ({ { 0, 0.6f }, { fifth, 0.15f }, { (int) scaleOf (gs).intervals.size(), 0.1f }, { walk (rng, 0, gs), 0.15f } });
        else
            deg = walk (rng, deg, gs);

        motifDeg[(size_t) j] = deg;
        degreeToStep (deg, gs, s);
    }

    // 2. lay out the pattern as repetitions of the motif with growing variation
    const int numBlocks = (p.length + motifLen - 1) / motifLen;
    for (int b = 0; b < numBlocks; ++b)
    {
        const bool last = b == numBlocks - 1 && numBlocks > 1;
        const float vary = b == 0 ? 0.0f : (last ? 0.5f : (b % 2 == 1 ? 0.2f : 0.3f));
        int prevDeg = motifDeg[0];

        for (int j = 0; j < motifLen; ++j)
        {
            const int i = b * motifLen + j;
            if (i >= p.length) break;

            Step s = motif[(size_t) j];
            int d = motifDeg[(size_t) j];

            if (rng.chance (vary))
            {
                if (rng.chance (0.65f))
                {
                    d = walk (rng, j == 0 ? prevDeg : d, gs);
                    degreeToStep (d, gs, s);
                    if (s.gate == Gate::Rest && rng.chance (gs.density)) s.gate = Gate::Note;
                }
                else
                {
                    s.gate = rng.chance (gs.density) ? Gate::Note : Gate::Rest;
                }
            }
            // first step of the pattern always lands on the root for a strong "one"
            if (i == 0 && s.gate != Gate::Rest)
            {
                degreeToStep (0, gs, s);
                s.gate = Gate::Note;
            }
            prevDeg = d;
            p.steps[(size_t) i] = s;
        }
    }

    // 3. octave flags, accents, slides
    for (int i = 0; i < p.length; ++i)
        if (p.steps[(size_t) i].gate == Gate::Note && i != 0)
            applyOctaveFlag (rng, p.steps[(size_t) i], gs.octave);

    tidy (p);
    rollAccents (rng, p, gs, current);
    rollSlides (rng, p, gs, current);

    restoreLocked (current, p);
    tidy (p);
    return p;
}

Pattern mutate (const Pattern& current, const GeneratorSettings& gs, uint32_t seed)
{
    Rng rng (seed);
    Pattern p = current;

    for (int i = 0; i < p.length; ++i)
    {
        auto& s = p.steps[(size_t) i];
        if (s.locked || ! rng.chance (gs.mutateAmount))
            continue;

        const int action = rng.weighted<int> ({ { 0, 0.4f }, { 1, 0.2f }, { 2, 0.15f }, { 3, 0.15f }, { 4, 0.1f } });
        switch (action)
        {
            case 0: // re-pitch within scale
            {
                const auto oct = s.octave;
                Step t = s;
                t.octave = 0;
                degreeToStep (walk (rng, stepToDegree (t, gs), gs), gs, t);
                s.semitone = t.semitone;
                s.octave = (int8_t) std::clamp (oct + t.octave, Pattern::minOctave, Pattern::maxOctave);
                if (s.gate == Gate::Rest) s.gate = Gate::Note;
                break;
            }
            case 1: s.gate = s.gate == Gate::Rest ? Gate::Note : Gate::Rest; break;
            case 2: s.accent = ! s.accent && s.gate == Gate::Note; break;
            case 3: s.slide = ! s.slide; break;
            default:
                s.octave = (int8_t) (s.octave != 0 ? 0 : (rng.chance (0.7f) ? 1 : -1));
                break;
        }
    }
    tidy (p);
    return p;
}

Pattern randomizeAccents (const Pattern& current, const GeneratorSettings& gs, uint32_t seed)
{
    Rng rng (seed);
    Pattern p = current;
    rollAccents (rng, p, gs, current);
    return p;
}

Pattern randomizeSlides (const Pattern& current, const GeneratorSettings& gs, uint32_t seed)
{
    Rng rng (seed);
    Pattern p = current;
    rollSlides (rng, p, gs, current);
    tidy (p);
    return p;
}

Pattern randomizeOctaves (const Pattern& current, const GeneratorSettings& gs, uint32_t seed)
{
    Rng rng (seed);
    Pattern p = current;
    for (int i = 0; i < p.length; ++i)
    {
        auto& s = p.steps[(size_t) i];
        if (s.locked) continue;
        s.octave = 0;
        if (s.gate == Gate::Note)
            applyOctaveFlag (rng, s, gs.octave);
    }
    return p;
}

Pattern shift (const Pattern& current, int direction)
{
    Pattern p = current;
    for (int i = 0; i < p.length; ++i)
        p.steps[(size_t) p.wrap (i + direction)] = current.steps[(size_t) i];
    return p;
}

Pattern reverse (const Pattern& current)
{
    Pattern p = current;
    const int n = p.length;
    for (int i = 0; i < n; ++i)
    {
        p.steps[(size_t) (n - 1 - i)] = current.steps[(size_t) i];
        p.steps[(size_t) (n - 1 - i)].slide = false;
    }
    // a slide from i → i+1 becomes a slide from (n-2-i) → (n-1-i)
    for (int i = 0; i < n; ++i)
        if (current.steps[(size_t) i].slide)
            p[n - 2 - i].slide = true;
    tidy (p);
    return p;
}

} // namespace Generator
} // namespace acid
