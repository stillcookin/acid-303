#include "World.h"

namespace acid
{

const char* worldName (World w)
{
    switch (w)
    {
        case World::Space:       return "Space";
        case World::Underground: return "Underground";
        case World::Classic:     break;
    }
    return "Classic";
}

const std::vector<int>& worldScales (World w)
{
    // indices into scales() (Generator.cpp)
    static const std::vector<int> classic { 0, 1, 2, 3, 4, 5, 6, 7 };  // the original eight
    static const std::vector<int> space { 8, 9, 2 };                     // Lydian, Whole Tone, Dorian
    static const std::vector<int> underground { 3, 10, 11 };             // Harmonic Minor, Phrygian Dom, Diminished HW

    switch (w)
    {
        case World::Space:       return space;
        case World::Underground: return underground;
        case World::Classic:     break;
    }
    return classic;
}

namespace WorldMidi
{

std::optional<World> fromController (int ccNumber, int value)
{
    if (ccNumber != controller)
        return std::nullopt;
    if (value < 43) return World::Classic;
    if (value < 86) return World::Space;
    return World::Underground;
}

std::optional<World> fromProgramChange (int program)
{
    if (program < 0 || program >= numWorlds)
        return std::nullopt;
    return worldFromIndex (program);
}

bool isKeyswitch (int note)
{
    return note >= lowestKeyswitch && note < lowestKeyswitch + numWorlds;
}

std::optional<World> fromKeyswitch (int note)
{
    if (! isKeyswitch (note))
        return std::nullopt;
    return worldFromIndex (note - lowestKeyswitch);
}

} // namespace WorldMidi
} // namespace acid
