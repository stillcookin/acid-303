#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include "seq/Generator.h"
#include "seq/MidiExport.h"

using namespace acid;

TEST_CASE ("Generation is deterministic per seed", "[gen]")
{
    GeneratorSettings gs;
    Pattern base;
    REQUIRE (Generator::generate (base, gs, 42) == Generator::generate (base, gs, 42));
    REQUIRE_FALSE (Generator::generate (base, gs, 42) == Generator::generate (base, gs, 43));
}

TEST_CASE ("Generated notes stay in the scale and obey structural rules", "[gen]")
{
    const int scale = GENERATE (Catch::Generators::range (0, 8));
    const int root = GENERATE (0, 5, 11);

    GeneratorSettings gs;
    gs.scale = scale;
    gs.root = root;
    gs.slide = 0.6f;
    gs.accent = 0.7f;
    gs.octave = 0.5f;
    gs.range = 2;

    for (uint32_t seed = 1; seed < 60; ++seed)
    {
        Pattern base;
        base.length = (int) (seed % 2 == 0 ? 16 : 13);
        const auto p = Generator::generate (base, gs, seed);
        REQUIRE (p.length == base.length);

        int sounding = 0, accentRun = 0;
        for (int i = 0; i < p.length; ++i)
        {
            const auto& s = p.steps[(size_t) i];
            if (s.gate == Gate::Note)
            {
                ++sounding;
                REQUIRE (Generator::isInScale (s.semitone, gs));
            }
            REQUIRE (s.octave >= Pattern::minOctave);
            REQUIRE (s.octave <= Pattern::maxOctave);
            if (s.slide)
                REQUIRE (p[i + 1].gate != Gate::Rest); // never slide into a rest
            if (s.gate == Gate::Tie)
                REQUIRE (p.effectiveGate (i) == Gate::Tie); // no orphan ties
            accentRun = s.accent ? accentRun + 1 : 0;
            REQUIRE (accentRun <= 2);
        }
        REQUIRE (sounding > 0);
    }
}

TEST_CASE ("Density controls how many steps sound", "[gen]")
{
    GeneratorSettings sparse, dense;
    sparse.density = 0.2f;
    dense.density = 1.0f;
    int a = 0, b = 0;
    for (uint32_t seed = 0; seed < 50; ++seed)
    {
        for (const auto& s : Generator::generate ({}, sparse, seed).steps) a += s.gate != Gate::Rest;
        for (const auto& s : Generator::generate ({}, dense, seed).steps) b += s.gate != Gate::Rest;
    }
    REQUIRE (b > a * 2);
}

TEST_CASE ("Locked steps survive generate and mutate", "[gen]")
{
    GeneratorSettings gs;
    gs.mutateAmount = 1.0f;
    Pattern base;
    base.steps[5] = { 11, 1, Gate::Note, true, false, true };
    base.steps[6] = { 0, 0, Gate::Note, false, false, false };

    for (uint32_t seed = 0; seed < 30; ++seed)
    {
        REQUIRE (Generator::generate (base, gs, seed).steps[5] == base.steps[5]);
        REQUIRE (Generator::mutate (base, gs, seed).steps[5] == base.steps[5]);
        REQUIRE (Generator::randomizeAccents (base, gs, seed).steps[5].accent);
    }
}

TEST_CASE ("Shift rotates and reverse mirrors", "[gen]")
{
    Pattern p;
    p.length = 4;
    for (int i = 0; i < 4; ++i) { p.steps[(size_t) i].gate = Gate::Note; p.steps[(size_t) i].semitone = (int8_t) i; }
    p.steps[0].slide = true; // slide 0 → 1

    auto r = Generator::shift (p, 1);
    CHECK (r.steps[1].semitone == 0);
    CHECK (r.steps[0].semitone == 3);
    CHECK (Generator::shift (r, -1) == p);

    auto rev = Generator::reverse (p);
    CHECK (rev.steps[0].semitone == 3);
    CHECK (rev.steps[3].semitone == 0);
    // original slide 0→1 is now 2→3
    CHECK (rev.steps[2].slide);
    CHECK_FALSE (rev.steps[3].slide);
}

TEST_CASE ("MIDI export writes one note per sounding note and overlaps slides", "[midi]")
{
    Pattern p;
    p.length = 4;
    p.steps[0] = { 0, 0, Gate::Note, true, true, false };   // slides into 1
    p.steps[1] = { 7, 0, Gate::Note, false, false, false };
    p.steps[2] = { 0, 0, Gate::Rest, false, false, false };
    p.steps[3] = { 3, 1, Gate::Note, false, false, false };

    auto mf = MidiExport::toMidiFile (p, 2);
    const auto* track = mf.getTrack (0);
    REQUIRE (track != nullptr);

    std::vector<const juce::MidiMessageSequence::MidiEventHolder*> ons;
    for (auto* e : *track)
        if (e->message.isNoteOn()) ons.push_back (e);

    REQUIRE (ons.size() == 6);
    CHECK (ons[0]->message.getVelocity() == 127);
    CHECK (ons[0]->message.getNoteNumber() == Pattern::baseNote);
    CHECK (ons[2]->message.getNoteNumber() == Pattern::baseNote + 15);

    // the slid note's off comes after the next note's on (legato overlap)
    const double offTime = ons[0]->noteOffObject->message.getTimeStamp();
    CHECK (offTime > ons[1]->message.getTimeStamp());
    // a normal note ends before the next step
    CHECK (ons[1]->noteOffObject->message.getTimeStamp() < 2 * MidiExport::ticksPerStep);
}
