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
    laneArea = r.removeFromBottom (numLanes * laneH);
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
    const int idx = juce::jlimit (0, numLanes - 1, (y - laneArea.getY()) / laneH);
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
    return { columnX (step), (float) (laneArea.getY() + laneIndex * laneH), columnWidth(), (float) laneH };
}

// ------------------------------------------------------------------ painting

void PianoRoll::paint (juce::Graphics& g)
{
    const auto& P = theme::pal (*this);
    theme::pixelPanel (g, getLocalBounds(), P.rollBg, P.panelHi, P.panelLo, P.edge);

    const auto& p = ctx.patterns.pattern();
    paintGrid (g, p);
    paintNotes (g, p);
    paintLanes (g, p);
}

void PianoRoll::paintGrid (juce::Graphics& g, const Pattern& p)
{
    const auto& P = theme::pal (*this);
    const float rowH = (float) gridArea.getHeight() / numRows;

    for (int s = 0; s < numRows; ++s)
    {
        const bool black = isBlackKey (s);
        // cells: alternate shading every four steps, playhead column highlighted
        for (int i = 0; i < p.length; ++i)
        {
            const bool alt = (i / 4) % 2 == 1;
            auto c = black ? (alt ? P.rowBlackAlt : P.rowBlack) : (alt ? P.rowWhiteAlt : P.rowWhite);
            if (i == shownPlayStep) c = P.playhead;
            g.setColour (c);
            g.fillRect (cellRect (i, s));
        }
        // key labels: naturals on a plate, sharps bare
        auto key = juce::Rectangle<float> ((float) keyArea.getX(), cellRect (0, s).getY(), (float) keyArea.getWidth() - 4.0f, rowH);
        if (! black)
        {
            g.setColour (P.keyWhite);
            g.fillRect (key.reduced (0.0f, 0.5f));
        }
        g.setColour (black ? P.rollDim : P.keyWhiteText);
        g.setFont (theme::labelFont (juce::jmin (15.0f, rowH + 3.0f)));
        g.drawText (noteNames[s], key.reduced (4.0f, 0.0f), juce::Justification::centredLeft);
    }

    for (int i = 0; i < p.length; ++i)
    {
        const float x = columnX (i);
        g.setColour (P.rollLine);
        g.fillRect (juce::Rectangle<float> (x, (float) gridArea.getY(), 1.0f, (float) gridArea.getHeight()));

        g.setColour (i == shownPlayStep ? P.acid : (i % 4 == 0 ? P.rollText : P.rollDim));
        g.setFont (theme::labelFont (16.0f));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (x, (float) numberArea.getY(), columnWidth(), 16.0f),
                    juce::Justification::centred);
    }
}

void PianoRoll::paintNotes (juce::Graphics& g, const Pattern& p)
{
    const auto& P = theme::pal (*this);

    auto soundingSemitone = [&] (int i)
    {
        for (int back = 0; back < p.length; ++back)
            if (p[i - back].gate == Gate::Note) return (int) p[i - back].semitone;
        return 0;
    };

    for (int i = 0; i < p.length; ++i)
    {
        if (p.effectiveGate (i) != Gate::Tie) continue;
        auto r = cellRect (i, soundingSemitone (i)).reduced (0.0f, 3.0f).withTrimmedRight (2.0f);
        r = r.withLeft (r.getX() - 3.0f);
        g.setColour (P.tie);
        g.fillRect (r);
    }

    for (int i = 0; i < p.length; ++i)
    {
        const auto& s = p.steps[(size_t) i];
        if (p.effectiveGate (i) != Gate::Note) continue;

        auto r = cellRect (i, s.semitone).reduced (2.0f, 1.0f);
        if (p[i + 1].gate == Gate::Tie && i + 1 < p.length)
            r = r.withRight (r.getRight() + 2.0f);

        g.setColour (s.accent ? P.accent : P.acid);
        g.fillRect (r);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRect (r.withTop (r.getBottom() - 2.0f));
        if (s.locked)
        {
            g.setColour (P.lock);
            g.drawRect (r, 2.0f);
        }
        if (s.octave != 0 && r.getWidth() > 14.0f)
        {
            g.setColour (P.onAcid);
            g.setFont (theme::labelFont (14.0f));
            g.drawText ((s.octave > 0 ? "+" : "") + juce::String ((int) s.octave), r, juce::Justification::centred);
        }
    }

    // glides: stepped pixel lines from note to note
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

        g.setColour (P.slide);
        const int n = 8;
        for (int k = 0; k <= n; ++k)
        {
            const float t = (float) k / n;
            const float e = t * t * (3.0f - 2.0f * t);
            g.fillRect (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * e }));
        }
    }
}

void PianoRoll::paintLanes (juce::Graphics& g, const Pattern& p)
{
    const auto& P = theme::pal (*this);
    for (int lane = 0; lane < numLanes; ++lane)
    {
        auto label = juce::Rectangle<float> ((float) keyArea.getX(), (float) (laneArea.getY() + lane * laneH),
                                             (float) keyArea.getWidth() - 4.0f, (float) laneH);
        g.setColour (P.rollDim);
        g.setFont (theme::labelFont (15.0f));
        g.drawText (laneNames[lane], label.reduced (2.0f, 0.0f), juce::Justification::centredLeft);

        for (int i = 0; i < p.length; ++i)
        {
            const auto& s = p.steps[(size_t) i];
            auto cell = laneCell (lane, i).reduced (3.0f, 3.0f);
            const auto lt = static_cast<Lane> (lane + 1);

            bool on = false;
            juce::Colour c;
            switch (lt)
            {
                case Lane::Accent: on = s.accent; c = P.accent; break;
                case Lane::Slide:  on = s.slide;  c = P.slide; break;
                case Lane::Tie:    on = s.gate == Gate::Tie; c = P.tie; break;
                case Lane::Lock:   on = s.locked; c = P.lock; break;
                case Lane::Octave:
                {
                    g.setColour (P.laneOff);
                    g.fillRect (cell);
                    g.setColour (P.laneEdge);
                    g.drawRect (cell, 1.0f);
                    g.setColour (s.octave == 0 ? P.rollDim : P.acid);
                    g.setFont (theme::labelFont (16.0f));
                    g.drawText (s.octave == 0 ? juce::String ("-") : (s.octave > 0 ? "+" : "") + juce::String ((int) s.octave),
                                cell, juce::Justification::centred);
                    continue;
                }
                case Lane::None: break;
            }
            g.setColour (on ? c : P.laneOff);
            g.fillRect (cell);
            g.setColour (on ? juce::Colours::black.withAlpha (0.35f) : P.laneEdge);
            g.drawRect (cell, 1.0f);
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
