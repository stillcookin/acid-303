#pragma once

#include "Theme.h"
#include "UiContext.h"

namespace acid::ui
{

/** 303-style step piano roll: one octave of note rows (C..C) per step, plus
    lanes for accent, slide, tie, octave and lock. Slides are drawn as glide
    lines between notes. All edits go through PatternController. */
class PianoRoll final : public juce::Component,
                        public juce::SettableTooltipClient,
                        private PatternController::Listener,
                        private juce::Timer
{
public:
    explicit PianoRoll (UiContext ctx);
    ~PianoRoll() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Lane { None, Accent, Slide, Tie, Octave, Lock };
    static constexpr int numRows = 13;
    static constexpr int numLanes = 5;
    static constexpr int laneH = 17;

    void patternChanged() override { repaint(); }
    void timerCallback() override;

    int stepAtX (int x) const;
    int semitoneAtY (int y) const;
    Lane laneAtY (int y) const;
    juce::Rectangle<float> cellRect (int step, int semitone) const;
    juce::Rectangle<float> laneCell (int laneIndex, int step) const;
    float columnX (int step) const;
    float columnWidth() const;

    void paintGrid (juce::Graphics&, const Pattern&);
    void paintNotes (juce::Graphics&, const Pattern&);
    void paintLanes (juce::Graphics&, const Pattern&);

    void applyLane (Lane lane, int step, bool value);

    UiContext ctx;
    juce::Rectangle<int> keyArea, gridArea, laneArea, numberArea;

    // drag state
    enum class DragMode { None, Paint, Erase, Lane } dragMode = DragMode::None;
    Lane dragLane = Lane::None;
    bool dragValue = false;
    int lastStep = -1, lastSemitone = -1;
    int shownPlayStep = -2;
};

} // namespace acid::ui
