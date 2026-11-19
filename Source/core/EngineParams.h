#pragma once

#include "ParamSpec.h"
#include "../seq/World.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace acid
{

/** Plain snapshot of all parameter values, handed to the Engine once per block.
    Built from raw (denormalised) parameter values in Param order. */
struct EngineParams
{
    // voice
    float tune = 0, cutoff = 0.35f, resonance = 0.65f, envMod = 0.5f, decay = 0.4f, accent = 0.6f, drift = 0.15f;
    int waveform = 0;
    float volumeDb = -6.0f;
    // sequencer
    PlayMode mode = PlayMode::Sequencer;
    float swing = 0.0f, gate = 0.55f;
    // drive
    bool driveOn = true;
    float drive = 0.3f, driveTone = 0.6f, driveMix = 1.0f;
    // delay
    bool delayOn = false, delayPingPong = true;
    int delayTime = 7;
    float delayFeedback = 0.4f, delayTone = 0.5f, delayMix = 0.25f;
    // reverb
    bool reverbOn = false;
    float reverbSize = 0.5f, reverbDamp = 0.5f, reverbMix = 0.2f;
    // world
    World world = World::Classic;
    WorldQuantize worldQuantize = WorldQuantize::Bar;
    bool worldMidi = true;
    bool worldAlt = false;   ///< the world's alternative chain: HOT / DEEP SPACE / MOLTEN CORE

    static EngineParams defaults()
    {
        std::array<float, numParams> v {};
        for (const auto& s : paramSpecs())
            v[(size_t) static_cast<int> (s.param)] = s.defaultValue;
        return fromRaw (v);
    }

    static EngineParams fromRaw (const std::array<float, numParams>& v)
    {
        auto f = [&] (Param p) { return v[(size_t) static_cast<int> (p)]; };
        auto i = [&] (Param p) { return (int) std::lround (f (p)); };
        auto b = [&] (Param p) { return f (p) >= 0.5f; };

        EngineParams e;
        e.tune = f (Param::Tune);
        e.waveform = i (Param::Waveform);
        e.cutoff = f (Param::Cutoff);
        e.resonance = f (Param::Resonance);
        e.envMod = f (Param::EnvMod);
        e.decay = f (Param::Decay);
        e.accent = f (Param::Accent);
        e.drift = f (Param::Drift);
        e.volumeDb = f (Param::Volume);
        e.mode = static_cast<PlayMode> (i (Param::Mode));
        e.swing = f (Param::Swing);
        e.gate = f (Param::Gate);
        e.driveOn = b (Param::DriveOn);
        e.drive = f (Param::Drive);
        e.driveTone = f (Param::DriveTone);
        e.driveMix = f (Param::DriveMix);
        e.delayOn = b (Param::DelayOn);
        e.delayTime = i (Param::DelayTime);
        e.delayFeedback = f (Param::DelayFeedback);
        e.delayTone = f (Param::DelayTone);
        e.delayMix = f (Param::DelayMix);
        e.delayPingPong = b (Param::DelayPingPong);
        e.reverbOn = b (Param::ReverbOn);
        e.reverbSize = f (Param::ReverbSize);
        e.reverbDamp = f (Param::ReverbDamp);
        e.reverbMix = f (Param::ReverbMix);
        e.world = worldFromIndex (i (Param::World));
        e.worldQuantize = static_cast<WorldQuantize> (std::clamp (i (Param::WorldQuantize), 0, 2));
        e.worldMidi = b (Param::WorldMidi);
        e.worldAlt = b (Param::WorldAlt);
        return e;
    }
};

} // namespace acid
