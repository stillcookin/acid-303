#pragma once

#include <juce_graphics/juce_graphics.h>

#include "seq/World.h"

#include <array>
#include <deque>

namespace acid::ui
{

/** Everything a scene needs, updated 30 times a second by WorldView.
    Space and Underground use smoothed motion (ve, spring-eased spos, eased
    speed); Classic's scope uses the raw note energy (e). */
struct SceneState
{
    int frame = 0;
    double clock = 0.0;            ///< seconds of playback
    bool playing = false;
    int step = -1;                 ///< playing step, -1 when stopped
    float e = 0.0f, ve = 0.0f;     ///< raw / smoothed note energy
    float spos = 12.0f, vel = 0.0f, target = 12.0f;
    bool glide = false;
    float spd = 0.15f, scroll = 0.0f, kick = 0.0f, dist = 0.0f;
    double lastTrig = -10.0;
    bool lastAccent = false;
    std::deque<float> hist, ehist; ///< recent positions / energies, newest first (≤ 64)
    World world = World::Classic;
    bool alt = false;
    bool square = false;
    std::array<bool, 16> usedSteps {};
};

/** The effect settings scenes react to (slot order: 0 drive-ish, 1 delay-ish, 2 reverb-ish). */
struct SceneFx
{
    std::array<bool, 3> on {};
    std::array<std::array<float, 3>, 3> v {};
    bool option = false;           ///< ping-pong / tape / rock damp
    double delaySeconds = 0.3;
};

namespace WorldScenes
{
    inline constexpr int width = 245, height = 69;       ///< World view, drawn ×4
    inline constexpr int bandWidth = 250, bandHeight = 12; ///< header strip, drawn ×4

    void drawWorld (juce::Graphics&, const SceneState&, const SceneFx&);
    void drawBand (juce::Graphics&, World, int step, bool playing, bool alt);
}

} // namespace acid::ui
