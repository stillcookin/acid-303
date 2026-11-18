#include "MainView.h"

namespace acid::ui
{

MainView::MainView (UiContext ctx)
    : header (ctx),
      voice (ctx.params, "VOICE",
             { Param::Waveform, Param::Tune, Param::Cutoff, Param::Resonance, Param::EnvMod,
               Param::Decay, Param::Accent, Param::Drift }),
      seq (ctx.params, "GROOVE / OUT", { Param::Swing, Param::Gate, Param::Volume }),
      roll (ctx),
      generator (ctx),
      drive (ctx.params, "OVERDRIVE", { Param::Drive, Param::DriveTone, Param::DriveMix }, Param::DriveOn),
      delay (ctx.params, "DELAY", { Param::DelayTime, Param::DelayFeedback, Param::DelayTone, Param::DelayMix, Param::DelayPingPong },
             Param::DelayOn),
      reverb (ctx.params, "REVERB", { Param::ReverbSize, Param::ReverbDamp, Param::ReverbMix }, Param::ReverbOn)
{
    setLookAndFeel (&lnf);
    for (auto* c : std::initializer_list<juce::Component*> { &header, &voice, &seq, &roll, &generator, &drive, &delay, &reverb })
        addAndMakeVisible (c);
    setSize (theme::baseWidth, theme::baseHeight);
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
    seq.setBounds (top.removeFromRight (250));
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
