#pragma once

#include "Theme.h"
#include "UiContext.h"
#include "WorldScenes.h"

namespace acid::ui
{

/** The World view: an animated pixel scene driven by the music. Notes move
    the ship / drill (pitch), loudness swells everything (smoothed outside
    Classic), tempo sets the speed, and the effect settings shape the scene.
    Drawn at 245×69 and scaled up with hard pixel edges. */
class WorldView final : public juce::Component,
                        private juce::Timer
{
public:
    explicit WorldView (UiContext ctx);

    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void tick (double dt);
    SceneFx readFx() const;
    float param (Param p) const;

    UiContext ctx;
    SceneState state;
    unsigned lastSerial = 0;
    bool wasRunning = false;
    juce::Image frame { juce::Image::RGB, WorldScenes::width, WorldScenes::height, true };
    double lastTime = 0.0;
};

} // namespace acid::ui
