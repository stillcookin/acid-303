#include <catch2/catch_test_macros.hpp>

#include "core/Engine.h"

#include <cmath>

using namespace acid;

namespace
{
    struct Rig
    {
        PatternMailbox mailbox;
        LiveState live;
        PatternController patterns { mailbox };
        Engine engine { mailbox, live };
    };

    float renderPeak (Rig& rig, int blocks, std::optional<TransportInfo> base, const juce::MidiBuffer& firstMidi = {})
    {
        juce::AudioBuffer<float> buf (2, 512);
        float peak = 0.0f;
        double ppq = base ? base->ppqPosition : 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            std::optional<TransportInfo> t;
            if (base)
            {
                t = *base;
                t->ppqPosition = ppq;
                ppq += 512.0 / 48000.0 * (base->bpm / 60.0);
            }
            rig.engine.process (buf, b == 0 ? firstMidi : juce::MidiBuffer(), t);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                {
                    const float v = buf.getSample (c, i);
                    REQUIRE (std::isfinite (v));
                    peak = std::max (peak, std::abs (v));
                }
        }
        return peak;
    }
}

TEST_CASE ("Engine plays the pattern when the host is playing", "[engine]")
{
    Rig rig;
    rig.engine.prepare (48000.0, 512);
    const float peak = renderPeak (rig, 200, TransportInfo { true, 0.0, 125.0, 48000.0, 512 });
    REQUIRE (peak > 0.02f);
    REQUIRE (peak < 4.0f);
    REQUIRE (rig.live.running);
    REQUIRE (rig.live.playingStep >= 0);
}

TEST_CASE ("Engine is silent when stopped, plays in preview", "[engine]")
{
    Rig rig;
    rig.engine.prepare (48000.0, 512);
    REQUIRE (renderPeak (rig, 50, TransportInfo { false, 0.0, 120.0, 48000.0, 512 }) < 1.0e-5f);

    rig.live.previewRequested = true;
    REQUIRE (renderPeak (rig, 100, std::nullopt) > 0.02f);
}

TEST_CASE ("Engine MIDI mode responds to notes", "[engine]")
{
    Rig rig;
    auto p = EngineParams::defaults();
    p.mode = PlayMode::Midi;
    rig.engine.setParams (p);
    rig.engine.prepare (48000.0, 512);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 110), 10);
    REQUIRE (renderPeak (rig, 40, TransportInfo { true, 0.0, 120.0, 48000.0, 512 }, midi) > 0.02f);
    REQUIRE (rig.live.voiceNote == 40);
}

TEST_CASE ("Engine handles blocks larger than prepared and mono output", "[engine]")
{
    Rig rig;
    rig.engine.prepare (44100.0, 128);
    rig.live.previewRequested = true;

    juce::AudioBuffer<float> big (2, 2048);
    for (int i = 0; i < 20; ++i)
    {
        big.clear();
        rig.engine.process (big, {}, std::nullopt);
    }
    REQUIRE (big.getMagnitude (0, 2048) > 0.0f);

    juce::AudioBuffer<float> mono (1, 256);
    rig.engine.process (mono, {}, std::nullopt);
    REQUIRE (std::isfinite (mono.getSample (0, 100)));
}
