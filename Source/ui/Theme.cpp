#include "Theme.h"
#include "RungsLookAndFeel.h"

#include <BinaryData.h>

#include <cmath>

namespace acid::ui::theme
{

namespace
{
    juce::Colour c (juce::uint32 rgb) { return juce::Colour (0xff000000u | rgb); }

    Palette classic()
    {
        Palette p;
        p.background = c (0x6e6a62); p.panel = c (0xc9c3b6); p.panelHi = c (0xe6e1d6); p.panelLo = c (0x9a9488); p.edge = c (0x24211d);
        p.raised = c (0xddd8cc); p.raisedHi = c (0xf4f1ea); p.raisedLo = c (0xa8a296);
        p.plate = c (0x3a3631); p.plateText = c (0xefeae0); p.plateDim = c (0xb5aea2);
        p.text = c (0x1f1d1a); p.textDim = c (0x4f4a42); p.title = c (0x1f1d1a);
        p.acid = c (0xd9632b); p.acidHi = c (0xf08a55); p.acidLo = c (0xa8461b); p.onAcid = c (0x1f1d1a);
        p.accent = c (0xb8323a); p.slide = c (0x3c6fa8); p.tie = c (0x6f8a3a); p.lock = c (0x6e58a0);
        p.rollBg = c (0x23211e); p.rowWhite = c (0x2e2b27); p.rowWhiteAlt = c (0x292622); p.rowBlack = c (0x24221f); p.rowBlackAlt = c (0x201e1b);
        p.rollLine = c (0x36332e); p.playhead = c (0x4a453d); p.keyWhite = c (0xc9c3b6); p.keyWhiteText = c (0x1f1d1a);
        p.laneOff = c (0x1c1a17); p.laneEdge = c (0x3d3933); p.rollDim = c (0xa39d92); p.rollText = c (0xefeae0);
        p.knobTrack = c (0x8c867a); p.knobBody = c (0x2a2724); p.knobHi = c (0x4a4540); p.knobPointer = c (0xefeae0);
        return p;
    }

    Palette space()
    {
        Palette p;
        p.background = c (0x03040b); p.panel = c (0x0d1230); p.panelHi = c (0x26306b); p.panelLo = c (0x05071a); p.edge = c (0x000000);
        p.raised = c (0x151b45); p.raisedHi = c (0x2f3c8c); p.raisedLo = c (0x080b22);
        p.plate = c (0x090c22); p.plateText = c (0xd8e4ff); p.plateDim = c (0x8796d6);
        p.text = c (0xd8e4ff); p.textDim = c (0x8796d6); p.title = c (0x6cf2ff);
        p.acid = c (0x6cf2ff); p.acidHi = c (0xb8faff); p.acidLo = c (0x2fb6c9); p.onAcid = c (0x05060f);
        p.accent = c (0xff5cc8); p.slide = c (0xb48cff); p.tie = c (0x4fb3c9); p.lock = c (0xffd166);
        p.rollBg = c (0x070a1c); p.rowWhite = c (0x0e1434); p.rowWhiteAlt = c (0x0b102b); p.rowBlack = c (0x090d24); p.rowBlackAlt = c (0x070a1d);
        p.rollLine = c (0x18204a); p.playhead = c (0x24327a); p.keyWhite = c (0x26306b); p.keyWhiteText = c (0xd8e4ff);
        p.laneOff = c (0x070a1c); p.laneEdge = c (0x222c66); p.rollDim = c (0x8796d6); p.rollText = c (0xd8e4ff);
        p.knobTrack = c (0x26306b); p.knobBody = c (0x1a2150); p.knobHi = c (0x34418f); p.knobPointer = c (0xffffff);
        return p;
    }

    Palette underground()
    {
        Palette p;
        p.background = c (0x0e0805); p.panel = c (0x2e1d12); p.panelHi = c (0x5a3a22); p.panelLo = c (0x170e08); p.edge = c (0x000000);
        p.raised = c (0x3c2717); p.raisedHi = c (0x6b4528); p.raisedLo = c (0x1d130c);
        p.plate = c (0x1d130c); p.plateText = c (0xf3dfc2); p.plateDim = c (0xc2a07a);
        p.text = c (0xf3dfc2); p.textDim = c (0xc2a07a); p.title = c (0xffb02e);
        p.acid = c (0xffb02e); p.acidHi = c (0xffd27a); p.acidLo = c (0xc27a12); p.onAcid = c (0x140c07);
        p.accent = c (0xff5a2e); p.slide = c (0x7fd1c7); p.tie = c (0xa8b04a); p.lock = c (0xc79bff);
        p.rollBg = c (0x1a100a); p.rowWhite = c (0x2a1b11); p.rowWhiteAlt = c (0x24170e); p.rowBlack = c (0x1f140c); p.rowBlackAlt = c (0x1b110a);
        p.rollLine = c (0x352216); p.playhead = c (0x4d3218); p.keyWhite = c (0x5a3a22); p.keyWhiteText = c (0xf3dfc2);
        p.laneOff = c (0x140c07); p.laneEdge = c (0x4a3020); p.rollDim = c (0xc2a07a); p.rollText = c (0xf3dfc2);
        p.knobTrack = c (0x5a3a22); p.knobBody = c (0x1d130c); p.knobHi = c (0x4a3020); p.knobPointer = c (0xffd27a);
        return p;
    }

    juce::Typeface::Ptr typeface (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }
}

const Palette& palette (World w)
{
    static const Palette pals[] { classic(), space(), underground() };
    return pals[indexOf (w)];
}

const Palette& pal (const juce::Component& comp)
{
    if (auto* lnf = dynamic_cast<const RungsLookAndFeel*> (&comp.getLookAndFeel()))
        return lnf->getPalette();
    return palette (World::Classic);
}

juce::Font titleFont (float height)
{
    static auto tf = typeface (BinaryData::PressStart2PRegular_ttf, BinaryData::PressStart2PRegular_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

juce::Font labelFont (float height)
{
    static auto tf = typeface (BinaryData::VT323Regular_ttf, BinaryData::VT323Regular_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

void pixelPanel (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour fill, juce::Colour hi, juce::Colour lo, juce::Colour edgeColour)
{
    constexpr int b = bevel;
    // notched outline: four bars, corners left open
    g.setColour (edgeColour);
    g.fillRect (r.getX() + b, r.getY(), r.getWidth() - 2 * b, b);
    g.fillRect (r.getX() + b, r.getBottom() - b, r.getWidth() - 2 * b, b);
    g.fillRect (r.getX(), r.getY() + b, b, r.getHeight() - 2 * b);
    g.fillRect (r.getRight() - b, r.getY() + b, b, r.getHeight() - 2 * b);

    auto in = r.reduced (b);
    g.setColour (fill);
    g.fillRect (in);
    g.setColour (hi);
    g.fillRect (in.getX(), in.getY(), in.getWidth(), b);
    g.fillRect (in.getX(), in.getY(), b, in.getHeight());
    g.setColour (lo);
    g.fillRect (in.getX(), in.getBottom() - b, in.getWidth(), b);
    g.fillRect (in.getRight() - b, in.getY(), b, in.getHeight());
}

void pixelKnob (juce::Graphics& g, juce::Rectangle<float> area, float v, const Palette& p, bool enabled)
{
    constexpr int N = 14;
    const float side = std::floor (std::min (area.getWidth(), area.getHeight()) / N) * N;
    if (side < N) return;
    const float cell = side / N;
    const auto origin = area.withSizeKeepingCentre (side, side).getTopLeft();

    const float av = -135.0f + 270.0f * juce::jlimit (0.0f, 1.0f, v);
    bool ptr[N][N] {};
    for (float rr = 0.6f; rr <= 4.7f; rr += 0.25f)
    {
        const float t = (av - 90.0f) * juce::MathConstants<float>::pi / 180.0f;
        const int px = (int) std::floor (7.0f + rr * std::cos (t)), py = (int) std::floor (7.0f + rr * std::sin (t));
        if (px >= 0 && px < N && py >= 0 && py < N) ptr[py][px] = true;
    }
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
        {
            const float dx = (float) x + 0.5f - 7.0f, dy = (float) y + 0.5f - 7.0f;
            const float d = std::hypot (dx, dy);
            const float a = std::atan2 (dx, -dy) * 180.0f / juce::MathConstants<float>::pi;
            juce::Colour col;
            if (d > 5.4f && d <= 6.95f)
            {
                if (std::abs (a) > 140.0f) continue;
                col = (a <= av && enabled) ? p.acid : p.knobTrack;
            }
            else if (d <= 4.95f)
                col = ptr[y][x] ? p.knobPointer : (dx + dy < -2.6f ? p.knobHi : p.knobBody);
            else
                continue;
            g.setColour (col);
            g.fillRect (juce::Rectangle<float> (origin.x + (float) x * cell, origin.y + (float) y * cell, cell, cell));
        }
}

} // namespace acid::ui::theme
