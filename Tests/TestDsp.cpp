#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "dsp/DiodeLadder.h"
#include "dsp/Delay.h"
#include "dsp/Oscillator.h"
#include "dsp/Overdrive.h"
#include "dsp/Voice303.h"

#include <cmath>
#include <random>

using namespace acid::dsp;

TEST_CASE ("Diode ladder stays finite and bounded at max resonance", "[dsp]")
{
    const double sr = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    const double cutoff = GENERATE (40.0, 500.0, 5000.0, 20000.0);

    DiodeLadder f;
    f.prepare (sr);
    f.setResonance (1.0f);
    f.setCutoff (cutoff);

    std::mt19937 rng (1);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    float maxAbs = 0.0f;
    for (int i = 0; i < (int) sr; ++i)
    {
        const float x = (i < 100 ? 1.0f : 0.0f) + 0.5f * noise (rng);
        const float y = f.process (x * 4.0f);
        REQUIRE (std::isfinite (y));
        maxAbs = std::max (maxAbs, std::abs (y));
    }
    REQUIRE (maxAbs < 4.0f);
}

TEST_CASE ("Diode ladder is a low-pass", "[dsp]")
{
    const double sr = 88200.0;
    DiodeLadder f;
    f.prepare (sr);
    f.setResonance (0.0f);
    f.setCutoff (500.0);

    auto rmsAt = [&] (double hz)
    {
        f.reset();
        double acc = 0.0;
        const int n = (int) sr / 2;
        for (int i = 0; i < n; ++i)
        {
            const float y = f.process (0.1f * (float) std::sin (2.0 * 3.14159265358979 * hz * i / sr));
            if (i > n / 2) acc += (double) y * y;
        }
        return std::sqrt (acc / (n / 2));
    };

    const double low = rmsAt (100.0), high = rmsAt (5000.0);
    REQUIRE (low > 10.0 * high);
}

TEST_CASE ("Max feedback is computed for unequal poles", "[dsp]")
{
    DiodeLadder f;
    // 4 equal poles would give exactly 4; the higher first pole needs more gain
    REQUIRE (f.maxFeedback() > 4.0);
    REQUIRE (f.maxFeedback() < 12.0);
}

TEST_CASE ("Oscillator is bounded for both waveforms", "[dsp]")
{
    Oscillator o;
    o.prepare (88200.0);
    for (auto w : { Oscillator::Wave::Saw, Oscillator::Wave::Square })
    {
        o.reset();
        o.setFrequency (1000.0);
        for (int i = 0; i < 88200; ++i)
        {
            const float y = o.process (w);
            REQUIRE (std::isfinite (y));
            REQUIRE (std::abs (y) < 1.5f);
        }
    }
}

TEST_CASE ("Slide glides to the target pitch without retriggering", "[dsp][voice]")
{
    const double sr = 88200.0;
    Voice303 v;
    v.prepare (sr);
    v.setParams ({});

    v.noteOn (36, false, false);
    for (int i = 0; i < 1000; ++i) v.process();
    REQUIRE (v.getCurrentPitch() == 36.0f);

    v.noteOn (48, false, true);
    // shortly after: still on the way
    for (int i = 0; i < (int) (0.01 * sr); ++i) v.process();
    REQUIRE (v.getCurrentPitch() > 36.5f);
    REQUIRE (v.getCurrentPitch() < 47.0f);

    // after ~150 ms it has arrived
    for (int i = 0; i < (int) (0.14 * sr); ++i) v.process();
    REQUIRE (std::abs (v.getCurrentPitch() - 48.0f) < 0.05f);
    REQUIRE (v.isGateOn());
}

TEST_CASE ("A non-slide note jumps pitch immediately", "[dsp][voice]")
{
    Voice303 v;
    v.prepare (88200.0);
    v.noteOn (36, false, false);
    for (int i = 0; i < 100; ++i) v.process();
    v.noteOn (50, true, false);
    REQUIRE (v.getCurrentPitch() == 50.0f);
}

TEST_CASE ("Voice output is finite and releases after note off", "[dsp][voice]")
{
    const double sr = 88200.0;
    Voice303 v;
    v.prepare (sr);
    VoiceParams p;
    p.resonance = 1.0f;
    p.envMod = 1.0f;
    p.accent = 1.0f;
    v.setParams (p);

    float peak = 0.0f;
    for (int n = 0; n < 8; ++n)
    {
        v.noteOn (36 + n * 3, true, n % 2 == 1);
        for (int i = 0; i < (int) (0.1 * sr); ++i)
        {
            const float y = v.process();
            REQUIRE (std::isfinite (y));
            peak = std::max (peak, std::abs (y));
        }
    }
    REQUIRE (peak > 0.01f);
    REQUIRE (peak < 3.0f);

    v.noteOff();
    for (int i = 0; i < (int) (0.2 * sr); ++i) v.process();
    REQUIRE_FALSE (v.isActive());
}

TEST_CASE ("Overdrive and delay produce finite output", "[dsp][fx]")
{
    Overdrive od;
    od.prepare (88200.0);
    od.setParams (1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 10000; ++i)
        REQUIRE (std::isfinite (od.process (std::sin (i * 0.01f) * 2.0f)));

    StereoDelay d;
    d.prepare (48000.0);
    d.setParams (0.25, 0.95f, 0.5f, 1.0f, true);
    std::vector<float> l (48000, 0.0f), r (48000, 0.0f);
    l[0] = r[0] = 1.0f;
    d.process (l.data(), r.data(), 48000);
    float maxAbs = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        REQUIRE (std::isfinite (l[(size_t) i]));
        maxAbs = std::max (maxAbs, std::abs (r[(size_t) i]));
    }
    REQUIRE (maxAbs > 0.1f); // ping-pong reached the right channel
}
