#pragma once

#include "Pattern.h"

#include <array>
#include <optional>
#include <vector>

namespace acid
{

/** The instrument's "worlds" (modes). A world nudges the voice, decides which
    scales the generator offers and how it phrases, and (in later steps) which
    effects run and how the UI looks. Classic is the instrument as designed.

    The world is a host parameter, so it can be automated, MIDI-mapped, or
    switched from incoming MIDI (CC, program change, keyswitch notes). */
enum class World : int { Classic = 0, Space = 1, Underground = 2 };

inline constexpr int numWorlds = 3;

inline World worldFromIndex (int i)
{
    return static_cast<World> (i < 0 ? 0 : (i >= numWorlds ? numWorlds - 1 : i));
}

inline int indexOf (World w) { return static_cast<int> (w); }

const char* worldName (World);

/** Scales a world offers, as indices into scales(). The first is the default. */
const std::vector<int>& worldScales (World);

/** When a requested world change takes effect while the transport runs. */
enum class WorldQuantize : int { Immediate = 0, Beat = 1, Bar = 2 };

/** Incoming-MIDI mapping for world switching. */
namespace WorldMidi
{
    inline constexpr int controller = 102;   ///< CC 102: 0-42 Classic, 43-85 Space, 86-127 Underground
    inline constexpr int lowestKeyswitch = 0; ///< MIDI notes 0, 1, 2 (C-2, C#-2, D-2 in Live)

    std::optional<World> fromController (int ccNumber, int value);
    std::optional<World> fromProgramChange (int program);  ///< 0, 1, 2 (Live's Pgm 1, 2, 3)
    std::optional<World> fromKeyswitch (int note);
    bool isKeyswitch (int note);
}

/** One pattern per world: the same source pattern, retuned for each world.
    The engine plays the entry for its active world, so a world switch on the
    bar changes pitches sample-accurately without waiting for the UI thread. */
using WorldPatterns = std::array<Pattern, numWorlds>;

} // namespace acid
