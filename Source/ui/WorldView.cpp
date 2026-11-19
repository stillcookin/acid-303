#include "WorldView.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

namespace acid::ui
{

namespace
{
    const char* const noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

WorldView::WorldView (UiContext c) : ctx (c)
{
    setOpaque (true);
    setInterceptsMouseClicks (false, false);
}

void WorldView::visibilityChanged()
{
    if (isVisible()) { lastTime = juce::Time::getMillisecondCounterHiRes(); startTimerHz (30); }
    else stopTimer();
}

float WorldView::param (Param p) const
{
    if (auto* v = ctx.params.getRawParameterValue (idOf (p)))
        return v->load();
    return specOf (p).defaultValue;
}

SceneFx WorldView::readFx() const
{
    SceneFx fx;
    fx.on = { param (Param::DriveOn) > 0.5f, param (Param::DelayOn) > 0.5f, param (Param::ReverbOn) > 0.5f };
    fx.v[0] = { param (Param::Drive), param (Param::DriveTone), param (Param::DriveMix) };
    fx.v[1] = { param (Param::DelayFeedback), param (Param::DelayTone), param (Param::DelayMix) };
    fx.v[2] = { param (Param::ReverbSize), param (Param::ReverbDamp), param (Param::ReverbMix) };
    fx.option = param (Param::DelayPingPong) > 0.5f;
    const auto& divs = delayDivisionsInBeats();
    const int idx = juce::jlimit (0, (int) divs.size() - 1, (int) std::lround (param (Param::DelayTime)));
    fx.delaySeconds = divs[(size_t) idx] * 60.0 / std::max (20.0, ctx.live.bpm.load());
    return fx;
}

void WorldView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.005, 0.1, (now - lastTime) * 0.001);
    lastTime = now;
    tick (dt);
    repaint();
}

void WorldView::tick (double dt)
{
    auto& s = state;
    s.frame++;
    s.world = worldFromIndex (ctx.live.world.load());
    s.alt = param (Param::WorldAlt) > 0.5f;
    s.square = std::lround (param (Param::Waveform)) == 1;

    const bool running = ctx.live.running.load();
    if (! running && wasRunning) { s.hist.clear(); s.ehist.clear(); s.e = 0.0f; }
    wasRunning = running;
    s.playing = running;
    s.step = running ? ctx.live.playingStep.load() : -1;
    if (running) s.clock += dt;

    // a new note: energy, target pitch, glide
    const unsigned serial = ctx.live.noteSerial.load();
    if (serial != lastSerial)
    {
        lastSerial = serial;
        if (running)
        {
            const bool acc = ctx.live.noteAccent.load();
            s.e = acc ? 1.0f : 0.62f;
            s.target = (float) juce::jlimit (0, 36, ctx.live.notePitch.load() - Pattern::baseNote);
            s.glide = ctx.live.noteSlide.load();
            s.lastTrig = s.clock;
            s.lastAccent = acc;
            if (acc) s.kick = 1.0f;
        }
    }

    const bool smooth = s.world != World::Classic;
    s.e *= (float) std::exp (-dt / 0.16);
    s.kick *= (float) std::exp (-dt / 0.25);
    s.ve += (s.e - s.ve) * (float) (1.0 - std::exp (-dt / (s.e > s.ve ? 0.05 : 0.45)));
    if (smooth)
    {
        // critically damped spring: eases into each note, softer on slides
        const float w = s.glide ? 5.0f : 11.0f;
        const float acc = w * w * (s.target - s.spos) - 2.0f * w * s.vel;
        s.vel += acc * (float) dt;
        s.spos += s.vel * (float) dt;
    }
    else
    {
        s.spos = s.glide ? s.spos + (s.target - s.spos) * 0.35f : s.target;
        s.vel = 0.0f;
    }
    const float tempo = (float) (ctx.live.bpm.load() / 120.0);
    const float targetSpeed = running ? (1.0f + (smooth ? s.ve : s.e) * 1.6f) * tempo : 0.15f;
    s.spd += (targetSpeed - s.spd) * (float) (1.0 - std::exp (-dt / (smooth ? 0.4 : 0.05)));
    s.scroll += s.spd * (float) dt * 16.0f;
    if (running)
    {
        s.hist.push_front (s.spos);
        s.ehist.push_front (smooth ? s.ve : s.e);
        while (s.hist.size() > 64) { s.hist.pop_back(); s.ehist.pop_back(); }
        s.dist += s.spd * (float) dt * 8.0f;
    }

    const auto& p = ctx.patterns.pattern();
    for (int i = 0; i < 16; ++i)
        s.usedSteps[(size_t) i] = i < p.length && p.effectiveGate (i) == Gate::Note;
}

void WorldView::paint (juce::Graphics& g)
{
    {
        juce::Graphics ig (frame);
        WorldScenes::drawWorld (ig, state, readFx());
    }
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (frame, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // HUD
    const auto& s = state;
    const int pitch = (int) std::lround (s.spos);
    const juce::String note = juce::String (noteNames[((pitch % 12) + 12) % 12]) + juce::String (pitch / 12 + 1);
    const auto& steps = ctx.patterns.pattern();
    juce::String stepFlags;
    if (s.step >= 0 && s.step < steps.length)
    {
        if (steps[s.step].accent) stepFlags << " ACC";
        if (steps[s.step].slide) stepFlags << " SLD";
    }
    const juce::String stepTxt = s.step >= 0 ? juce::String (s.step + 1).paddedLeft ('0', 2) : juce::String ("--");
    const auto& gs = ctx.patterns.generatorSettings();
    const juce::String scale = juce::String (scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, gs.scale)].name).toUpperCase();
    const int bpm = (int) std::lround (ctx.live.bpm.load());

    auto text = [&] (const juce::String& t, juce::Font f, juce::Colour c, juce::Rectangle<int> r, juce::Justification j)
    {
        g.setFont (f);
        g.setColour (c);
        g.drawText (t, r, j, false);
    };
    auto r = getLocalBounds().reduced (14, 10);
    const auto small = theme::titleFont (8.0f), label = theme::labelFont (22.0f);

    switch (s.world)
    {
        case World::Space:
            if (s.alt)
            {
                text ("CHAIN ECHO>PHASE>SHIMMER", small, juce::Colour (0xffffd166), r.removeFromTop (12), juce::Justification::topLeft);
                text ("DIST FROM HORIZON " + juce::String ((int) (1000 + s.dist)).paddedLeft ('0', 5) + " AU", label, juce::Colour (0xffff8a3d),
                      getLocalBounds().reduced (14, 8).removeFromTop (22), juce::Justification::topRight);
                text (s.playing ? "STEP " + stepTxt + " / " + note + stepFlags : juce::String ("PRESS PLAY TO ESCAPE"), label, juce::Colour (0xffd8e4ff),
                      getLocalBounds().reduced (14, 8).removeFromBottom (22), juce::Justification::bottomLeft);
            }
            else
            {
                text ("NAV / " + scale + " / " + rootNames()[(size_t) gs.root], small, juce::Colour (0xff6cf2ff), r.removeFromTop (12), juce::Justification::topLeft);
                text (juce::String (bpm) + " BPM / WARP " + (s.playing ? "1.0" : "0.0"), label, juce::Colour (0xff8796d6),
                      getLocalBounds().reduced (14, 8).removeFromTop (22), juce::Justification::topRight);
                text (s.playing ? "STEP " + stepTxt + " / " + note + stepFlags : juce::String ("PRESS PLAY TO LAUNCH"), label, juce::Colour (0xffd8e4ff),
                      getLocalBounds().reduced (14, 8).removeFromBottom (22), juce::Justification::bottomLeft);
            }
            break;
        case World::Underground:
            text (s.alt ? juce::String ("CHAIN GRIND>FILTER>PONG>CAVERN") : "CORE SURVEY / " + scale, small,
                  juce::Colour (s.alt ? 0xffffd27b : 0xffffb02e), r.removeFromTop (12), juce::Justification::topLeft);
            text (s.alt ? "CORE " + juce::String (5200 + (int) std::lround (s.ve * 600)) + " K"
                        : "DEPTH " + juce::String (303 + (int) (s.dist / 2)).paddedLeft ('0', 4) + " M",
                  label, juce::Colour (0xffffd27a), getLocalBounds().reduced (14, 8).removeFromTop (22), juce::Justification::topRight);
            text (s.playing ? (s.alt ? "DIAMOND " : "ORE ") + note + stepFlags : juce::String (s.alt ? "PRESS PLAY TO REACH THE CORE" : "PRESS PLAY TO DRILL"),
                  label, juce::Colour (0xfff3dfc2), getLocalBounds().reduced (14, 8).removeFromBottom (22), juce::Justification::bottomLeft);
            break;
        case World::Classic:
        {
            // inside the LCD: top-left mode, right panel step + big note
            const float sx = (float) getWidth() / WorldScenes::width, sy = (float) getHeight() / WorldScenes::height;
            auto lcd = juce::Rectangle<float> (11 * sx, 7 * sy, 223 * sx, 50 * sy).toNearestInt();
            auto left = lcd.reduced (10, 8);
            auto right = lcd.withLeft ((int) (166 * sx)).reduced (12, 8);
            const juce::Colour amber (s.alt ? 0xffffd27c : 0xffffb347);
            text (s.alt ? juce::String ("HOT / DRIVE>FILTER") : "SCOPE / " + juce::String (s.square ? "SQUARE" : "SAW"), small, amber, left, juce::Justification::topLeft);
            text ("STEP " + stepTxt + stepFlags, label, juce::Colour (0xffc98a3a), right.removeFromTop (22), juce::Justification::topRight);
            text (note, theme::labelFont (72.0f), amber, right, juce::Justification::centredRight);
            break;
        }
    }
}

} // namespace acid::ui
