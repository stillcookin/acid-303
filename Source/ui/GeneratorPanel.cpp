#include "GeneratorPanel.h"

namespace acid::ui
{

GeneratorPanel::GeneratorPanel (UiContext c) : ctx (c)
{
    for (int i = 0; i < 12; ++i)
        rootBox.addItem (rootNames()[(size_t) i], i + 1);
    rangeBox.addItem ("1 Oct", 1);
    rangeBox.addItem ("2 Oct", 2);

    for (auto* b : { &scaleBox, &rootBox, &rangeBox })
    {
        addAndMakeVisible (*b);
        b->onChange = [this] { pushSettings(); };
    }
    scaleBox.setTooltip ("Scale for generated and mutated notes");
    rootBox.setTooltip ("Root note");
    rangeBox.setTooltip ("Melodic range of generated lines");

    const char* captions[] { "DENSITY", "ACCENT", "SLIDE", "OCTAVE", "MUTATE" };
    const char* tips[] { "How many steps play", "Accent probability", "Slide probability",
                         "Octave-jump probability", "How much Mutate changes" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.caption = captions[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRange (0.0, 1.0);
        k.slider.setPopupDisplayEnabled (true, true, nullptr);
        k.slider.setTooltip (tips[i]);
        k.slider.onValueChange = [this] { pushSettings(); };
        addAndMakeVisible (k.slider);
    }

    auto& pc = ctx.patterns;
    addButton (generateButton, "GENERATE", "New random pattern (locked steps are kept)", [&pc] { pc.generate(); });
    addButton (mutateButton, "MUTATE", "Randomly vary unlocked steps", [&pc] { pc.mutate(); });
    addButton (accentsButton, "ACC", "Re-roll accents only", [&pc] { pc.randomizeAccents(); });
    addButton (slidesButton, "SLD", "Re-roll slides only", [&pc] { pc.randomizeSlides(); });
    addButton (octavesButton, "OCT", "Re-roll octave jumps only", [&pc] { pc.randomizeOctaves(); });
    addButton (leftButton, "<", "Shift pattern left", [&pc] { pc.shift (-1); });
    addButton (rightButton, ">", "Shift pattern right", [&pc] { pc.shift (1); });
    addButton (reverseButton, "REV", "Reverse pattern", [&pc] { pc.reverse(); });
    addButton (undoButton, "UNDO", "Undo", [&pc] { pc.undo(); });
    addButton (redoButton, "REDO", "Redo", [&pc] { pc.redo(); });
    addButton (clearButton, "CLR", "Clear unlocked steps", [&pc] { pc.clearPattern(); });
    seedLabel.setEditable (false, true, false);
    seedLabel.setJustificationType (juce::Justification::centred);
    seedLabel.setTooltip ("Seed of the last generated pattern. Double-click to type a seed and recall it.");
    seedLabel.onTextChange = [this]
    {
        const auto txt = seedLabel.getText().trim();
        if (txt.containsOnly ("0123456789") && txt.isNotEmpty())
            ctx.patterns.generateWithSeed ((uint32_t) txt.getLargeIntValue());
        else
            refresh();
    };
    addAndMakeVisible (seedLabel);

    ctx.patterns.addListener (this);
    refresh();
}

GeneratorPanel::~GeneratorPanel()
{
    ctx.patterns.removeListener (this);
}

juce::TextButton& GeneratorPanel::addButton (juce::TextButton& b, const juce::String& text, const juce::String& tip,
                                             std::function<void()> fn)
{
    b.setButtonText (text);
    b.setTooltip (tip);
    b.onClick = std::move (fn);
    addAndMakeVisible (b);
    return b;
}

void GeneratorPanel::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    const auto& gs = ctx.patterns.generatorSettings();
    // each world offers its own scales (item id = index into scales() + 1)
    if (indexOf (gs.world) != scaleListWorld)
    {
        scaleListWorld = indexOf (gs.world);
        scaleBox.clear (juce::dontSendNotification);
        for (int idx : worldScales (gs.world))
            scaleBox.addItem (scales()[(size_t) idx].name, idx + 1);
    }
    scaleBox.setSelectedId (gs.scale + 1, juce::dontSendNotification);
    rootBox.setSelectedId (gs.root + 1, juce::dontSendNotification);
    rangeBox.setSelectedId (gs.range, juce::dontSendNotification);
    const float values[] { gs.density, gs.accent, gs.slide, gs.octave, gs.mutateAmount };
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].slider.setValue (values[i], juce::dontSendNotification);
    seedLabel.setText ("SEED " + juce::String (gs.seed), juce::dontSendNotification);
    refreshButtons();
}

void GeneratorPanel::refreshButtons()
{
    undoButton.setEnabled (ctx.patterns.canUndo());
    redoButton.setEnabled (ctx.patterns.canRedo());
}

void GeneratorPanel::pushSettings()
{
    if (updating) return;
    auto gs = ctx.patterns.generatorSettings();
    gs.scale = scaleBox.getSelectedId() - 1;
    gs.root = rootBox.getSelectedId() - 1;
    gs.range = rangeBox.getSelectedId();
    gs.density = (float) knobs[0].slider.getValue();
    gs.accent = (float) knobs[1].slider.getValue();
    gs.slide = (float) knobs[2].slider.getValue();
    gs.octave = (float) knobs[3].slider.getValue();
    gs.mutateAmount = (float) knobs[4].slider.getValue();
    ctx.patterns.setGeneratorSettings (gs);
}

void GeneratorPanel::lookAndFeelChanged()
{
    const auto& p = theme::pal (*this);
    generateButton.setColour (juce::TextButton::buttonColourId, p.acid);
    generateButton.setColour (juce::TextButton::textColourOffId, p.onAcid);
    seedLabel.setColour (juce::Label::backgroundColourId, p.rollBg);
    seedLabel.setColour (juce::Label::textColourId, p.acid);
    seedLabel.setColour (juce::Label::outlineColourId, p.panelLo);
    seedLabel.setFont (theme::labelFont (18.0f));
    repaint();
}

void GeneratorPanel::paint (juce::Graphics& g)
{
    const auto& p = theme::pal (*this);
    theme::pixelPanel (g, getLocalBounds(), p.panel, p.panelHi, p.panelLo, p.edge);

    const auto world = ctx.patterns.world();
    const char* title = world == World::Space ? "STAR CHART" : (world == World::Underground ? "CORE SURVEY" : "PATTERN GENERATOR");
    g.setColour (p.title);
    g.setFont (theme::titleFont (8.0f));
    g.drawText (title, getLocalBounds().reduced (12, 0).removeFromTop (26), juce::Justification::centredLeft);

    g.setColour (p.textDim);
    g.setFont (theme::labelFont (16.0f));
    for (auto& k : knobs)
        g.drawText (k.caption, k.slider.getBounds().withY (k.slider.getBottom()).withHeight (14),
                    juce::Justification::centred);
}

void GeneratorPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 0);
    auto header = r.removeFromTop (24);
    seedLabel.setBounds (header.removeFromRight (110).reduced (0, 3));
    r.removeFromBottom (8);

    auto row1 = r.removeFromTop (26);
    scaleBox.setBounds (row1.removeFromLeft (150));
    row1.removeFromLeft (6);
    rootBox.setBounds (row1.removeFromLeft (64));
    row1.removeFromLeft (6);
    rangeBox.setBounds (row1.removeFromLeft (80));
    row1.removeFromLeft (6);
    generateButton.setBounds (row1);

    r.removeFromTop (4);
    auto buttons = r.removeFromBottom (24);
    auto knobRow = r;
    knobRow.removeFromBottom (14);
    const int kw = knobRow.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
        k.slider.setBounds (knobRow.removeFromLeft (kw).reduced (4, 0));

    juce::TextButton* row[] { &mutateButton, &accentsButton, &slidesButton, &octavesButton, &leftButton,
                              &rightButton, &reverseButton, &clearButton, &undoButton, &redoButton };
    const int weights[] { 7, 4, 4, 4, 3, 3, 4, 4, 5, 5 };
    int total = 0;
    for (int w : weights) total += w;
    const int unit = buttons.getWidth() / total;
    for (size_t i = 0; i < std::size (row); ++i)
        row[i]->setBounds (buttons.removeFromLeft (weights[i] * unit).reduced (2, 0));
}

} // namespace acid::ui
