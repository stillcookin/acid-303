#include "MainView.h"

namespace acid::ui
{

MainView::MainView (UiContext c)
    : ctx (c),
      header (c),
      voice (c.params, "VOICE",
             { Param::Waveform, Param::Tune, Param::Cutoff, Param::Resonance, Param::EnvMod,
               Param::Decay, Param::Accent, Param::Drift }),
      seq (c.params, "GROOVE / OUT", { Param::WorldQuantize, Param::Swing, Param::Gate, Param::Volume }),
      roll (c),
      generator (c),
      drive (c.params, "OVERDRIVE", { Param::Drive, Param::DriveTone, Param::DriveMix }, Param::DriveOn),
      delay (c.params, "DELAY", { Param::DelayTime, Param::DelayFeedback, Param::DelayTone, Param::DelayMix, Param::DelayPingPong },
             Param::DelayOn),
      reverb (c.params, "REVERB", { Param::ReverbSize, Param::ReverbDamp, Param::ReverbMix }, Param::ReverbOn)
{
    setLookAndFeel (&lnf);
    for (auto* comp : std::initializer_list<juce::Component*> { &header, &voice, &seq, &roll, &generator, &drive, &delay, &reverb })
        addAndMakeVisible (comp);
    if (auto* q = seq.controlFor (Param::WorldQuantize))
        q->setTooltip ("When a world change lands while playing: immediately, on the next beat, or on the next bar");
    setSize (theme::baseWidth, theme::baseHeight);
    timerCallback();
    startTimerHz (10);
}

void MainView::timerCallback()
{
    // the character knob means something different in each world
    const int w = ctx.live.world.load();
    if (w == shownWorld)
        return;
    shownWorld = w;
    if (auto* k = voice.controlFor (Param::Drift))
    {
        const auto world = worldFromIndex (w);
        k->setCaption (world == World::Space ? "DRIFT" : (world == World::Underground ? "SUB" : "AGE"));
        k->setTooltip (world == World::Space ? "Drift: pitch and slow filter wander"
                       : (world == World::Underground ? "Sub: sine one octave down" : "Age: analogue pitch drift"));
    }
}

MainView::~MainView()
{
    setLookAndFeel (nullptr);
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void MainView::resized()
{
    constexpr int gap = theme::gap;
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (52));
    r.reduce (gap, gap);

    auto top = r.removeFromTop (118);
    seq.setBounds (top.removeFromRight (330));
    top.removeFromRight (gap);
    voice.setBounds (top);
    r.removeFromTop (gap);

    auto bottom = r.removeFromBottom (186);
    r.removeFromBottom (gap);
    roll.setBounds (r);

    generator.setBounds (bottom.removeFromLeft (440));
    bottom.removeFromLeft (gap);
    reverb.setBounds (bottom.removeFromRight (160));
    bottom.removeFromRight (gap);
    drive.setBounds (bottom.removeFromLeft (160));
    bottom.removeFromLeft (gap);
    delay.setBounds (bottom);
}

} // namespace acid::ui
