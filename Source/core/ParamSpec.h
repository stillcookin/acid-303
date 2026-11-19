#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace acid
{

/** Every automatable parameter of the instrument.
    The order here defines the order of paramSpecs() and EngineParams::fromRaw().
    A UI only needs these IDs (via idOf()) to attach controls. */
enum class Param : int
{
    // Voice
    Tune, Waveform, Cutoff, Resonance, EnvMod, Decay, Accent, Drift, Volume,
    // Sequencer
    Mode, Swing, Gate,
    // FX: overdrive
    DriveOn, Drive, DriveTone, DriveMix,
    // FX: delay
    DelayOn, DelayTime, DelayFeedback, DelayTone, DelayMix, DelayPingPong,
    // FX: reverb
    ReverbOn, ReverbSize, ReverbDamp, ReverbMix,
    // World (appended: IDs and saved sets stay compatible)
    World, WorldQuantize, WorldMidi, WorldAlt,
    // effect choices per slot (appended): 0 = the world's own effect
    DriveType, DelayType, ReverbType,

    Count
};

inline constexpr int numParams = static_cast<int> (Param::Count);

struct ParamSpec
{
    enum class Kind { Float, Choice, Bool };

    Param param;
    Kind kind;
    const char* id;     ///< stable string ID, used for host automation & state; never rename
    const char* name;   ///< display name
    const char* group;  ///< "voice", "seq", "drive", "delay", "reverb", "world"
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    float skewCentre = -1.0f; ///< < 0 means linear
    const char* unit = "";
    std::vector<const char*> choices {};
};

const std::array<ParamSpec, numParams>& paramSpecs();
const ParamSpec& specOf (Param p);
inline const char* idOf (Param p) { return specOf (p).id; }

/** Modes for the Mode parameter. */
enum class PlayMode : int { Sequencer = 0, SequencerTranspose = 1, Midi = 2 };

/** Delay sync divisions, in quarter notes. Index matches the DelayTime choice list. */
const std::vector<double>& delayDivisionsInBeats();

} // namespace acid
