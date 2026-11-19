#include <catch2/catch_test_macros.hpp>

#include "seq/PatternController.h"

using namespace acid;

namespace
{
    struct Counter : PatternController::Listener
    {
        int patterns = 0, settings = 0;
        void patternChanged() override { ++patterns; }
        void generatorSettingsChanged() override { ++settings; }
    };
}

TEST_CASE ("Edits publish to the audio mailbox", "[controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);
    Pattern audio;
    mb.pull (audio);
    REQUIRE (audio == pc.pattern());

    pc.toggleAccent (3);
    mb.pull (audio);
    REQUIRE (audio == pc.pattern());
    REQUIRE (audio.steps[3].accent == pc.pattern().steps[3].accent);
}

TEST_CASE ("toggleNote sets then rests; listeners fire", "[controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);
    Counter c;
    pc.addListener (&c);

    pc.clearPattern();
    pc.toggleNote (2, 7);
    REQUIRE (pc.pattern().steps[2].gate == Gate::Note);
    REQUIRE (pc.pattern().steps[2].semitone == 7);
    pc.toggleNote (2, 7);
    REQUIRE (pc.pattern().steps[2].gate == Gate::Rest);
    REQUIRE (c.patterns == 3);
    pc.removeListener (&c);
}

TEST_CASE ("Undo / redo and gestures", "[controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);
    const auto original = pc.pattern();

    pc.generate();
    const auto generated = pc.pattern();
    REQUIRE_FALSE (generated == original);

    pc.undo();
    REQUIRE (pc.pattern() == original);
    pc.redo();
    REQUIRE (pc.pattern() == generated);

    // a gesture of several edits is one undo step
    pc.beginGesture();
    pc.setNote (0, 1);
    pc.setNote (1, 2);
    pc.setNote (2, 3);
    pc.endGesture();
    pc.undo();
    REQUIRE (pc.pattern() == generated);
}

TEST_CASE ("Slots are independent and state round-trips", "[controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);

    pc.selectSlot (4);
    pc.setLength (11);
    pc.toggleLock (0);
    auto gs = pc.generatorSettings();
    gs.scale = 3;
    gs.root = 9;
    gs.density = 0.33f;
    pc.setGeneratorSettings (gs);
    pc.generateWithSeed (1234);

    const auto tree = pc.toValueTree();

    PatternMailbox mb2;
    PatternController restored (mb2);
    restored.fromValueTree (tree);

    REQUIRE (restored.currentSlot() == 4);
    for (int s = 0; s < PatternBank::numSlots; ++s)
        REQUIRE (restored.patternInSlot (s) == pc.patternInSlot (s));
    REQUIRE (restored.generatorSettings() == pc.generatorSettings());
    REQUIRE (restored.pattern().length == 11);

    // survives XML too (what the plugin actually stores)
    auto xml = tree.createXml();
    PatternMailbox mb3;
    PatternController fromXml (mb3);
    fromXml.fromValueTree (juce::ValueTree::fromXml (*xml));
    REQUIRE (fromXml.pattern() == pc.pattern());
}

TEST_CASE ("Duplicate doubles the pattern with one undo step", "[controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);
    pc.setLength (8);
    const auto before = pc.pattern();
    pc.duplicate();
    REQUIRE (pc.pattern().length == 16);
    for (int i = 0; i < 8; ++i)
    {
        REQUIRE (pc.pattern().steps[(size_t) i] == before.steps[(size_t) i]);
        REQUIRE (pc.pattern().steps[(size_t) (i + 8)] == before.steps[(size_t) i]);
    }
    pc.duplicate();
    REQUIRE (pc.pattern().length == 32);
    pc.duplicate(); // 64 would not fit: unchanged
    REQUIRE (pc.pattern().length == 32);
    pc.undo();
    REQUIRE (pc.pattern().length == 16);
    pc.undo();
    REQUIRE (pc.pattern() == before);
}

TEST_CASE (":2 spreads notes out and x2 squeezes them back", "[controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);
    Pattern p;
    p.length = 4;
    p.steps[0] = Step { 0, 0, Gate::Note, true, false, false };
    p.steps[1] = Step { 3, 0, Gate::Note, false, true, false };  // slides into step 2
    p.steps[2] = Step { 7, 0, Gate::Note, false, false, false };
    p.steps[3] = Step {};                                         // rest
    pc.setPattern (p);

    pc.halfSpeed();
    const auto& h = pc.pattern();
    REQUIRE (h.length == 8);
    REQUIRE (h.steps[0].gate == Gate::Note);
    REQUIRE (h.steps[0].accent);
    REQUIRE (h.steps[1].gate == Gate::Tie);   // the note holds twice as long
    REQUIRE (! h.steps[1].accent);
    REQUIRE (h.steps[2].semitone == 3);
    REQUIRE (! h.steps[2].slide);
    REQUIRE (h.steps[3].gate == Gate::Tie);
    REQUIRE (h.steps[3].slide);               // still glides into the next note
    REQUIRE (h.steps[4].semitone == 7);
    REQUIRE (h.steps[6].gate == Gate::Rest);
    REQUIRE (h.steps[7].gate == Gate::Rest);

    pc.doubleSpeed();                         // round trip
    REQUIRE (pc.pattern() == p);

    pc.doubleSpeed();
    REQUIRE (pc.pattern().length == 2);
    REQUIRE (pc.pattern().steps[1].semitone == 7);
    pc.undo();
    REQUIRE (pc.pattern() == p);

    pc.setLength (32);
    pc.halfSpeed();                           // 64 would not fit: unchanged
    REQUIRE (pc.pattern().length == 32);
}
