#include "PianoRoll.h"

namespace acid::ui
{

namespace
{
    const char* const noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C" };
    bool isBlackKey (int semi) { const int s = semi % 12; return s == 1 || s == 3 || s == 6 || s == 8 || s == 10; }
    const char* const laneNames[] { "ACC", "SLIDE", "TIE", "OCT", "LOCK" };
}

PianoRoll::PianoRoll (UiContext c) : ctx (c)
{
    ctx.patterns.addListener (this);
    startTimerHz (30);
    setTooltip ("Click: set/clear note  |  Drag: paint notes  |  Right-click: rest\n"
                "Lanes: click to toggle accent / slide / tie / lock; OCT: left = up, right = down");
}

PianoRoll::~PianoRoll()
{
    ctx.patterns.removeListener (this);
}

void PianoRoll::timerCallback()
{
    const int step = ctx.live.running.load() ? ctx.live.playingStep.load() : -1;
    if (step != shownPlayStep)
    {
        shownPlayStep = step;
        repaint (gridArea.getUnion (laneArea));
    }
}

void PianoRoll::resized()
{
    auto r = getLocalBounds().reduced (8);
    keyArea = r.removeFromLeft (44);
    numberArea = r.removeFromBottom (16);
    laneArea = r.removeFromBottom (numLanes * 20);
    r.removeFromBottom (6);
    gridArea = r;
    keyArea = keyArea.withTop (gridArea.getY()).withBottom (laneArea.getBottom());
}

// ------------------------------------------------------------------ geometry

float PianoRoll::columnWidth() const
{
    return (float) gridArea.getWidth() / (float) ctx.patterns.pattern().length;
}

float PianoRoll::columnX (int step) const
{
    return (float) gridArea.getX() + (float) step * columnWidth();
}

int PianoRoll::stepAtX (int x) const
{
    const int len = ctx.patterns.pattern().length;
    return juce::jlimit (0, len - 1, (int) ((float) (x - gridArea.getX()) / columnWidth()));
}

int PianoRoll::semitoneAtY (int y) const
{
    const float rowH = (float) gridArea.getHeight() / numRows;
    const int row = juce::jlimit (0, numRows - 1, (int) ((float) (y - gridArea.getY()) / rowH));
    return (numRows - 1) - row;
}

PianoRoll::Lane PianoRoll::laneAtY (int y) const
{
    if (! laneArea.contains (laneArea.getX(), y)) return Lane::None;
    const int idx = juce::jlimit (0, numLanes - 1, (y - laneArea.getY()) / 20);
    return static_cast<Lane> (idx + 1);
}

juce::Rectangle<float> PianoRoll::cellRect (int step, int semitone) const
{
    const float rowH = (float) gridArea.getHeight() / numRows;
    const int row = (numRows - 1) - semitone;
    return { columnX (step), (float) gridArea.getY() + (float) row * rowH, columnWidth(), rowH };
}

juce::Rectangle<float> PianoRoll::laneCell (int laneIndex, int step) const
{
    return { columnX (step), (float) (laneArea.getY() + laneIndex * 20), columnWidth(), 20.0f };
}

// ------------------------------------------------------------------ painting

void PianoRoll::paint (juce::Graphics& g)
{
    using namespace theme;
    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), corner);
    g.setColour (outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), corner, 1.0f);

    const auto& p = ctx.patterns.pattern();
    paintGrid (g, p);
    paintNotes (g, p);
    paintLanes (g, p);
}

void PianoRoll::paintGrid (juce::Graphics& g, const Pattern& p)
{
    using namespace theme;
    const float rowH = (float) gridArea.getHeight() / numRows;

    // rows & key labels
    for (int s = 0; s < numRows; ++s)
    {
        auto row = cellRect (0, s).withX ((float) gridArea.getX()).withWidth ((float) gridArea.getWidth());
        g.setColour (isBlackKey (s) ? blackKeyRow : gridBeat.brighter (0.05f));
        g.fillRect (row);

        auto key = juce::Rectangle<float> ((float) keyArea.getX(), row.getY(), (float) keyArea.getWidth() - 4.0f, rowH);
        g.setColour (isBlackKey (s) ? panelRaised.darker (0.4f) : panelRaised.brighter (0.15f));
        g.fillRect (key.reduced (0.0f, 0.5f));
        g.setColour (isBlackKey (s) ? textDim : text);
        g.setFont (font (10.5f, s % 12 == 0));
        g.drawText (noteNames[s], key.reduced (4.0f, 0.0f), juce::Justification::centredLeft);
    }

    // beat shading & column lines
    for (int i = 0; i < p.length; ++i)
    {
        const float x = columnX (i);
        if ((i / 4) % 2 == 1)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillRect (juce::Rectangle<float> (x, (float) gridArea.getY(), columnWidth(), (float) gridArea.getHeight()));
        }
        g.setColour (i % 4 == 0 ? outline : gridLine);
        g.drawVerticalLine ((int) x, (float) gridArea.getY(), (float) laneArea.getBottom());

        g.setColour (i % 4 == 0 ? text : textDim);
        g.setFont (font (10.0f, i % 4 == 0));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (x, (float) numberArea.getY(), columnWidth(), 16.0f),
                    juce::Justification::centred);
    }
    for (int s = 0; s <= numRows; ++s)
    {
        g.setColour (gridLine);
        g.drawHorizontalLine (gridArea.getY() + (int) ((float) s * rowH), (float) gridArea.getX(), (float) gridArea.getRight());
    }

    // playhead
    if (shownPlayStep >= 0 && shownPlayStep < p.length)
    {
        g.setColour (playhead);
        g.fillRect (juce::Rectangle<float> (columnX (shownPlayStep), (float) gridArea.getY(), columnWidth(),
                                            (float) (laneArea.getBottom() - gridArea.getY())));
    }
}

void PianoRoll::paintNotes (juce::Graphics& g, const Pattern& p)
{
    using namespace theme;

    auto soundingSemitone = [&] (int i)
    {
        for (int back = 0; back < p.length; ++back)
            if (p[i - back].gate == Gate::Note) return (int) p[i - back].semitone;
        return 0;
    };

    // tie bars first (so they sit under the note heads)
    for (int i = 0; i < p.length; ++i)
    {
        if (p.effectiveGate (i) != Gate::Tie) continue;
        auto r = cellRect (i, soundingSemitone (i)).reduced (0.0f, 3.0f).withTrimmedRight (2.0f);
        r = r.withLeft (r.getX() - 3.0f);
        g.setColour (tie.withAlpha (0.85f));
        g.fillRect (r);
    }

    for (int i = 0; i < p.length; ++i)
    {
        const auto& s = p.steps[(size_t) i];
        if (p.effectiveGate (i) != Gate::Note) continue;

        auto r = cellRect (i, s.semitone).reduced (2.0f, 2.0f);
        // a note followed by a tie visually joins it
        if (p[i + 1].gate == Gate::Tie && i + 1 < p.length)
            r = r.withRight (r.getRight() + 2.0f);

        g.setColour (s.accent ? accent : acid);
        g.fillRoundedRectangle (r, 2.5f);
        if (s.locked)
        {
            g.setColour (lock);
            g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.5f);
        }
        if (s.octave != 0 && r.getWidth() > 14.0f)
        {
            g.setColour (background);
            g.setFont (font (9.5f, true));
            g.drawText ((s.octave > 0 ? "+" : "") + juce::String ((int) s.octave), r, juce::Justification::centred);
        }
    }

    // glide lines
    for (int i = 0; i < p.length; ++i)
    {
        const auto& s = p.steps[(size_t) i];
        if (! s.slide || p.effectiveGate (i) == Gate::Rest) continue;

        const auto from = cellRect (i, soundingSemitone (i)).getCentre().withX (columnX (i) + columnWidth() * 0.75f);
        juce::Point<float> to;
        if (i + 1 < p.length && p.effectiveGate (i + 1) == Gate::Note)
            to = cellRect (i + 1, p[i + 1].semitone).getCentre().withX (columnX (i + 1) + columnWidth() * 0.3f);
        else
            to = from.translated (columnWidth() * 0.5f, 0.0f);

        juce::Path path;
        path.startNewSubPath (from);
        const float midX = (from.x + to.x) * 0.5f;
        path.cubicTo ({ midX, from.y }, { midX, to.y }, to);
        g.setColour (slide);
        g.strokePath (path, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (to));
    }
}

void PianoRoll::paintLanes (juce::Graphics& g, const Pattern& p)
{
    using namespace theme;
    for (int lane = 0; lane < numLanes; ++lane)
    {
        auto label = juce::Rectangle<float> ((float) keyArea.getX(), (float) (laneArea.getY() + lane * 20),
                                             (float) keyArea.getWidth() - 4.0f, 20.0f);
        g.setColour (textDim);
        g.setFont (font (9.5f, true));
        g.drawText (laneNames[lane], label.reduced (2.0f, 0.0f), juce::Justification::centredLeft);
        g.setColour (gridLine);
        g.drawHorizontalLine (laneArea.getY() + lane * 20, (float) gridArea.getX(), (float) gridArea.getRight());

        for (int i = 0; i < p.length; ++i)
        {
            const auto& s = p.steps[(size_t) i];
            auto cell = laneCell (lane, i).reduced (3.0f, 4.0f);
            const auto lt = static_cast<Lane> (lane + 1);

            bool on = false;
            juce::Colour c;
            switch (lt)
            {
                case Lane::Accent: on = s.accent; c = accent; break;
                case Lane::Slide:  on = s.slide;  c = slide; break;
                case Lane::Tie:    on = s.gate == Gate::Tie; c = tie; break;
                case Lane::Lock:   on = s.locked; c = lock; break;
                case Lane::Octave:
                {
                    g.setColour (s.octave == 0 ? outline : acid);
                    g.setFont (font (10.0f, true));
                    g.drawText (s.octave == 0 ? juce::String ("-") : (s.octave > 0 ? "+" : "") + juce::String ((int) s.octave),
                                cell, juce::Justification::centred);
                    continue;
                }
                case Lane::None: break;
            }
            g.setColour (on ? c : outline.withAlpha (0.5f));
            if (on) g.fillRoundedRectangle (cell, 2.0f);
            else g.drawRoundedRectangle (cell, 2.0f, 1.0f);
        }
    }
}

// ------------------------------------------------------------------ mouse

void PianoRoll::applyLane (Lane lane, int step, bool value)
{
    auto s = ctx.patterns.pattern()[step];
    switch (lane)
    {
        case Lane::Accent: if (s.accent == value) return; s.accent = value; if (value && s.gate == Gate::Rest) s.gate = Gate::Note; break;
        case Lane::Slide:  if (s.slide == value) return;  s.slide = value;  if (value && s.gate == Gate::Rest) s.gate = Gate::Note; break;
        case Lane::Lock:   if (s.locked == value) return; s.locked = value; break;
        case Lane::Tie:
            if ((s.gate == Gate::Tie) == value) return;
            s.gate = value ? Gate::Tie : Gate::Note;
            break;
        case Lane::Octave:
        case Lane::None:
            return;
    }
    ctx.patterns.setStep (step, s);
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    const int step = stepAtX (pos.x);
    lastStep = step;
    dragMode = DragMode::None;

    if (gridArea.contains (pos))
    {
        const int semi = semitoneAtY (pos.y);
        lastSemitone = semi;
        ctx.patterns.beginGesture();
        if (e.mods.isPopupMenu())
        {
            ctx.patterns.setGate (step, Gate::Rest);
            dragMode = DragMode::Erase;
            return;
        }
        const auto& s = ctx.patterns.pattern()[step];
        if (s.gate == Gate::Note && s.semitone == semi)
        {
            ctx.patterns.setGate (step, Gate::Rest);
            dragMode = DragMode::Erase;
        }
        else
        {
            ctx.patterns.setNote (step, semi);
            dragMode = DragMode::Paint;
        }
        return;
    }

    const auto lane = laneAtY (pos.y);
    if (lane == Lane::None || pos.x < gridArea.getX())
        return;

    ctx.patterns.beginGesture();
    if (lane == Lane::Octave)
    {
        ctx.patterns.cycleOctave (step, e.mods.isPopupMenu() ? -1 : 1);
        return;
    }
    const auto& s = ctx.patterns.pattern()[step];
    const bool current = lane == Lane::Accent ? s.accent
                       : lane == Lane::Slide  ? s.slide
                       : lane == Lane::Lock   ? s.locked
                                              : s.gate == Gate::Tie;
    dragMode = DragMode::Lane;
    dragLane = lane;
    dragValue = ! current;
    applyLane (lane, step, dragValue);
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::None) return;
    const auto pos = e.getPosition();
    const int step = stepAtX (pos.x);

    if (dragMode == DragMode::Lane)
    {
        if (step != lastStep)
        {
            // fill any skipped steps when dragging fast
            const int dir = step > lastStep ? 1 : -1;
            for (int i = lastStep + dir; i != step + dir; i += dir)
                applyLane (dragLane, i, dragValue);
            lastStep = step;
        }
        return;
    }

    const int semi = semitoneAtY (pos.y);
    if (step == lastStep && semi == lastSemitone) return;
    if (dragMode == DragMode::Paint)
        ctx.patterns.setNote (step, semi);
    else if (step != lastStep)
        ctx.patterns.setGate (step, Gate::Rest);
    lastStep = step;
    lastSemitone = semi;
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    ctx.patterns.endGesture();
    dragMode = DragMode::None;
}

} // namespace acid::ui
