#include "HeaderBar.h"

namespace acid::ui
{

namespace
{
    const char* worldLabel (World w)
    {
        return w == World::Space ? "SPACE" : (w == World::Underground ? "UNDERGROUND" : "CLASSIC");
    }
    const char* chainLabel (World w)
    {
        return w == World::Space ? "DEEP SPACE" : (w == World::Underground ? "MOLTEN CORE" : "HOT");
    }
    const char* worldTag (World w, bool alt)
    {
        if (w == World::Space) return alt ? "DEEP SPACE / ESCAPING THE HOLE" : "SPACE / ENDLESS DRIFT";
        if (w == World::Underground) return alt ? "MOLTEN CORE / DIAMOND DEPTH" : "UNDERGROUND / DRILL RIG";
        return alt ? "HOT / DRIVE INTO THE FILTER" : "CLASSIC / HARDWARE BASS";
    }
}

// -------------------------------------------------------------------------- MIDI drag

MidiDragSource::MidiDragSource (UiContext c) : ctx (c)
{
    setTooltip ("Drag the current pattern into Live as a MIDI clip");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void MidiDragSource::paint (juce::Graphics& g)
{
    const auto& p = theme::pal (*this);
    theme::pixelPanel (g, getLocalBounds(), isMouseOver() ? p.raised.brighter (0.06f) : p.raised, p.raisedHi, p.raisedLo, p.edge);
    g.setColour (p.slide);
    g.setFont (theme::labelFont (17.0f));
    g.drawText ("DRAG MIDI", getLocalBounds(), juce::Justification::centred);
}

void MidiDragSource::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;
    dragging = true;

    float gate = 0.55f;
    if (auto* p = ctx.params.getRawParameterValue (idOf (Param::Gate)))
        gate = p->load();

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Rungs");
    dir.createDirectory();
    auto file = dir.getChildFile ("Rungs Pattern " + juce::String (ctx.patterns.currentSlot() + 1) + ".mid");
    if (ctx.patterns.exportMidi (file, 1, gate, ctx.live.transpose.load()))
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                    [this] { dragging = false; });
    else
        dragging = false;
}

// -------------------------------------------------------------------------- world button

WorldButton::WorldButton (World w) : juce::Button (worldLabel (w)), world (w)
{
    setTitle (juce::String ("World: ") + worldLabel (w));
}

void WorldButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto& p = theme::pal (*this);
    const bool on = getToggleState();
    auto fill = on ? p.acid : p.raised;
    if (highlighted && ! down) fill = fill.brighter (0.06f);
    const auto hi = on ? p.acidHi : p.raisedHi, lo = on ? p.acidLo : p.raisedLo;
    theme::pixelPanel (g, getLocalBounds(), fill, down ? lo : hi, down ? hi : lo, pending ? p.accent : p.edge);

    // 7×7 ladder, the world's rung lit (top: Space, middle: Classic, bottom: Underground)
    const int litRow = world == World::Space ? 1 : (world == World::Underground ? 5 : 3);
    const auto ink = on ? p.onAcid : p.text;
    const int cell = 3, ox = 8, oy = (getHeight() - 7 * cell) / 2;
    g.setColour (ink);
    for (int y = 0; y < 7; ++y)
    {
        if (y == litRow) continue;
        g.fillRect (ox + 1 * cell, oy + y * cell, cell, cell);
        g.fillRect (ox + 5 * cell, oy + y * cell, cell, cell);
        if (y % 2 == 1) g.fillRect (ox + 2 * cell, oy + y * cell, 3 * cell, cell);
    }
    g.setColour (on ? p.onAcid : p.acid);
    g.fillRect (ox, oy + litRow * cell, 7 * cell, cell);

    g.setColour (ink);
    g.setFont (theme::labelFont (17.0f));
    g.drawText (getButtonText(), getLocalBounds().withTrimmedLeft (ox + 7 * cell + 4).withTrimmedRight (6),
                juce::Justification::centredLeft, false);
}

// -------------------------------------------------------------------------- header

HeaderBar::HeaderBar (UiContext c)
    : ctx (c), mode (c.params, Param::Mode), dragSource (c)
{
    mode.setShowCaption (false);
    mode.setTooltip ("Play mode: Sequencer, Seq + Transpose (keys transpose the pattern), or MIDI (play from clips)");
    addAndMakeVisible (mode);

    for (int i = 0; i < numWorlds; ++i)
    {
        const auto w = worldFromIndex (i);
        worldButtons[(size_t) i] = std::make_unique<WorldButton> (w);
        auto& b = *worldButtons[(size_t) i];
        b.setTooltip ("Switch to " + juce::String (worldLabel (w)) + ". Also from MIDI: CC 102, program change 1-3, or keys C-2 / C#-2 / D-2");
        b.onClick = [this, w] { selectWorld (w); };
        addAndMakeVisible (b);
    }

    chainButton.setClickingTogglesState (true);
    chainButton.setTooltip ("The world's alternative signal chain: HOT, DEEP SPACE or MOLTEN CORE");
    chainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (ctx.params, idOf (Param::WorldAlt), chainButton);
    addAndMakeVisible (chainButton);

    for (int i = 0; i < (int) slotButtons.size(); ++i)
    {
        auto& b = slotButtons[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.setClickingTogglesState (false);
        b.setTooltip ("Pattern slot " + juce::String (i + 1) + "  (shift-click: copy current pattern here)");
        b.onClick = [this, i]
        {
            if (juce::ModifierKeys::currentModifiers.isShiftDown())
                ctx.patterns.copySlot (ctx.patterns.currentSlot(), i);
            ctx.patterns.selectSlot (i);
        };
        addAndMakeVisible (b);
    }

    for (int n = 1; n <= Pattern::maxSteps; ++n)
        lengthBox.addItem (juce::String (n) + " steps", n);
    lengthBox.setTooltip ("Pattern length");
    lengthBox.onChange = [this]
    {
        if (lengthBox.getSelectedId() != ctx.patterns.pattern().length)
            ctx.patterns.setLength (lengthBox.getSelectedId());
    };
    addAndMakeVisible (lengthBox);

    // Live-style clip tools, grouped around the length they change
    halfButton.setTooltip (":2  Half speed: spread the notes over twice the steps (16 > 32)");
    halfButton.onClick = [this] { ctx.patterns.halfSpeed(); };
    fastButton.setTooltip ("x2  Double speed: squeeze the notes into half the steps (keeps every other step)");
    fastButton.onClick = [this] { ctx.patterns.doubleSpeed(); };
    dupButton.setTooltip ("Duplicate: copy the pattern after itself (8 > 16 > 32 steps)");
    dupButton.onClick = [this] { ctx.patterns.duplicate(); };
    halfButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    fastButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    for (auto* b : { &halfButton, &fastButton, &dupButton })
        addAndMakeVisible (b);

    previewButton.setClickingTogglesState (true);
    previewButton.setTooltip ("Play the pattern on an internal clock while the host transport is stopped");
    previewButton.onClick = [this] { ctx.live.previewRequested = previewButton.getToggleState(); };
    previewButton.setToggleState (ctx.live.previewRequested.load(), juce::dontSendNotification);
    addAndMakeVisible (previewButton);

    addAndMakeVisible (dragSource);

    rollButton.setTooltip ("Piano roll: edit the pattern");
    worldViewButton.setTooltip ("World view: the scene reacts to the music");
    rollButton.onClick = [this] { setWorldView (false); if (onViewChange) onViewChange (false); };
    worldViewButton.onClick = [this] { setWorldView (true); if (onViewChange) onViewChange (true); };
    addAndMakeVisible (rollButton);
    addAndMakeVisible (worldViewButton);
    setWorldView (false);

    ctx.patterns.addListener (this);
    refresh();
    timerCallback();
    startTimerHz (15);
}

HeaderBar::~HeaderBar()
{
    ctx.patterns.removeListener (this);
}

void HeaderBar::setWorldView (bool world)
{
    rollButton.setToggleState (! world, juce::dontSendNotification);
    worldViewButton.setToggleState (world, juce::dontSendNotification);
}

void HeaderBar::selectWorld (World w)
{
    if (auto* p = ctx.params.getParameter (idOf (Param::World)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) indexOf (w)));
        p->endChangeGesture();
    }
}

void HeaderBar::lookAndFeelChanged()
{
    const auto& p = theme::pal (*this);
    chainButton.setColour (juce::TextButton::buttonOnColourId, p.accent);
    chainButton.setColour (juce::TextButton::textColourOnId, p.onAcid);
    repaint();
}

void HeaderBar::refresh()
{
    for (int i = 0; i < (int) slotButtons.size(); ++i)
        slotButtons[(size_t) i].setToggleState (i == ctx.patterns.currentSlot(), juce::dontSendNotification);
    lengthBox.setSelectedId (ctx.patterns.pattern().length, juce::dontSendNotification);
    const int len = ctx.patterns.pattern().length;
    dupButton.setEnabled (len * 2 <= Pattern::maxSteps);
    halfButton.setEnabled (len * 2 <= Pattern::maxSteps);
    fastButton.setEnabled (len >= 2);
}

void HeaderBar::timerCallback()
{
    const int w = ctx.live.world.load();
    const int pending = ctx.live.pendingWorld.load();
    const bool running = ctx.live.running.load();
    const int step = running ? ctx.live.playingStep.load() : -1;
    bool alt = false;
    if (auto* v = ctx.params.getRawParameterValue (idOf (Param::WorldAlt)))
        alt = v->load() > 0.5f;

    for (int i = 0; i < numWorlds; ++i)
    {
        worldButtons[(size_t) i]->setToggleState (i == w, juce::dontSendNotification);
        worldButtons[(size_t) i]->setPending (i == pending);
    }

    if (w != shownWorld)
    {
        if (shownWorld >= 0)
            retuneTicks = 38; // ~2.5 s of "RETUNED TO ..."
        chainButton.setButtonText (chainLabel (worldFromIndex (w)));
    }

    juce::String newTag = worldTag (worldFromIndex (w), alt);
    if (retuneTicks > 0)
    {
        --retuneTicks;
        if (w != indexOf (World::Classic))
            newTag = "RETUNED TO " + juce::String (scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1,
                                                                                      ctx.patterns.generatorSettings().scale)].name).toUpperCase();
    }

    juce::String s = juce::String (ctx.live.bpm.load(), 1) + " BPM";
    if (pending >= 0) s = "> " + juce::String (worldLabel (worldFromIndex (pending)));
    const int t = ctx.live.transpose.load();
    if (pending < 0 && t != 0) s << "  " << (t > 0 ? "+" : "") << t;

    if (w != shownWorld || step != shownStep || alt != shownAlt || running != shownRunning || s != status || newTag != tag)
    {
        shownWorld = w;
        shownStep = step;
        shownAlt = alt;
        shownRunning = running;
        status = s;
        tag = newTag;
        juce::Graphics bg (band);
        WorldScenes::drawBand (bg, worldFromIndex (w), step, running, alt);
        repaint();
    }
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& p = theme::pal (*this);
    auto r = getLocalBounds();
    auto strip = r.removeFromTop (bandHeight);

    // scene strip, drawn at 4× with hard pixel edges
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (band, strip.toFloat(), juce::RectanglePlacement::stretchToFit);

    // title plate
    auto plate = strip.withWidth (216);
    g.setColour (p.plate);
    g.fillRect (plate);
    g.setColour (p.edge);
    g.fillRect (plate.removeFromRight (4));
    auto title = plate.reduced (14, 8);
    g.setColour (p.acid);
    g.setFont (theme::titleFont (16.0f));
    g.drawText ("RUNGS", title.removeFromTop (20), juce::Justification::centredLeft, false);
    g.setColour (p.plateDim);
    g.setFont (theme::titleFont (6.0f));
    g.drawText (tag, title, juce::Justification::centredLeft, true);

    // world plate
    auto right = strip.withLeft (strip.getRight() - 356);
    g.setColour (p.plate);
    g.fillRect (right);
    g.setColour (p.edge);
    g.fillRect (right.withWidth (4));

    // controls row
    g.setColour (p.plate);
    g.fillRect (r);
    g.setColour (p.edge);
    g.fillRect (r.removeFromTop (2));
    g.fillRect (r.removeFromBottom (2));

    g.setColour (p.plateText);
    g.setFont (theme::labelFont (18.0f));
    g.drawFittedText (status, statusArea, juce::Justification::centredRight, 1, 0.8f);
}

void HeaderBar::resized()
{
    auto strip = getLocalBounds().removeFromTop (bandHeight);
    auto right = strip.withLeft (strip.getRight() - 356).reduced (10, 8).withTrimmedLeft (4);
    const int widths[] { 96, 86, 136 };
    for (int i = 0; i < numWorlds; ++i)
    {
        worldButtons[(size_t) i]->setBounds (right.removeFromLeft (widths[i]));
        right.removeFromLeft (6);
    }
    chainButton.setBounds (strip.getRight() - 356 - 12 - 120, 8, 120, 32);

    auto r = getLocalBounds().withTrimmedTop (bandHeight).reduced (10, 6);
    mode.setBounds (r.removeFromLeft (120));
    r.removeFromLeft (8);
    auto slots = r.removeFromLeft (304);
    const int w = slots.getWidth() / (int) slotButtons.size();
    for (auto& b : slotButtons)
        b.setBounds (slots.removeFromLeft (w).reduced (1, 0));
    r.removeFromLeft (8);
    halfButton.setBounds (r.removeFromLeft (30));
    lengthBox.setBounds (r.removeFromLeft (84));
    fastButton.setBounds (r.removeFromLeft (30));
    r.removeFromLeft (4);
    dupButton.setBounds (r.removeFromLeft (40));
    r.removeFromLeft (8);
    previewButton.setBounds (r.removeFromLeft (68));
    r.removeFromLeft (6);
    dragSource.setBounds (r.removeFromLeft (76));
    r.removeFromLeft (8);
    rollButton.setBounds (r.removeFromLeft (52));
    worldViewButton.setBounds (r.removeFromLeft (58));
    statusArea = r.withTrimmedLeft (6);
}

} // namespace acid::ui
