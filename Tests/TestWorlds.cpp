#include <catch2/catch_test_macros.hpp>

#include "core/Engine.h"
#include "seq/Generator.h"

#include <cmath>

using namespace acid;

namespace
{
    GeneratorSettings settingsFor (World w, int root = 0)
    {
        GeneratorSettings gs;
        gs.world = w;
        gs.root = root;
        gs.scale = gs.worldScale[(size_t) indexOf (w)];
        return gs;
    }

    Pattern chromaticRun()
    {
        Pattern p;
        p.length = 16;
        for (int i = 0; i < 13; ++i)
        {
            p.steps[(size_t) i].gate = Gate::Note;
            p.steps[(size_t) i].semitone = (int8_t) i;
        }
        return p;
    }

    struct Rig
    {
        PatternMailbox mailbox;
        LiveState live;
        PatternController patterns { mailbox };
        Engine engine { mailbox, live };
        std::array<float, numParams> raw {};

        Rig()
        {
            for (const auto& s : paramSpecs())
                raw[(size_t) static_cast<int> (s.param)] = s.defaultValue;
            engine.prepare (48000.0, 512);
        }

        void set (Param p, float v) { raw[(size_t) static_cast<int> (p)] = v; }

        /** Renders `blocks` blocks of 512; returns the peak. */
        float run (int blocks, std::optional<TransportInfo> t, double& ppq, const juce::MidiBuffer& firstMidi = {})
        {
            juce::AudioBuffer<float> buf (2, 512);
            float peak = 0.0f;
            for (int b = 0; b < blocks; ++b)
            {
                engine.setParams (EngineParams::fromRaw (raw));
                buf.clear();
                std::optional<TransportInfo> tt;
                if (t)
                {
                    tt = *t;
                    tt->ppqPosition = ppq;
                    ppq += 512.0 / 48000.0 * (t->bpm / 60.0);
                }
                engine.process (buf, b == 0 ? firstMidi : juce::MidiBuffer(), tt);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                    {
                        REQUIRE (std::isfinite (buf.getSample (c, i)));
                        peak = std::max (peak, std::abs (buf.getSample (c, i)));
                    }
            }
            return peak;
        }
    };

    juce::MidiBuffer single (const juce::MidiMessage& m)
    {
        juce::MidiBuffer b;
        b.addEvent (m, 0);
        return b;
    }
}

// ------------------------------------------------------------------ scales & retune

TEST_CASE ("Each world offers its own scales, and the scale table only grew", "[worlds]")
{
    REQUIRE (std::string (scales()[0].name) == "Minor");
    REQUIRE (std::string (scales()[7].name) == "Chromatic");
    REQUIRE (std::string (scales()[(size_t) worldScales (World::Space).front()].name) == "Lydian");
    REQUIRE (std::string (scales()[(size_t) worldScales (World::Underground).front()].name) == "Harmonic Minor");
    for (int w = 0; w < numWorlds; ++w)
        for (int idx : worldScales (worldFromIndex (w)))
            REQUIRE (idx < (int) scales().size());
}

TEST_CASE ("Retune moves only out-of-scale notes, to the nearest scale note", "[worlds]")
{
    const auto gs = settingsFor (World::Space); // C Lydian: C D E F# G A B
    const auto src = chromaticRun();
    const auto out = Generator::retune (src, gs);

    for (int i = 0; i < 13; ++i)
    {
        const auto& s = out.steps[(size_t) i];
        REQUIRE (Generator::isInScale (s.semitone, gs));
        REQUIRE (std::abs ((s.semitone + 12 * s.octave) - i) <= 1);
        if (Generator::isInScale (i, gs))
            REQUIRE (s == src.steps[(size_t) i]); // untouched
    }
    REQUIRE (out.steps[1].semitone == 0); // C# → C (ties go down)
}

TEST_CASE ("Retune keeps locked steps, rests, accents and slides", "[worlds]")
{
    auto src = chromaticRun();
    src.steps[1].locked = true;   // C#, out of Harmonic Minor
    src.steps[3].accent = true;   // D#: in scale
    src.steps[6].slide = true;    // F#: out of scale, moves but keeps its slide
    const auto out = Generator::retune (src, settingsFor (World::Underground));

    REQUIRE (out.steps[1] == src.steps[1]);
    REQUIRE (out.steps[3].accent);
    REQUIRE (out.steps[6].slide);
    REQUIRE (out.steps[14].gate == Gate::Rest);
}

// ------------------------------------------------------------------ controller

TEST_CASE ("Switching world retunes what plays; Classic restores the original", "[worlds][controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);
    pc.setPattern (chromaticRun());
    const auto original = pc.pattern();

    pc.setWorld (World::Space);
    REQUIRE (pc.world() == World::Space);
    REQUIRE (pc.sourcePattern() == original);          // the written pattern is untouched
    REQUIRE (pc.pattern() != original);                // what plays is retuned
    for (int i = 0; i < 13; ++i)
        REQUIRE (Generator::isInScale (pc.pattern().steps[(size_t) i].semitone, pc.generatorSettings()));

    pc.setWorld (World::Classic);
    REQUIRE (pc.pattern() == original);
}

TEST_CASE ("The mailbox carries the pattern for every world", "[worlds][controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);
    pc.setPattern (chromaticRun());

    WorldPatterns audio {};
    mb.pull (audio);
    REQUIRE (audio[0] == pc.patternForWorld (World::Classic));
    REQUIRE (audio[1] == pc.patternForWorld (World::Space));
    REQUIRE (audio[2] == pc.patternForWorld (World::Underground));
    REQUIRE (audio[1] != audio[0]);
}

TEST_CASE ("Each world remembers its scale; edits land on the source", "[worlds][controller]")
{
    PatternMailbox mb;
    PatternController pc (mb);

    pc.setWorld (World::Space);
    auto gs = pc.generatorSettings();
    gs.scale = 9; // Whole Tone
    pc.setGeneratorSettings (gs);

    pc.setWorld (World::Underground);
    REQUIRE (pc.generatorSettings().scale == 3);
    pc.setWorld (World::Space);
    REQUIRE (pc.generatorSettings().scale == 9);

    // clicking the note that is shown rests it, even if the source note differs
    pc.setPattern (chromaticRun());
    const int shown = pc.pattern().steps[1].semitone; // C# shows as C
    pc.toggleNote (1, shown);
    REQUIRE (pc.sourcePattern().steps[1].gate == Gate::Rest);

    // state round-trip keeps the per-world scales
    PatternMailbox mb2;
    PatternController pc2 (mb2);
    pc2.fromValueTree (pc.toValueTree());
    pc2.setWorld (World::Space);
    REQUIRE (pc2.generatorSettings().scale == 9);
}

// ------------------------------------------------------------------ generator phrasing

TEST_CASE ("Space phrasing: in scale, open ending, fewer accents", "[worlds][generator]")
{
    const auto gs = settingsFor (World::Space);
    auto classic = gs;
    classic.world = World::Classic;

    int spaceAccents = 0, classicAccents = 0;
    for (uint32_t seed = 1; seed <= 200; ++seed)
    {
        const auto p = Generator::generate (Pattern {}, gs, seed);
        REQUIRE (p.steps[15].gate == Gate::Rest);
        for (int i = 0; i < p.length; ++i)
        {
            if (p.steps[(size_t) i].gate == Gate::Note)
                REQUIRE (Generator::isInScale (p.steps[(size_t) i].semitone, gs));
            spaceAccents += p.steps[(size_t) i].accent ? 1 : 0;
        }
        const auto c = Generator::generate (Pattern {}, classic, seed);
        for (int i = 0; i < c.length; ++i)
            classicAccents += c.steps[(size_t) i].accent ? 1 : 0;
    }
    REQUIRE (spaceAccents < classicAccents);
}

TEST_CASE ("Underground phrasing: root on beats 1 and 3, tension before them", "[worlds][generator]")
{
    for (int root : { 0, 5, 9 })
    {
        const auto gs = settingsFor (World::Underground, root);
        for (uint32_t seed = 1; seed <= 100; ++seed)
        {
            const auto p = Generator::generate (Pattern {}, gs, seed);
            for (int i : { 0, 8 })
            {
                REQUIRE (p.steps[(size_t) i].gate == Gate::Note);
                REQUIRE ((p.steps[(size_t) i].semitone - root + 12) % 12 == 0);
            }
            for (int i : { 7, 15 })
            {
                const auto& s = p.steps[(size_t) i];
                REQUIRE (s.gate == Gate::Note);
                REQUIRE (s.accent);
                const int rel = ((s.semitone - root) % 12 + 12) % 12;
                REQUIRE ((rel == 11 || rel == 1));
            }
            REQUIRE (p.steps[7].slide); // glides into beat 3
        }
    }
}

TEST_CASE ("World phrasing respects locked steps", "[worlds][generator]")
{
    Pattern current;
    current.steps[0] = { 6, 0, Gate::Note, false, false, true };   // locked F# on the downbeat
    current.steps[15] = { 3, 0, Gate::Note, true, false, true };   // locked last step
    for (World w : { World::Space, World::Underground })
    {
        const auto p = Generator::generate (current, settingsFor (w), 77);
        REQUIRE (p.steps[0] == current.steps[0]);
        REQUIRE (p.steps[15] == current.steps[15]);
    }
}

// ------------------------------------------------------------------ MIDI mapping

TEST_CASE ("World MIDI mapping", "[worlds][midi]")
{
    REQUIRE (WorldMidi::fromController (102, 0) == World::Classic);
    REQUIRE (WorldMidi::fromController (102, 42) == World::Classic);
    REQUIRE (WorldMidi::fromController (102, 43) == World::Space);
    REQUIRE (WorldMidi::fromController (102, 85) == World::Space);
    REQUIRE (WorldMidi::fromController (102, 127) == World::Underground);
    REQUIRE (! WorldMidi::fromController (1, 127));
    REQUIRE (WorldMidi::fromProgramChange (1) == World::Space);
    REQUIRE (! WorldMidi::fromProgramChange (3));
    REQUIRE (WorldMidi::fromKeyswitch (2) == World::Underground);
    REQUIRE (! WorldMidi::fromKeyswitch (36));
}

// ------------------------------------------------------------------ engine

TEST_CASE ("Engine adopts the World parameter and switches immediately when stopped", "[worlds][engine]")
{
    Rig rig;
    double ppq = 0.0;
    rig.run (2, std::nullopt, ppq);
    REQUIRE (rig.live.world == 0);

    rig.set (Param::World, 1.0f);
    rig.run (1, std::nullopt, ppq);
    REQUIRE (rig.live.world == 1);
    REQUIRE (rig.live.pendingWorld == -1);
}

TEST_CASE ("MIDI switches worlds: CC 102, program change, keyswitch", "[worlds][engine][midi]")
{
    Rig rig;
    double ppq = 0.0;
    rig.run (1, std::nullopt, ppq);

    const auto serial0 = rig.live.midiWorldSerial.load();
    rig.run (1, std::nullopt, ppq, single (juce::MidiMessage::controllerEvent (1, 102, 100)));
    REQUIRE (rig.live.world == 2);
    REQUIRE (rig.live.midiWorldRequest == 2);
    REQUIRE (rig.live.midiWorldSerial.load() != serial0);

    rig.run (1, std::nullopt, ppq, single (juce::MidiMessage::programChange (1, 1)));
    REQUIRE (rig.live.world == 1);

    // keyswitch in Seq + Transpose: switches, but does not transpose
    rig.set (Param::Mode, 1.0f);
    rig.run (1, std::nullopt, ppq, single (juce::MidiMessage::noteOn (1, 0, (juce::uint8) 100)));
    REQUIRE (rig.live.world == 0);
    REQUIRE (rig.live.transpose == 0);

    // World MIDI off: ignored
    rig.set (Param::WorldMidi, 0.0f);
    rig.run (1, std::nullopt, ppq, single (juce::MidiMessage::controllerEvent (1, 102, 127)));
    REQUIRE (rig.live.world == 0);
}

TEST_CASE ("While playing, a world change waits for the next bar", "[worlds][engine]")
{
    Rig rig;
    const TransportInfo host { true, 0.0, 120.0, 48000.0, 512 };
    double ppq = 0.5;
    rig.run (2, host, ppq);
    REQUIRE (rig.live.world == 0);

    rig.set (Param::World, 1.0f); // Space, Next Bar (default)
    rig.run (1, host, ppq);
    REQUIRE (rig.live.world == 0);
    REQUIRE (rig.live.pendingWorld == 1);

    // run until just before beat 4.0, still Classic
    while (ppq + 512.0 / 48000.0 * 2.0 < 4.0)
        rig.run (1, host, ppq);
    REQUIRE (rig.live.world == 0);

    rig.run (2, host, ppq); // crosses the bar line
    REQUIRE (rig.live.world == 1);
    REQUIRE (rig.live.pendingWorld == -1);
}

TEST_CASE ("Next Beat and Immediate quantize", "[worlds][engine]")
{
    const TransportInfo host { true, 0.0, 120.0, 48000.0, 512 };
    {
        Rig rig;
        rig.set (Param::WorldQuantize, 1.0f); // Next Beat
        double ppq = 0.1;
        rig.run (1, host, ppq);
        rig.set (Param::World, 2.0f);
        rig.run (1, host, ppq);
        REQUIRE (rig.live.world == 0);
        while (ppq < 1.05)
            rig.run (1, host, ppq);
        REQUIRE (rig.live.world == 2);
    }
    {
        Rig rig;
        rig.set (Param::WorldQuantize, 0.0f); // Immediate
        double ppq = 0.1;
        rig.run (1, host, ppq);
        rig.set (Param::World, 2.0f);
        rig.run (1, host, ppq);
        REQUIRE (rig.live.world == 2);
    }
}

TEST_CASE ("Every world renders a stable, audible voice", "[worlds][engine]")
{
    float peaks[numWorlds] {};
    for (int w = 0; w < numWorlds; ++w)
    {
        Rig rig;
        rig.set (Param::World, (float) w);
        rig.set (Param::Resonance, 1.0f);
        double ppq = 0.0;
        peaks[w] = rig.run (150, TransportInfo { true, 0.0, 130.0, 48000.0, 512 }, ppq);
        REQUIRE (peaks[w] > 0.02f);
        REQUIRE (peaks[w] < 4.0f);
    }
    REQUIRE (peaks[2] != peaks[0]); // the sub and tighter decay change the sound
}

TEST_CASE ("Every world's effects and chain toggle stay stable with everything up full", "[worlds][engine][fx]")
{
    for (int w = 0; w < numWorlds; ++w)
        for (int alt = 0; alt < 2; ++alt)
        {
            Rig rig;
            rig.set (Param::World, (float) w);
            rig.set (Param::WorldAlt, (float) alt);
            rig.set (Param::Resonance, 1.0f);
            rig.set (Param::DriveOn, 1.0f);   rig.set (Param::Drive, 1.0f);  rig.set (Param::DriveMix, 1.0f);
            rig.set (Param::DelayOn, 1.0f);   rig.set (Param::DelayFeedback, 0.95f); rig.set (Param::DelayMix, 1.0f);
            rig.set (Param::ReverbOn, 1.0f);  rig.set (Param::ReverbSize, 1.0f); rig.set (Param::ReverbDamp, 1.0f); rig.set (Param::ReverbMix, 1.0f);
            double ppq = 0.0;
            const float peak = rig.run (400, TransportInfo { true, 0.0, 128.0, 48000.0, 512 }, ppq);
            INFO ("world " << w << " alt " << alt << " peak " << peak);
            REQUIRE (peak > 0.02f);
            REQUIRE (peak < 6.0f);
        }
}

TEST_CASE ("A world switch lets the previous echoes ring out", "[worlds][engine][fx]")
{
    Rig rig;
    rig.set (Param::WorldQuantize, 0.0f); // immediate
    rig.set (Param::DelayOn, 1.0f);
    rig.set (Param::DelayFeedback, 0.8f);
    rig.set (Param::DelayMix, 1.0f);
    double ppq = 0.0;
    rig.run (200, TransportInfo { true, 0.0, 120.0, 48000.0, 512 }, ppq);

    // switch while the transport stops: no new notes, only tails can sound
    rig.set (Param::World, 1.0f);
    const float tailPeak = rig.run (20, std::nullopt, ppq);
    REQUIRE (rig.live.world == 1);
    REQUIRE (tailPeak > 0.001f);
}
