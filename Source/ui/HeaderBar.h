#pragma once

#include "ParamPanels.h"
#include "Theme.h"
#include "UiContext.h"

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

private:
    UiContext ctx;
    bool dragging = false;
};

/** Top bar: title, play mode, pattern slots, length, preview, MIDI drag, status. */
class HeaderBar final : public juce::Component,
                        private PatternController::Listener,
                        private juce::Timer
{
public:
    explicit HeaderBar (UiContext ctx);
    ~HeaderBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void patternChanged() override { refresh(); }
    void timerCallback() override;
    void refresh();

    UiContext ctx;
    ParamControl mode, world;
    std::array<juce::TextButton, PatternBank::numSlots> slotButtons;
    juce::ComboBox lengthBox;
    juce::TextButton previewButton { "PREVIEW" };
    MidiDragSource dragSource;
    juce::String status;
    float meter = 0.0f;
};

} // namespace acid::ui
