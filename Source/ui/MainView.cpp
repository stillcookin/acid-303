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
      worldView (c),
      generator (c),
      drive (c.params, "OVERDRIVE", { Param::Drive, Param::DriveTone, Param::DriveMix }, Param::DriveOn),
      delay (c.params, "DELAY", { Param::DelayTime, Param::DelayFeedback, Param::DelayTone, Param::DelayMix, Param::DelayPingPong },
             Param::DelayOn),
      reverb (c.params, "REVERB", { Param::ReverbSize, Param::ReverbDamp, Param::ReverbMix }, Param::ReverbOn)
{
    setLookAndFeel (&lnf);
    for (auto* comp : std::initializer_list<juce::Component*> { &header, &voice, &seq, &roll, &generator, &drive, &delay, &reverb })
        addAndMakeVisible (comp);
    addChildComponent (worldView);
    if (auto* q = seq.controlFor (Param::WorldQuantize))
        q->setTooltip ("When a world change lands while playing: immediately, on the next beat, or on the next bar");
    header.onViewChange = [this] (bool on) { setWorldView (on); };
    setSize (theme::baseWidth, theme::baseHeight);
    timerCallback();
    startTimerHz (30);
}

MainView::~MainView()
{
    setLookAndFeel (nullptr);
}

void MainView::setWorldView (bool on)
{
    worldView.setVisible (on);
    roll.setVisible (! on);
    header.setWorldView (on);
}

void MainView::applyWorld (World w, bool alt, int types)
{
    lnf.setWorld (w);
    sendLookAndFeelChange();

    const bool crusher = (types & 1) != 0, simpleDelay = (types & 2) != 0, plainReverb = (types & 4) != 0;
    auto caption = [] (ParamSection& s, Param p, const char* text)
    {
        if (auto* c = s.controlFor (p)) c->setCaption (text);
    };

    // each world's own effect for the three slots
    const char* ownDrive = w == World::Space ? "PHASER" : (w == World::Underground ? "GRIND" : "OVERDRIVE");
    const char* ownDelay = w == World::Space ? "SPACE ECHO" : (w == World::Underground ? "PING-PONG" : "DELAY");
    const char* ownReverb = w == World::Space ? "SHIMMER" : (w == World::Underground ? "CAVERN" : "REVERB");
    drive.setTypeChoice (Param::DriveType, { ownDrive, "BITCRUSHER" });
    delay.setTypeChoice (Param::DelayType, w == World::Classic ? juce::StringArray() : juce::StringArray { ownDelay, "SIMPLE DELAY" });
    reverb.setTypeChoice (Param::ReverbType, w == World::Classic ? juce::StringArray() : juce::StringArray { ownReverb, "REVERB" });

    const bool deep = w == World::Space && alt;
    const bool preStage = alt && w != World::Space;  // HOT / MOLTEN CORE add a copy before the filter

    // distortion slot
    if (crusher)
    {
        drive.setTitle (preStage ? "PRE-CRUSH" : "BITCRUSHER");
        caption (drive, Param::Drive, "BITS"); caption (drive, Param::DriveTone, "RATE"); caption (drive, Param::DriveMix, "MIX");
    }
    else switch (w)
    {
        case World::Classic:
            drive.setTitle (alt ? "PRE-DRIVE" : "OVERDRIVE");
            caption (drive, Param::Drive, "DRIVE"); caption (drive, Param::DriveTone, "TONE"); caption (drive, Param::DriveMix, "MIX");
            break;
        case World::Space:
            drive.setTitle (deep ? "2 PHASER" : "PHASER");
            caption (drive, Param::Drive, "RATE"); caption (drive, Param::DriveTone, "DEPTH"); caption (drive, Param::DriveMix, "MIX");
            break;
        case World::Underground:
            drive.setTitle (alt ? "PRE-GRIND" : "GRIND");
            caption (drive, Param::Drive, "GRIT"); caption (drive, Param::DriveTone, "WEIGHT"); caption (drive, Param::DriveMix, "MIX");
            break;
    }

    // delay slot
    if (w == World::Classic || simpleDelay)
    {
        delay.setTitle (deep ? "1 DELAY" : "DELAY");
        caption (delay, Param::DelayTone, "TONE"); caption (delay, Param::DelayPingPong, "Ping-Pong");
    }
    else if (w == World::Space)
    {
        delay.setTitle (deep ? "1 SPACE ECHO" : "SPACE ECHO");
        caption (delay, Param::DelayTone, "WOW"); caption (delay, Param::DelayPingPong, "Tape");
    }
    else
    {
        delay.setTitle ("PING-PONG");
        caption (delay, Param::DelayTone, "WIDTH"); caption (delay, Param::DelayPingPong, "Rock Damp");
    }

    // reverb slot
    if (w == World::Classic || plainReverb)
    {
        reverb.setTitle (deep ? "3 REVERB" : "REVERB");
        caption (reverb, Param::ReverbSize, "SIZE"); caption (reverb, Param::ReverbDamp, "DAMP");
    }
    else if (w == World::Space)
    {
        reverb.setTitle (deep ? "3 SHIMMER" : "SHIMMER");
        caption (reverb, Param::ReverbSize, "SIZE"); caption (reverb, Param::ReverbDamp, "SHIMMER");
    }
    else
    {
        reverb.setTitle ("CAVERN");
        caption (reverb, Param::ReverbSize, "DEPTH"); caption (reverb, Param::ReverbDamp, "DRIP");
    }

    if (auto* k = voice.controlFor (Param::Drift))
    {
        k->setCaption (w == World::Space ? "DRIFT" : (w == World::Underground ? "SUB" : "AGE"));
        k->setTooltip (w == World::Space ? "Drift: pitch and slow filter wander"
                       : (w == World::Underground ? "Sub: sine one octave down" : "Age: analogue pitch drift"));
    }
    repaint();
}

void MainView::timerCallback()
{
    const int w = ctx.live.world.load();
    bool alt = false;
    if (auto* v = ctx.params.getRawParameterValue (idOf (Param::WorldAlt)))
        alt = v->load() > 0.5f;

    auto choice = [this] (Param p)
    {
        auto* v = ctx.params.getRawParameterValue (idOf (p));
        return v != nullptr && v->load() > 0.5f ? 1 : 0;
    };
    const int types = choice (Param::DriveType) | (choice (Param::DelayType) << 1) | (choice (Param::ReverbType) << 2);

    if (w != shownWorld || alt != shownAlt || types != shownTypes)
    {
        if (shownWorld >= 0 && (w != shownWorld || alt != shownAlt)) flash = 1.0f;
        shownWorld = w;
        shownAlt = alt;
        shownTypes = types;
        applyWorld (worldFromIndex (w), alt, types);
    }
    if (flash > 0.0f)
    {
        flash = std::max (0.0f, flash - 0.12f);
        repaint();
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (theme::pal (*this).background);
}

void MainView::paintOverChildren (juce::Graphics& g)
{
    if (flash <= 0.0f) return;
    // a quick pixel wipe: horizontal bars sweeping down as the flash fades
    const auto& p = theme::pal (*this);
    const int bar = 8, n = getHeight() / bar;
    const int lit = (int) ((1.0f - flash) * (float) n);
    g.setColour (p.acid.withAlpha (0.35f * flash));
    for (int i = lit; i < n; i += 2)
        g.fillRect (0, i * bar, getWidth(), bar);
}

void MainView::resized()
{
    constexpr int gap = theme::gap;
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (HeaderBar::bandHeight + 36));
    r.reduce (gap, gap);

    auto top = r.removeFromTop (108);
    seq.setBounds (top.removeFromRight (330));
    top.removeFromRight (gap);
    voice.setBounds (top);
    r.removeFromTop (gap);

    auto bottom = r.removeFromBottom (176);
    r.removeFromBottom (gap);
    roll.setBounds (r);
    worldView.setBounds (r);

    generator.setBounds (bottom.removeFromLeft (440));
    bottom.removeFromLeft (gap);
    reverb.setBounds (bottom.removeFromRight (164));
    bottom.removeFromRight (gap);
    drive.setBounds (bottom.removeFromLeft (164));
    bottom.removeFromLeft (gap);
    delay.setBounds (bottom);
}

} // namespace acid::ui
