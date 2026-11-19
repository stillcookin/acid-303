#include "ParamSpec.h"

#include <cassert>

namespace acid
{

using K = ParamSpec::Kind;

static std::array<ParamSpec, numParams> makeSpecs()
{
    std::array<ParamSpec, numParams> s {
        ParamSpec { Param::Tune,          K::Float,  "tune",       "Tune",       "voice", -12.0f, 12.0f, 0.0f,  -1.0f, "st" },
        ParamSpec { Param::Waveform,      K::Choice, "waveform",   "Wave",       "voice", 0, 1, 0, -1, "", { "Saw", "Square" } },
        ParamSpec { Param::Cutoff,        K::Float,  "cutoff",     "Cutoff",     "voice", 0, 1, 0.35f },
        ParamSpec { Param::Resonance,     K::Float,  "resonance",  "Reso",       "voice", 0, 1, 0.65f },
        ParamSpec { Param::EnvMod,        K::Float,  "envmod",     "Env Mod",    "voice", 0, 1, 0.5f },
        ParamSpec { Param::Decay,         K::Float,  "decay",      "Decay",      "voice", 0, 1, 0.4f },
        ParamSpec { Param::Accent,        K::Float,  "accent",     "Accent",     "voice", 0, 1, 0.6f },
        ParamSpec { Param::Drift,         K::Float,  "drift",      "Age",        "voice", 0, 1, 0.15f },
        ParamSpec { Param::Volume,        K::Float,  "volume",     "Volume",     "voice", -36.0f, 6.0f, -6.0f, -1.0f, "dB" },

        ParamSpec { Param::Mode,          K::Choice, "mode",       "Mode",       "seq", 0, 2, 0, -1, "", { "Sequencer", "Seq + Transpose", "MIDI" } },
        ParamSpec { Param::Swing,         K::Float,  "swing",      "Swing",      "seq", 0, 1, 0.0f },
        ParamSpec { Param::Gate,          K::Float,  "gate",       "Gate",       "seq", 0.1f, 1.0f, 0.55f },

        ParamSpec { Param::DriveOn,       K::Bool,   "drive_on",   "Drive On",   "drive", 0, 1, 1 },
        ParamSpec { Param::Drive,         K::Float,  "drive",      "Drive",      "drive", 0, 1, 0.3f },
        ParamSpec { Param::DriveTone,     K::Float,  "drive_tone", "Tone",       "drive", 0, 1, 0.6f },
        ParamSpec { Param::DriveMix,      K::Float,  "drive_mix",  "Mix",        "drive", 0, 1, 1.0f },

        ParamSpec { Param::DelayOn,       K::Bool,   "delay_on",   "Delay On",   "delay", 0, 1, 0 },
        ParamSpec { Param::DelayTime,     K::Choice, "delay_time", "Time",       "delay", 0, 10, 7, -1, "",
                    { "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/4D", "1/2" } },
        ParamSpec { Param::DelayFeedback, K::Float,  "delay_fb",   "Feedback",   "delay", 0, 0.95f, 0.4f },
        ParamSpec { Param::DelayTone,     K::Float,  "delay_tone", "Tone",       "delay", 0, 1, 0.5f },
        ParamSpec { Param::DelayMix,      K::Float,  "delay_mix",  "Mix",        "delay", 0, 1, 0.25f },
        ParamSpec { Param::DelayPingPong, K::Bool,   "delay_pp",   "Ping-Pong",  "delay", 0, 1, 1 },

        ParamSpec { Param::ReverbOn,      K::Bool,   "reverb_on",  "Reverb On",  "reverb", 0, 1, 0 },
        ParamSpec { Param::ReverbSize,    K::Float,  "reverb_size","Size",       "reverb", 0, 1, 0.5f },
        ParamSpec { Param::ReverbDamp,    K::Float,  "reverb_damp","Damp",       "reverb", 0, 1, 0.5f },
        ParamSpec { Param::ReverbMix,     K::Float,  "reverb_mix", "Mix",        "reverb", 0, 1, 0.2f },

        ParamSpec { Param::World,         K::Choice, "world",      "World",      "world", 0, 2, 0, -1, "", { "Classic", "Space", "Underground" } },
        ParamSpec { Param::WorldQuantize, K::Choice, "world_quant","World Switch","world", 0, 2, 2, -1, "", { "Immediate", "Next Beat", "Next Bar" } },
        ParamSpec { Param::WorldMidi,     K::Bool,   "world_midi", "World MIDI", "world", 0, 1, 1 },
        ParamSpec { Param::WorldAlt,      K::Bool,   "world_alt",  "Chain Toggle","world", 0, 1, 0 },

        ParamSpec { Param::DriveType,     K::Choice, "drive_type", "Distortion Type", "drive",  0, 1, 0, -1, "", { "World", "Bitcrusher" } },
        ParamSpec { Param::DelayType,     K::Choice, "delay_type", "Delay Type",      "delay",  0, 1, 0, -1, "", { "World", "Simple Delay" } },
        ParamSpec { Param::ReverbType,    K::Choice, "reverb_type","Reverb Type",     "reverb", 0, 1, 0, -1, "", { "World", "Reverb" } },
    };

    for (int i = 0; i < numParams; ++i)
        assert (static_cast<int> (s[(size_t) i].param) == i && "paramSpecs() order must match the Param enum");

    return s;
}

const std::array<ParamSpec, numParams>& paramSpecs()
{
    static const auto specs = makeSpecs();
    return specs;
}

const ParamSpec& specOf (Param p)
{
    return paramSpecs()[(size_t) static_cast<int> (p)];
}

const std::vector<double>& delayDivisionsInBeats()
{
    static const std::vector<double> d { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };
    return d;
}

} // namespace acid
