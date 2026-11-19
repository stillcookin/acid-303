#pragma once

#include "ParamPanels.h"
#include "Theme.h"
#include "UiContext.h"
#include "WorldScenes.h"

#include <functional>

namespace acid::ui
{

/** Drag handle that exports the current pattern as a .mid file and starts an
    external drag, so it can be dropped straight into a Live clip slot. */
class MidiDragSource final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MidiDragSource (UiContext ctx);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    UiContext ctx;
    bool dragging = false;
};

/** A world button: a small pixel ladder with the world's rung lit, plus its name. */
class WorldButton final : public juce::Button
{
public:
    WorldButton (World w);
    void setPending (bool p) { if (p != pending) { pending = p; repaint(); } }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    World world;
    bool pending = false;
};

/** Top of the window, two rows:
    1. a pixel scene strip with the RUNGS title, the chain toggle and the world buttons
    2. play mode, pattern slots, length, ×2, preview, MIDI drag, ROLL / WORLD and status */
class HeaderBar final : public juce::Component,
                        private PatternController::Listener,
                        private juce::Timer
{
public:
    explicit HeaderBar (UiContext ctx);
    ~HeaderBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

    /** Called with true when the user picks WORLD, false for ROLL. */
    std::function<void (bool)> onViewChange;
    void setWorldView (bool world);

    static constexpr int bandHeight = 48;

private:
    void patternChanged() override { refresh(); }
    void timerCallback() override;
    void refresh();
    void selectWorld (World w);

    UiContext ctx;
    ParamControl mode;
    std::array<std::unique_ptr<WorldButton>, numWorlds> worldButtons;
    juce::TextButton chainButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chainAttachment;
    std::array<juce::TextButton, PatternBank::numSlots> slotButtons;
    juce::ComboBox lengthBox;
    juce::TextButton doubleButton { "x2" }, previewButton { "PREVIEW" }, rollButton { "ROLL" }, worldViewButton { "WORLD" };
    MidiDragSource dragSource;

    juce::Image band { juce::Image::RGB, WorldScenes::bandWidth, WorldScenes::bandHeight, true };
    juce::String status, tag;
    int shownWorld = -1, shownStep = -2, retuneTicks = 0;
    bool shownAlt = false, shownRunning = false;
};

} // namespace acid::ui
