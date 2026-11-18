#include "HeaderBar.h"

namespace acid::ui
{

MidiDragSource::MidiDragSource (UiContext c) : ctx (c)
{
    setTooltip ("Drag the current pattern into Live as a MIDI clip");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void MidiDragSource::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (isMouseOver() ? panelRaised.brighter (0.1f) : panelRaised);
    g.fillRoundedRectangle (r, corner);
    g.setColour (slide);
    g.drawRoundedRectangle (r, corner, 1.0f);
    g.setFont (font (11.5f, true));
    g.drawText ("DRAG MIDI", r, juce::Justification::centred);
}

void MidiDragSource::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;
    dragging = true;

    float gate = 0.55f;
    if (auto* p = ctx.params.getRawParameterValue (idOf (Param::Gate)))
        gate = p->load();

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Acid303");
    dir.createDirectory();
    auto file = dir.getChildFile ("Acid303 Pattern " + juce::String (ctx.patterns.currentSlot() + 1) + ".mid");
    if (ctx.patterns.exportMidi (file, 1, gate, ctx.live.transpose.load()))
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                    [this] { dragging = false; });
    else
        dragging = false;
}

// --------------------------------------------------------------------------

HeaderBar::HeaderBar (UiContext c)
    : ctx (c), mode (c.params, Param::Mode), dragSource (c)
{
    mode.setShowCaption (false);
    addAndMakeVisible (mode);

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

    previewButton.setClickingTogglesState (true);
    previewButton.setTooltip ("Play the pattern on an internal clock while the host transport is stopped");
    previewButton.onClick = [this] { ctx.live.previewRequested = previewButton.getToggleState(); };
    previewButton.setToggleState (ctx.live.previewRequested.load(), juce::dontSendNotification);
    addAndMakeVisible (previewButton);

    addAndMakeVisible (dragSource);

    ctx.patterns.addListener (this);
    refresh();
    startTimerHz (15);
}

HeaderBar::~HeaderBar()
{
    ctx.patterns.removeListener (this);
}

void HeaderBar::refresh()
{
    for (int i = 0; i < (int) slotButtons.size(); ++i)
        slotButtons[(size_t) i].setToggleState (i == ctx.patterns.currentSlot(), juce::dontSendNotification);
    lengthBox.setSelectedId (ctx.patterns.pattern().length, juce::dontSendNotification);
}

void HeaderBar::timerCallback()
{
    juce::String s = juce::String (ctx.live.bpm.load(), 1) + " BPM";
    if (ctx.live.hostPlaying.load()) s << "  |  HOST";
    else if (ctx.live.running.load()) s << "  |  PREVIEW";
    const int t = ctx.live.transpose.load();
    if (t != 0) s << "  |  " << (t > 0 ? "+" : "") << t << " st";

    const float m = juce::jlimit (0.0f, 1.0f, ctx.live.outputPeak.load());
    if (s != status || std::abs (m - meter) > 0.01f)
    {
        status = s;
        meter = m;
        repaint();
    }
}

void HeaderBar::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds();
    g.setColour (panel);
    g.fillRect (r);
    g.setColour (outline);
    g.drawHorizontalLine (r.getBottom() - 1, 0.0f, (float) r.getWidth());

    auto title = r.reduced (12, 0).removeFromLeft (120);
    g.setColour (acid);
    g.setFont (font (20.0f, true));
    g.drawText ("ACID", title, juce::Justification::centredLeft);
    g.setColour (text);
    g.drawText ("303", title.withTrimmedLeft (52), juce::Justification::centredLeft);

    // status + meter (right edge)
    auto right = r.reduced (10, 0).removeFromRight (150);
    g.setColour (textDim);
    g.setFont (font (11.0f));
    g.drawText (status, right.removeFromTop (r.getHeight() / 2 + 4).withTrimmedTop (4), juce::Justification::centredRight);
    auto meterR = right.removeFromTop (6).toFloat();
    g.setColour (outline);
    g.fillRoundedRectangle (meterR, 2.0f);
    g.setColour (meter > 0.98f ? accent : acid);
    g.fillRoundedRectangle (meterR.withWidth (meterR.getWidth() * std::sqrt (meter)), 2.0f);
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromLeft (110);
    r.removeFromRight (160);

    mode.setBounds (r.removeFromLeft (132).withSizeKeepingCentre (132, 26));
    r.removeFromLeft (10);

    dragSource.setBounds (r.removeFromRight (84).withSizeKeepingCentre (84, 28));
    r.removeFromRight (6);
    previewButton.setBounds (r.removeFromRight (74).withSizeKeepingCentre (74, 28));
    r.removeFromRight (6);
    lengthBox.setBounds (r.removeFromRight (96).withSizeKeepingCentre (96, 26));
    r.removeFromRight (10);

    // two rows of 8 slot buttons
    const int half = (int) slotButtons.size() / 2;
    const int w = r.getWidth() / half;
    auto rows = r.withSizeKeepingCentre (w * half, 34);
    auto topRow = rows.removeFromTop (17), bottomRow = rows;
    for (int i = 0; i < (int) slotButtons.size(); ++i)
    {
        auto& row = i < half ? topRow : bottomRow;
        slotButtons[(size_t) i].setBounds (row.removeFromLeft (w).reduced (1, 1));
    }
}

} // namespace acid::ui
