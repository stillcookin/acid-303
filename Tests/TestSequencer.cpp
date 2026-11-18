#include <catch2/catch_test_macros.hpp>

#include "seq/Sequencer.h"

#include <vector>

using namespace acid;

namespace
{
    Pattern allNotes (int len = 16)
    {
        Pattern p;
        p.length = len;
        for (int i = 0; i < len; ++i)
        {
            p.steps[(size_t) i].gate = Gate::Note;
            p.steps[(size_t) i].semitone = (int8_t) (i % 12);
        }
        return p;
    }

    std::vector<SeqEvent> run (Sequencer& s, const Pattern& p, double ppq, int numSamples,
                               double bpm = 120.0, double sr = 48000.0)
    {
        std::vector<SeqEvent> ev (512);
        TransportInfo t { true, ppq, bpm, sr, numSamples };
        const int n = s.process (p, t, ev.data(), (int) ev.size());
        ev.resize ((size_t) n);
        return ev;
    }

    std::vector<SeqEvent> ofType (const std::vector<SeqEvent>& ev, SeqEvent::Type t)
    {
        std::vector<SeqEvent> out;
        for (auto& e : ev) if (e.type == t) out.push_back (e);
        return out;
    }
}

TEST_CASE ("Notes land exactly on 16th-note boundaries", "[seq]")
{
    // 120 BPM @ 48 kHz: one quarter = 24000 samples, one 16th = 6000
    Sequencer s;
    s.setGate (0.5f);
    const auto ev = run (s, allNotes(), 0.0, 24000);

    const auto ons = ofType (ev, SeqEvent::Type::NoteOn);
    const auto offs = ofType (ev, SeqEvent::Type::NoteOff);
    REQUIRE (ons.size() == 4);
    REQUIRE (offs.size() == 4);
    for (int i = 0; i < 4; ++i)
    {
        CHECK (ons[(size_t) i].sampleOffset == i * 6000);
        CHECK (ons[(size_t) i].step == i);
        CHECK (ons[(size_t) i].note == Pattern::baseNote + i);
        CHECK (offs[(size_t) i].sampleOffset == i * 6000 + 3000);
    }
}

TEST_CASE ("Events are independent of block size", "[seq]")
{
    auto p = allNotes();
    p.steps[3].slide = true;
    p.steps[6].gate = Gate::Rest;
    p.steps[9].gate = Gate::Tie;

    Sequencer big;
    const auto ref = run (big, p, 0.0, 24000 * 4);

    Sequencer small;
    std::vector<std::pair<int, int>> got; // (absolute sample, type)
    const int block = 441;
    for (int pos = 0; pos < 24000 * 4; pos += block)
    {
        const int n = std::min (block, 24000 * 4 - pos);
        const double ppq = pos / 24000.0;
        for (auto& e : run (small, p, ppq, n))
            got.emplace_back (pos + e.sampleOffset, (int) e.type);
    }

    REQUIRE (got.size() == ref.size());
    for (size_t i = 0; i < ref.size(); ++i)
    {
        CHECK (std::abs (got[i].first - ref[i].sampleOffset) <= 1);
        CHECK (got[i].second == (int) ref[i].type);
    }
}

TEST_CASE ("Slide holds the gate and flags the next note", "[seq]")
{
    auto p = allNotes();
    p.steps[0].slide = true;

    Sequencer s;
    const auto ev = run (s, p, 0.0, 12000); // steps 0 and 1
    const auto ons = ofType (ev, SeqEvent::Type::NoteOn);
    const auto offs = ofType (ev, SeqEvent::Type::NoteOff);

    REQUIRE (ons.size() == 2);
    CHECK_FALSE (ons[0].slide);
    CHECK (ons[1].slide);
    REQUIRE (offs.size() == 1);   // only step 1 gates off
    CHECK (offs[0].step == 1);
}

TEST_CASE ("Ties extend the previous note and rests gate off", "[seq]")
{
    Pattern p;
    p.length = 4;
    p.steps[0].gate = Gate::Note;
    p.steps[1].gate = Gate::Tie;
    p.steps[2].gate = Gate::Rest;
    p.steps[3].gate = Gate::Note;

    Sequencer s;
    const auto ev = run (s, p, 0.0, 24000);
    const auto ons = ofType (ev, SeqEvent::Type::NoteOn);
    REQUIRE (ons.size() == 2);
    CHECK (ons[0].step == 0);
    CHECK (ons[1].step == 3);

    // no gate-off during step 0 (tied), one during step 1, one at the rest's start, one during step 3
    for (auto& e : ofType (ev, SeqEvent::Type::NoteOff))
        CHECK (e.step != 0);
}

TEST_CASE ("Swing delays odd steps", "[seq]")
{
    Sequencer s;
    s.setSwing (1.0f); // 75% swing: odd 16ths start half a step late
    const auto ons = ofType (run (s, allNotes(), 0.0, 24000), SeqEvent::Type::NoteOn);
    REQUIRE (ons.size() == 4);
    CHECK (ons[0].sampleOffset == 0);
    CHECK (ons[1].sampleOffset == 6000 + 3000);
    CHECK (ons[2].sampleOffset == 12000);
}

TEST_CASE ("Transport stop sends a note off; negative ppq works", "[seq]")
{
    Sequencer s;
    run (s, allNotes(), -1.0, 24000);
    std::vector<SeqEvent> ev (8);
    TransportInfo stopped { false, 0.0, 120.0, 48000.0, 512 };
    REQUIRE (s.process (allNotes(), stopped, ev.data(), 8) == 1);
    CHECK (ev[0].type == SeqEvent::Type::NoteOff);
    REQUIRE (s.process (allNotes(), stopped, ev.data(), 8) == 0);

    // count-in: ppq -1 .. 0 plays the last 4 steps of the pattern
    Sequencer s2;
    const auto ons = ofType (run (s2, allNotes(), -1.0, 24000), SeqEvent::Type::NoteOn);
    REQUIRE (ons.size() == 4);
    CHECK (ons[0].step == 12);
}

TEST_CASE ("Pattern length wraps", "[seq]")
{
    Sequencer s;
    const auto ons = ofType (run (s, allNotes (3), 0.0, 24000), SeqEvent::Type::NoteOn);
    REQUIRE (ons.size() == 4);
    CHECK (ons[3].step == 0);
    CHECK (s.stepAt (allNotes (3), 0.25 * 4) == 1);
}

TEST_CASE ("Transpose shifts notes", "[seq]")
{
    Sequencer s;
    s.setTranspose (5);
    const auto ons = ofType (run (s, allNotes(), 0.0, 6000), SeqEvent::Type::NoteOn);
    REQUIRE (ons.size() == 1);
    CHECK (ons[0].note == Pattern::baseNote + 5);
}
