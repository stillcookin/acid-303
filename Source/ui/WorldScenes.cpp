// Pixel scenes for the World view and the header strip.
// Ported from the design canvas: every number here was tuned there.
#include "WorldScenes.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace acid::ui
{

namespace
{
    using u32 = juce::uint32;
    constexpr double pi = 3.14159265358979323846;

    /** The canvas's LCG: same seeds give the same star fields. */
    struct Rng
    {
        explicit Rng (double seed) : s (std::fmod (seed * 7919.0, 233280.0)) {}
        double operator()() { s = std::fmod (s * 9301.0 + 49297.0, 233280.0); return s / 233280.0; }
        double s;
    };

    double hash2 (int x, int y)
    {
        u32 n = (u32) x * 374761393u + (u32) y * 668265263u;
        n = (n ^ (n >> 13)) * 1274126177u;
        n = n ^ (n >> 16);
        return (double) n / 4294967296.0;
    }

    double rnd (double v) { return std::floor (v + 0.5); }

    struct Pix
    {
        juce::Graphics& g;
        int W, H;
        double ox = 0.0, oy = 0.0;

        void raw (u32 rgb, double x, double y, double w = 1.0, double h = 1.0)
        {
            if (w == 0.0) w = 1.0;
            if (h == 0.0) h = 1.0;
            if (w < 0.0 || h < 0.0) return;
            const double X = rnd (x), Y = rnd (y);
            const double ww = std::max (1.0, rnd (w)), hh = std::max (1.0, rnd (h));
            if (X >= W || Y >= H || X + ww <= 0 || Y + hh <= 0) return;
            g.setColour (juce::Colour (0xff000000u | rgb));
            g.fillRect ((float) X, (float) Y, (float) ww, (float) hh);
        }
        void put (u32 c, double x, double y, double w = 1.0, double h = 1.0) { raw (c, x + ox, y + oy, w, h); }
    };

    void disc (Pix& p, double cx, double cy, double r, u32 c, double ry = 1.0)
    {
        if (r <= 0.0) return;
        for (int y = (int) std::floor (cy - r * ry); y <= cy + r * ry; ++y)
        {
            const double dy = ((double) y + 0.5 - cy) / ry;
            if (std::abs (dy) > r) continue;
            const double hw = std::sqrt (r * r - dy * dy);
            p.put (c, rnd (cx - hw), y, std::max (1.0, rnd (hw * 2.0)), 1);
        }
    }

    void ring (Pix& p, double cx, double cy, double r, u32 c)
    {
        const int n = std::max (8, (int) rnd (r * 6.0));
        for (int i = 0; i < n; ++i)
        {
            const double a = (double) i / n * pi * 2.0;
            p.put (c, cx + std::cos (a) * r, cy + std::sin (a) * r * 0.8);
        }
    }

    const char* const SHIP[] { ".##......", "######...", "#########", "######...", ".##......" };
    const char* const DRILL[] { ".#####.", "#######", "#######", ".#####.", "..###..", "..###..", "...#..." };

    template <size_t N>
    void sprite (Pix& p, const char* const (&rows)[N], double x, double y, u32 c, u32 c2 = 0, int hx = -1, int hy = -1)
    {
        for (size_t j = 0; j < N; ++j)
            for (int i = 0; rows[j][i] != 0; ++i)
                if (rows[j][i] == '#')
                    p.put ((c2 != 0 && i == hx && (int) j == hy) ? c2 : c, x + i, y + (double) j);
    }

    double wrap (double v, double m) { return std::fmod (std::fmod (v, m) + m, m); }

    // ------------------------------------------------------------------ Space
    void space (Pix& P, const SceneState& s, const SceneFx& fx)
    {
        const int W = WorldScenes::width, H = WorldScenes::height;
        const double sc = s.scroll, t = s.clock, E = s.ve, age = s.clock - s.lastTrig;
        const double rate = fx.v[0][0], depth = fx.v[0][1];
        const double fb = fx.v[1][0], wow = fx.v[1][1], smear = fx.option ? 0.6 : 0.2;
        const double size = fx.v[2][0], shAmt = fx.v[2][1];

        P.put (0x03040b, 0, 0, W, H);
        if (fx.on[2])
        {
            Rng rz (77);
            const int n = (int) rnd (size * 90.0);
            for (int i = 0; i < n; ++i) { const double x0 = rz() * W, y = std::floor (rz() * H); P.put (0x161c48, wrap (x0 - sc * 0.05, W), y); }
        }
        Rng r0 (11);
        const double sway = fx.on[0] ? (1.0 + depth * 4.0) : 0.0, sw = (0.15 + rate * 1.2) * pi * 2.0;
        for (int i = 0; i < 170; ++i)
        {
            const double cx = 140 + (r0() - 0.5) * 120, cy = rnd (22 + (r0() - 0.5) * 36), hb = r0();
            const double x = rnd (wrap (cx - sc * 0.08 + std::sin (t * sw + cy * 0.14) * sway, W));
            if (((int) x + (int) cy) % 2 == 0) P.put (hb < E * 0.9 ? 0x1f2770 : 0x11164a, x, cy);
        }
        Rng r (42);
        for (int i = 0; i < 70; ++i) { const double x0 = r() * W, y = std::floor (r() * H); P.put (0x2c3a86, wrap (x0 - sc * 0.3, W), y); }
        const double smearK = fx.on[1] ? 1.0 + smear * 1.6 : 1.0;
        const int streak = s.playing ? (int) rnd ((1.0 + E * 5.0) * smearK) : 0;
        for (int i = 0; i < 34; ++i)
        {
            const double x0 = r() * W, y = std::floor (r() * H), b = r();
            const double x = wrap (x0 - sc * 1.4, W);
            P.put (b > 0.7 ? 0xf4f7ff : 0x8796d6, x, y);
            if (streak > 0 && b > 0.45)
            {
                const int h1 = (int) std::ceil (streak / 2.0);
                P.put (b > 0.7 ? 0x3a4aa8 : 0x1d2660, x + 1, y, h1, 1);
                if (fx.on[1]) P.put (0x161c48, x + 1 + h1, y, streak - h1 + 1, 1);
            }
        }
        // distant planets and tumbling asteroids
        struct Body { double x0, y, r; u32 dk, lt; double par; };
        const Body bodies[] { { 40, 14, 4, 0x1b5f75, 0x2fb6c9, 0.10 }, { 118, 52, 3, 0xb58a2c, 0xffd166, 0.14 }, { 168, 9, 2, 0x5a3a8a, 0xb48cff, 0.07 } };
        for (const auto& b : bodies)
        {
            const double x = wrap (b.x0 - sc * b.par, W + 20) - 10;
            disc (P, x, b.y, b.r, b.dk);
            disc (P, x - b.r * 0.35, b.y - b.r * 0.35, b.r * 0.6, b.lt);
        }
        Rng ra (31);
        for (int i = 0; i < 9; ++i)
        {
            const double x0 = ra() * (W + 30), y = 4 + ra() * 60, sz = 1 + std::floor (ra() * 3), par = 0.45 + ra() * 0.5;
            const int tum = (int) std::floor (std::fmod (t * (1 + ra() * 2) + i, 2.0));
            const double x = wrap (x0 - sc * par, W + 30) - 15;
            P.put (0x4a4560, x, y, sz + 1, sz);
            P.put (0x8a849c, x + tum, y, std::max (1.0, sz - tum), 1);
            if (sz > 1) P.put (0x2e2a3e, x + sz - tum, y + sz - 1, 1, 1);
        }
        // hero planet with a tilted ring in front of and behind it
        const double PX = 192, PY = 34, pr = 19 + E * 1.6, rx = 36, ry = 7;
        auto ringPt = [&] (double a) { return juce::Point<double> (PX + std::cos (a) * rx, PY + std::sin (a) * ry - std::cos (a) * 5); };
        auto ringCol = [&] (double a) -> u32
        {
            if (! fx.on[2]) return 0xc9a24a;
            const double tr = wrap (a / (pi * 2) - t * 0.35, 1.0);
            return tr < 0.06 + shAmt * 0.18 ? 0xfff1c4 : 0xffd166;
        };
        for (int i = 0; i < 160; ++i) { const double a = i / 160.0 * pi * 2; if (std::sin (a) < 0) { auto q = ringPt (a); P.put (0x8a6a2c, q.x, q.y); } }
        disc (P, PX, PY, pr + 3, 0x2a0f30);
        disc (P, PX, PY, pr, 0x7a2266);
        disc (P, PX - 2, PY - 2, pr - 2, 0xc23a96);
        for (int b = -2; b <= 2; ++b)
        {
            const double hw = std::sqrt (std::max (0.0, (pr - 3) * (pr - 3) - (b * 5.0) * (b * 5.0)));
            P.put (0xa82f82, PX - hw, PY + b * 5, hw * 2, 1);
        }
        disc (P, PX - 5, PY - 5, pr * 0.55, 0xff5cc8);
        disc (P, PX - 8, PY - 8, 2 + E * 5, 0xff8fd9);
        for (int i = 0; i < 160; ++i) { const double a = i / 160.0 * pi * 2; if (std::sin (a) >= 0) { auto q = ringPt (a); P.put (ringCol (a), q.x, q.y); P.put (0x8a6a2c, q.x, q.y + 1); } }
        if (fx.on[2])
        {
            Rng rs (5);
            const int n = (int) rnd (4 + shAmt * 18 + E * 8);
            const double climb = 3 + shAmt * 14 + E * 4;
            for (int i = 0; i < n; ++i)
            {
                const double a = rs() * pi, sp = 3 + rs() * 6, off = rs() * climb;
                const auto q = ringPt (a);
                const double rise = std::fmod (t * sp + off, climb);
                P.put (rise < climb * 0.5 ? 0xfff1c5 : 0xffd167, q.x, q.y - rise);
            }
        }
        auto py = [] (double p) { return 60 - std::min (36.0, std::max (0.0, p)) * 1.4; };
        const double SX = 64;
        for (size_t j = 0; j < s.hist.size(); ++j) P.put (j < 14 ? 0x2fb6c9 : 0x1b5f75, SX - 5 - (double) j * 1.3, py (s.hist[j]));
        if (fx.on[1])
        {
            const int n = 1 + (int) rnd (fb * 3);
            const int D = (int) rnd (fx.delaySeconds * 30);
            const u32 G[] { 0x8796d6, 0x3a4aa8, 0x26306b, 0x1d2660 };
            for (int k = 1; k <= n && k <= 4; ++k)
            {
                const size_t j = (size_t) (k * D);
                if (j >= s.hist.size()) continue;
                sprite (P, SHIP, SX - 4 - k * 17, py (s.hist[j]) - 2 + std::sin (t * 2.4 + k * 1.7) * wow * 2.5, G[k - 1]);
            }
        }
        const double sy = rnd (py (s.spos));
        const int fl = 2 + (int) rnd (E * 14);
        P.put (0xff5cc8, SX - 5 - fl, sy, fl, 1);
        P.put (0xffd166, SX - 5 - std::ceil (fl / 2.0), sy, std::ceil (fl / 2.0), 1);
        const int w2 = (int) rnd (fl * 0.55 * std::min (1.0, E * 1.6));
        if (w2 > 0) { P.put (0xff5cc8, SX - 3 - w2, sy - 1, w2, 1); P.put (0xff5cc8, SX - 3 - w2, sy + 1, w2, 1); }
        sprite (P, SHIP, SX - 4, sy - 2, 0xd8e4ff, 0x6cf2fe, 6, 2);
        if (std::abs (s.vel) > 4) P.put (0xd8e4ff, SX + 4, sy + (s.vel > 0 ? -1 : 1));
        if (s.playing && age < 0.45) ring (P, SX, sy, 3 + age * 38, s.lastAccent ? 0xff5cc9 : 0x6cf2fd);
    }

    // ------------------------------------------------------------------ Deep Space
    void deepSpace (Pix& P, const SceneState& s, const SceneFx& fx)
    {
        const int W = WorldScenes::width, H = WorldScenes::height;
        const double E = s.ve, age = s.clock - s.lastTrig, tt = s.frame / 30.0;
        const double rate = fx.v[0][0], fb = fx.v[1][0], wow = fx.v[1][1], smear = fx.option ? 0.6 : 0.2;
        const double size = fx.v[2][0], shAmt = fx.v[2][1], HX = 40, HY = 34;

        P.put (0x020208, 0, 0, W, H);
        Rng r0 (77);
        for (int i = 0; i < 60; ++i) { const double x0 = r0() * W, y = std::floor (r0() * H); P.put (0x161c48, wrap (x0 - s.scroll * 0.05, W), y); }
        const double pull = 0.05 + E * 0.05 + (fx.on[1] ? smear * 0.03 : 0.0);
        Rng rb (9);
        for (int i = 0; i < 80; ++i)
        {
            const double seed = rb(), a0 = rb() * pi * 2, kind = rb(), R0 = 110 + rb() * 140;
            const double p = std::fmod (tt * pull * (0.6 + seed) + seed, 1.0);
            const double rad = 13 + R0 * std::pow (1 - p, 1.6) + 2, ang = a0 + p * p * 7;
            const double x = HX + std::cos (ang) * rad, y = HY + std::sin (ang) * rad * 0.55;
            if (rad < 15) continue;
            const bool hot = rad < 40;
            if (kind > 0.94) disc (P, x, y, std::max (1.0, rad / 45), hot ? 0xff8a3e : (kind > 0.97 ? 0x2fb6c9 : 0xb48cff));
            else if (kind > 0.8) { P.put (hot ? 0xffd27b : 0x8a849c, x, y, 2, 1); P.put (0x4a4560, x + 1, y + 1); }
            else
            {
                P.put (hot ? 0xfff1c6 : (kind > 0.45 ? 0x8796d6 : 0x3a4aa8), x, y);
                if (rad < 64)
                {
                    const double dx = (HX - x) / rad, dy = (HY - y) / rad;
                    for (int k = 1; k <= 3; ++k) P.put (hot ? 0xff8a3e : 0x2c3a86, x + dx * k * 1.6, y + dy * k * 0.9);
                }
            }
        }
        disc (P, HX, HY, 30, 0x0b0718, 0.8);
        disc (P, HX, HY, 22, 0x170a24, 0.85);
        const double rx = 44 + (fx.on[2] ? size * 8 : 0), ry = 9;
        const double spin = tt * (0.5 + (fx.on[0] ? rate * 1.5 : 0)) + E * 0.6, glow = E + (fx.on[2] ? shAmt * 0.4 : 0);
        auto disk = [&] (double a, double w, bool front) -> u32
        {
            const double tr = wrap ((a + w * 0.7) / (pi * 2) - spin, 1.0);
            const int band = (int) std::floor (tr * 9) % 3;
            return band == 0 ? (glow > 0.6 ? 0xfff7e0 : 0xffd166) : (band == 1 ? 0xff8a3d : (front ? 0xc23a96 : 0x7a2266));
        };
        auto drawDisk = [&] (bool front)
        {
            for (int i = 0; i < 240; ++i)
            {
                const double a = i / 240.0 * pi * 2;
                if ((std::sin (a) >= 0) != front) continue;
                for (double rr = rx; rr >= 15; rr -= 1.5)
                    P.put (disk (a, (rx - rr) / 6, front), HX + std::cos (a) * rr, HY + std::sin (a) * ry * (rr / rx));
            }
        };
        drawDisk (false);
        for (int w = 0; w < 3; ++w)
            for (int i = 0; i <= 80; ++i)
            {
                const double a = pi + i / 80.0 * pi;
                P.put (w == 1 ? (glow > 0.5 ? 0xffd168 : 0xff8a3f) : 0xa8462a, HX + std::cos (a) * (17 + w), HY + std::sin (a) * (16 + w));
            }
        disc (P, HX, HY, 15.5 + E * 0.8, 0xffe7a1);
        disc (P, HX, HY, 14, 0xfff7e1);
        disc (P, HX, HY, 13, 0x000000);
        drawDisk (true);
        if (s.playing && age < 0.5) ring (P, HX, HY, 18 + age * 44, s.lastAccent ? 0xff5cc9 : 0x6cf2fd);

        const double SX = rnd (150 + (E - 0.3) * 14);
        auto py = [] (double p) { return 60 - std::min (36.0, std::max (0.0, p)) * 1.4; };
        for (size_t j = 0; j < s.hist.size(); ++j)
        {
            const double y0 = py (s.hist[j]);
            P.put (j < 14 ? 0x2fb6c9 : 0x1b5f75, SX - 5 - (double) j * 1.6, y0 + (HY - y0) * std::min (1.0, j / 70.0));
        }
        if (fx.on[1])
        {
            const int n = 1 + (int) rnd (fb * 3), D = (int) rnd (fx.delaySeconds * 30);
            const u32 G[] { 0x8796d6, 0x3a4aa8, 0x26306b, 0x1d2660 };
            for (int k = 1; k <= n && k <= 4; ++k)
            {
                const size_t j = (size_t) (k * D);
                if (j >= s.hist.size()) continue;
                const double gx = SX - 4 - k * 20, gy = py (s.hist[j]);
                sprite (P, SHIP, gx, gy - 2 + (HY - gy) * k * 0.12 + std::sin (tt * 2.4 + k) * wow * 2.5, G[k - 1]);
            }
        }
        const double sy = rnd (py (s.spos));
        const int fl = 4 + (int) rnd (E * 18);
        P.put (0xff5cc8, SX - 5 - fl, sy, fl, 1);
        P.put (0xffd166, SX - 5 - std::ceil (fl / 2.0), sy, std::ceil (fl / 2.0), 1);
        const int w2 = (int) rnd (fl * 0.6);
        P.put (0xff5cc8, SX - 3 - w2, sy - 1, w2, 1);
        P.put (0xff5cc8, SX - 3 - w2, sy + 1, w2, 1);
        sprite (P, SHIP, SX - 4, sy - 2, 0xd8e4ff, 0x6cf2fe, 6, 2);
    }

    // ------------------------------------------------------------------ Underground / Molten Core
    void underground (juce::Graphics& g, const SceneState& s, const SceneFx& fx)
    {
        const int W = WorldScenes::width, H = WorldScenes::height;
        const double t = s.clock, E = s.ve, age = s.clock - s.lastTrig;
        const double grit = fx.v[0][0], sub = fx.v[0][1], fb = fx.v[1][0], width = fx.v[1][1];
        const double cdepth = fx.v[2][0], drip = fx.v[2][1];
        const bool MC = s.alt;
        const double tt = s.frame / 30.0, shakeK = MC ? 2.6 : 1.6;

        Pix P { g, W, H };
        P.ox = rnd (s.kick * shakeK * std::sin (t * 42) + (fx.on[0] && s.playing ? std::sin (t * 71) * grit * (MC ? 1.4 : 0.9) : 0.0));
        P.oy = fx.on[0] && s.playing ? rnd (std::sin (t * pi * 2 * 1.1) * sub * 1.6) : 0.0;
        const u32 bands[4] { MC ? 0x3a1a10u : 0x3a2a20u, MC ? 0x4a2214u : 0x4a3424u, MC ? 0x2a120cu : 0x2e2018u, MC ? 0x5a2a16u : 0x5a3a22u };
        const int depth = (int) std::floor (s.scroll * 2);
        const int DY = 22;

        P.raw (MC ? 0x2a120c : 0x2e2018, 0, 0, W, H);
        for (int y = -2; y < H + 2; ++y) { const int d = y + depth; P.put (bands[(((int) std::floor (d / 13.0) % 4) + 4) % 4], -2, y, W + 4, 1); }
        for (int y = 0; y < H; ++y)
        {
            const int d = y + depth;
            for (int x = 0; x < W; ++x)
            {
                const double h = hash2 (x, d);
                if (h < 0.03) P.put (MC ? 0x7a3a22 : 0x6b5242, x, y);
                else if (h < 0.045) P.put (0x1a100a, x, y);
                else if (h > 0.9965)
                {
                    if (MC)
                    {
                        P.put (hash2 (x + 7, d) < E ? 0xffffff : 0xbff6ff, x, y);
                        if (h > 0.9991)
                        {
                            P.put (0x7fe3ff, x - 1, y); P.put (0x7fe3ff, x + 1, y); P.put (0x7fe3ff, x, y - 1); P.put (0x7fe3ff, x, y + 1);
                            if (hash2 (x, s.frame >> 2) < 0.25 + E * 0.4) { P.put (0xfffffe, x - 2, y); P.put (0xfffffe, x + 2, y); P.put (0xfffffe, x, y - 2); }
                        }
                    }
                    else P.put (hash2 (x + 7, d) < E ? 0xd8fff9 : (hash2 (x + 3, d) < E + 0.35 ? 0x7fd1c7 : 0x2f6b64), x, y);
                }
                else if (MC && (x * 3 + d * 5) % 37 == 0 && h < 0.5) P.put (hash2 (x + 11, d) < E ? 0xffb02f : 0xc2361b, x, y);
            }
        }
        // caves with stalactites and stalagmites
        struct Tip { double x, y0, floor; };
        std::vector<Tip> tips;
        const int k0 = (int) std::floor (depth / 34.0) - 1;
        for (int k = k0; k < k0 + 4; ++k)
        {
            if (! fx.on[2] && hash2 (k, 98) > 0.55) continue;
            const double cx = 20 + hash2 (k, 99) * 205, cy = k * 34 + 17 - depth;
            const double rx = 8 + (fx.on[2] ? cdepth * 22 : 5), ry = 4 + (fx.on[2] ? cdepth * 6 : 1.5);
            disc (P, cx, cy, rx + 1, MC ? 0x4a1d10 : 0x2a1b11, ry / rx);
            disc (P, cx, cy, rx, MC ? 0x2a0f08 : 0x1f140c, ry / rx);
            disc (P, cx, cy + 1, rx - 2, MC ? 0x1f0a05 : 0x140c07, ry / rx);
            const int ns = 3 + (int) std::floor (hash2 (k, 5) * 3);
            for (int i = 0; i < ns; ++i)
            {
                const double fxp = (i + 0.5) / ns - 0.5 + (hash2 (k, i + 20) - 0.5) * 0.12;
                const double x = rnd (cx + fxp * rx * 1.7), half = ry * std::sqrt (std::max (0.0, 1 - (fxp * 1.7) * (fxp * 1.7)));
                const int lu = 3 + (int) std::floor (hash2 (k, i + 40) * (ry * 1.1)), ld = 2 + (int) std::floor (hash2 (k, i + 60) * (ry * 0.8));
                const double top = rnd (cy - half), bot = rnd (cy + 1 + half);
                for (int j = 0; j < lu; ++j)
                {
                    const int w = j < lu * 0.3 ? 4 : (j < lu * 0.6 ? 3 : (j < lu * 0.85 ? 2 : 1));
                    const double x0 = x - std::floor (w / 2.0);
                    P.put (MC ? 0x2b2433 : 0x8a6a4a, x0, top + j, w, 1);
                    P.put (MC ? (j == lu - 1 ? 0xff5a2f : 0x4a3f55) : 0xb8946c, x0, top + j);
                }
                tips.push_back ({ x, top + lu, cy + half });
                for (int j = 0; j < ld; ++j)
                {
                    const int w = j < ld * 0.35 ? 4 : (j < ld * 0.65 ? 3 : (j < ld * 0.85 ? 2 : 1));
                    const double x0 = x - std::floor (w / 2.0) + 1;
                    P.put (MC ? 0x2b2433 : 0x7a5a40, x0, bot - 1 - j, w, 1);
                    P.put (MC ? 0x4a3f55 : 0xa07c58, x0, bot - 1 - j);
                }
            }
        }
        if (fx.on[2])
        {
            const size_t nd = (size_t) (1 + rnd (drip * 5)) * 2;
            for (size_t i = 0; i < std::min (nd, tips.size()); ++i)
            {
                const auto& tp = tips[i];
                const double span = std::max (3.0, tp.floor - tp.y0);
                const double fall = std::fmod (t * (9 + hash2 ((int) i, 4) * 7) + hash2 ((int) i, 9) * span, span);
                const u32 dc = MC ? 0xff8a3e : 0x7fd1c6;
                P.put (dc, tp.x, tp.y0 + fall);
                if (fall < 1.2) P.put (dc, tp.x, tp.y0);
            }
        }
        // lava pockets
        const int L0 = (int) std::floor (depth / 46.0) - 1;
        for (int k = L0; k < L0 + 3; ++k)
        {
            if (hash2 (k, 55) > 0.65) continue;
            const double lx = 16 + hash2 (k, 56) * 213, ly = k * 46 + 31 - depth, lr = 8 + hash2 (k, 57) * 8;
            disc (P, lx, ly, lr + 3, 0x4a1d10, 0.45);
            disc (P, lx, ly + 1, lr, 0xc2361a, 0.32);
            disc (P, lx, ly + 1, lr - 2, E > 0.35 ? 0xff5a2e : 0xe0441f, 0.26);
            disc (P, lx, ly + 1, (lr - 4) * (0.4 + E * 0.6), 0xffb02f, 0.22);
            P.put (0x3a1a10, lx - lr, ly - 1, lr * 2, 1);
            Rng rb (k * 13 + 1);
            for (int i = 0; i < 3; ++i)
            {
                const double bx = lx + (rb() - 0.5) * lr * 1.4, per = 1.2 + rb();
                const double ph2 = std::fmod (t + rb() * per, per) / per;
                P.put (ph2 < 0.6 ? 0xff8a3d : 0xffd27b, bx, ly - 1 - ph2 * 4);
            }
            if (s.playing && age < 0.7 && (s.lastAccent || E > 0.55))
            {
                Rng rf (std::floor (s.lastTrig * 1000) + k * 7);
                const int n = (int) rnd ((s.lastAccent ? 12 : 5) * (0.6 + E));
                for (int i = 0; i < n; ++i)
                {
                    const double vx = (rf() - 0.5) * 18, vy = 22 + rf() * 26;
                    const double x = lx + vx * age, y = ly - 1 - vy * age + 48 * age * age;
                    if (y <= ly) P.put (age < 0.3 ? 0xffd27b : 0xff5a2f, x, y);
                }
            }
        }
        if (MC)
        {
            const u32 SH[] { 0xfff4dd, 0xffd27b, 0xff8a3e, 0xff5a2f, 0xc2361b, 0x8a2412 };
            std::vector<double> surf ((size_t) W);
            for (int x = 0; x < W; ++x)
            {
                const double sy = rnd (51 + std::sin (x * 0.18 + tt * 2.2) * (1 + E * 1.6) + std::sin (x * 0.05 - tt * 1.1));
                surf[(size_t) x] = sy;
                if (hash2 (x, s.frame >> 1) < 0.25 + E * 0.35) P.put (0x5a2a14, x, sy - 2 - std::floor (hash2 (x, 3) * 2));
                P.put (SH[1], x, sy); P.put (SH[2], x, sy + 1, 1, 2); P.put (SH[3], x, sy + 3, 1, 4); P.put (SH[4], x, sy + 7, 1, 5); P.put (SH[5], x, sy + 12, 1, H);
                if (hash2 (x, (int) std::floor (tt * 3)) < 0.05 + E * 0.1) P.put (SH[0], x, sy + 1);
            }
            const int fountains[] { 36, 92, 158, 214 };
            for (int fi = 0; fi < 4; ++fi)
            {
                const int fxp = fountains[fi];
                const double base = surf[(size_t) fxp];
                Rng rf (fi * 31 + 5);
                const int n = 12 + (int) rnd (E * 16);
                for (int i = 0; i < n; ++i)
                {
                    const double T = 0.8 + rf() * 0.6, ph = std::fmod (tt + rf() * T, T);
                    const double v = (18 + rf() * 16) * (0.6 + E * 1.4), vx = (rf() - 0.5) * 14;
                    const double y = base - v * ph + 34 * ph * ph;
                    if (y > base) continue;
                    P.put (SH[std::min (5, (int) std::floor (ph / T * 6))], fxp + vx * ph, y, 2, i % 2 ? 2 : 1);
                }
            }
            if (s.playing && s.lastAccent && age < 0.9)
            {
                Rng rf (std::floor (s.lastTrig * 1000) + 99);
                const int fxp = fountains[(int) std::floor (rf() * 4) & 3];
                const double base = surf[(size_t) fxp];
                for (int i = 0; i < 22; ++i)
                {
                    const double vx = (rf() - 0.5) * 26, vy = 30 + rf() * 32;
                    const double x = fxp + vx * age, y = base - vy * age + 40 * age * age;
                    if (y <= base) P.put (SH[std::min (5, (int) std::floor (age * 6.5))], x, y, 2, 2);
                }
            }
        }
        // the drill and its tunnel
        auto px = [] (double p) { return 34 + std::min (32.0, std::max (0.0, p)) * 5.6; };
        const bool flick = fx.on[0] && s.playing && hash2 (s.frame, 3) < grit * 0.35;
        const u32 lamp = flick ? 0x5a3a10 : (E > 0.45 ? 0xffd27a : 0xc27a12);
        if (s.hist.empty())
        {
            P.put (0x0e0805, px (s.spos) - 3, 0, 7, DY);
            for (int y = 3; y < DY; y += 6) P.put (lamp, px (s.spos) - 4, y);
        }
        const double stepY = std::max (1.0, s.spd * 2.1);
        for (size_t j = 0; j < s.hist.size(); ++j)
        {
            const double y0 = DY - (double) j * stepY;
            if (y0 < -4) break;
            const double x = px (s.hist[j]);
            P.put (0x0e0805, x - 3, y0 - 3, 7, std::ceil (stepY) + 3);
            if (j % 5 == 0) { P.put (lamp, x - 4, y0 - 2); P.put (lamp, x + 4, y0 - 1); }
        }
        const double dx = rnd (px (s.spos));
        if (fx.on[1] && s.playing)
        {
            const int n = std::max (1, 1 + (int) rnd (fb * 4) - (fx.option ? 1 : 0));
            const double T = std::max (0.05, fx.delaySeconds), wd = 10 + width * 70;
            const int k = (int) std::floor (age / T);
            const u32 C1[] { 0xffd27a, 0xc27a12, 0x6b4012, 0x3a2410 };
            const u32 C2[] { 0xfff1c4, 0xffd27a, 0xffb02e, 0xc27a12, 0x7a4a12 };
            const u32* C = fx.option ? C1 : C2;
            const int nc = fx.option ? 4 : 5;
            if (k >= 0 && k < n && k < nc)
            {
                const double fr = std::fmod (age, T) / T;
                const double from = k == 0 ? dx : (k % 2 ? dx - wd : dx + wd), to = k % 2 == 0 ? dx - wd : dx + wd;
                const double x = from + (to - from) * fr;
                P.put (C[k], x - 1, DY, 3, 2);
                P.put (C[std::min (nc - 1, k + 1)], x - (to > from ? 3 : -3), DY, 2, 1);
                if (fr > 0.9) P.put (C[k], to, DY - 3, 1, 7);
            }
        }
        sprite (P, DRILL, dx - 3, DY - 2, MC ? 0x7a6060 : 0x6b6f75);
        P.put (0x9aa0a6, dx - 2, DY - 2, 5, 1);
        P.put (lamp, dx, DY - 1);
        P.put (MC ? 0xffd27b : 0xc9ced4, dx - 1, DY + 2, 3, 2);
        P.put (s.frame % 2 ? 0xffffff : (MC ? 0xffd27b : 0xc9ced4), dx, DY + 4);
        P.put (0x8a8f96, dx + (s.frame % 2 ? -1 : 1), DY + 3);
        if (s.playing && age < 0.55)
        {
            Rng rr (std::floor (s.lastTrig * 1000) + 11);
            const int n = (int) rnd ((s.lastAccent ? 16 : 8) * (1 + (fx.on[0] ? grit : 0.0)));
            for (int i = 0; i < n; ++i)
            {
                const double a = 0.15 + rr() * 2.85, v = 12 + rr() * 34;
                const double x = dx + std::cos (a) * v * age * 1.5, y = DY + 5 + std::sin (a) * v * age * 0.6 + 26 * age * age;
                P.put (age < 0.15 ? 0xfff4dc : (age < 0.32 ? 0xffd27a : 0x8a6a4a), x, y);
            }
        }
        Rng rd (3);
        const int dust = 5 + (int) rnd (E * 14);
        for (int i = 0; i < 20; ++i)
        {
            const double x = rd() * W, y0 = rd() * H, sp = 6 + rd() * 6;
            if (i < dust) P.put (0x8a6a4a, x, std::fmod (y0 + t * sp, (double) H));
        }
    }

    // ------------------------------------------------------------------ Classic (scope)
    double waveY (double x, int frame, double pos, double amp, double lim, bool square, bool hot)
    {
        const double P0 = std::max (5.0, 30.0 / std::pow (2.0, std::min (36.0, std::max (0.0, pos)) / 12.0));
        const double ph = wrap ((x + frame * 1.3) / P0, 1.0);
        double y = square ? (ph < 0.5 ? -1.0 : 1.0) : (1.0 - 2.0 * ph);
        if (hot) y = std::tanh (y * 2.4) / std::tanh (2.4);
        if (lim < 1.0) y = std::max (-lim, std::min (lim, y));
        return y * amp;
    }

    void classic (Pix& P, const SceneState& s, const SceneFx& fx)
    {
        const int W = WorldScenes::width, H = WorldScenes::height, f = s.frame;
        P.put (0xbdb7aa, 0, 0, W, H);
        P.put (0xd6d0c4, 0, 0, W, 1);
        P.put (0x8f8a80, 0, H - 1, W, 1);
        const int screws[4][2] { { 3, 3 }, { 240, 3 }, { 3, 64 }, { 240, 64 } };
        for (auto& sc : screws) { P.put (0x7a756c, sc[0], sc[1], 2, 2); P.put (0x5a5650, sc[0], sc[1] + 1, 2, 1); }
        P.put (0x24211d, 9, 5, 227, 54);
        P.put (0x2a2416, 11, 7, 223, 50);
        for (int y = 11; y < 56; y += 6) for (int x = 15; x < 160; x += 6) P.put (0x3a3220, x, y);
        P.put (0x3a3220, 164, 9, 1, 46);

        const double lim = fx.on[0] ? 1.0 - fx.v[0][0] * 0.65 : 1.0, mid = 31;
        auto trace = [&] (double pos, double amp, int fo, u32 col, u32 glow, double dxs)
        {
            bool havePrev = false;
            double prev = 0;
            for (int x = 14; x < 161; ++x)
            {
                const double y = rnd (mid - waveY (x - dxs, fo, pos, amp, lim, s.square, s.alt));
                if (havePrev && std::abs (y - prev) > 1)
                {
                    const double a = std::min (y, prev), b = std::max (y, prev);
                    P.put (glow != 0 ? glow : col, x, a, 1, b - a);
                }
                P.put (col, x, y);
                prev = y;
                havePrev = true;
            }
        };
        auto energyAt = [&] (size_t j) { return j < s.ehist.size() ? s.ehist[j] : 0.0f; };
        if (fx.on[2])
        {
            const int n = 1 + (int) rnd (fx.v[2][0] * 3);
            const u32 C[] { 0x5a4320, 0x46351a, 0x382a16, 0x2f2514 };
            for (int k = std::min (n, 4); k >= 1; --k)
            {
                const size_t j = (size_t) k * 2;
                if (j >= s.hist.size()) continue;
                trace (s.hist[j], 2 + energyAt (j) * 17, f - (int) j, C[k - 1], 0, 0);
            }
        }
        if (fx.on[1])
        {
            const int n = 1 + (int) rnd (fx.v[1][0] * 2), D = (int) rnd (fx.delaySeconds * 30);
            for (int k = n; k >= 1; --k)
            {
                const size_t j = (size_t) (k * D);
                if (j >= s.hist.size()) continue;
                trace (s.hist[j], (2 + energyAt (j) * 17) * std::pow (fx.v[1][0], k), f - (int) j, k == 1 ? 0x7a5420 : 0x4a3418, 0, k * 5);
            }
        }
        trace (s.spos, 2 + s.e * 17, f, s.alt ? 0xffd27c : 0xffb347, s.alt ? 0xd9632b : 0xb9782c, 0);
        for (int i = 0; i < 16; ++i)
        {
            const double x = 18 + i * 13.5;
            const bool on = i == s.step, used = s.usedSteps[(size_t) i];
            P.put (on ? (s.alt ? 0xff4a2a : 0xff8a3d) : (used ? 0x5a2a1a : 0x3f3a33), x, 62, 5, 3);
            if (on) P.put (0xffd2a8, x + 1, 62, 2, 1);
        }
    }
}

namespace WorldScenes
{

void drawWorld (juce::Graphics& g, const SceneState& s, const SceneFx& fx)
{
    Pix P { g, width, height };
    switch (s.world)
    {
        case World::Space:       if (s.alt) deepSpace (P, s, fx); else space (P, s, fx); break;
        case World::Underground: underground (g, s, fx); break;
        case World::Classic:     classic (P, s, fx); break;
    }
}

void drawBand (juce::Graphics& g, World world, int step, bool playing, bool alt)
{
    Pix P { g, bandWidth, bandHeight };
    if (world == World::Classic)
    {
        P.put (0x8f8a80, 0, 0, 250, 12); P.put (0xa39d92, 0, 1, 250, 1); P.put (0x7a756c, 0, 10, 250, 1);
        for (int x = 58; x < 160; x += 3) P.put (0x7a756c, x, 3, 2, 1);
        for (int i = 0; i < 16; ++i)
        {
            const int x = (int) (58 + i * 6.4);
            P.put (i == step ? (alt ? 0xff4a2a : 0xff8a3d) : (i % 4 == 0 ? 0x5a2a1a : 0x3f2a22), x, 6, 3, 2);
            if (i == step) P.put (0xffd2a8, x + 1, 6, 1, 1);
        }
    }
    else if (world == World::Space)
    {
        P.put (0x03040b, 0, 0, 250, 12);
        Rng r (42);
        const double off = playing ? step * 3 : 0;
        for (int i = 0; i < 46; ++i)
        {
            const double x = std::floor (r() * 250), y = std::floor (r() * 12), b = r();
            P.put (b > 0.85 ? 0xffffff : (b > 0.5 ? 0x8796d6 : 0x3a4aa8), wrap (x - off, 250), y);
        }
        if (alt)
        {
            P.put (0xff8a3d, 118, 6, 36, 1); P.put (0xffd166, 124, 5, 24, 1);
            for (int y = 2; y < 10; ++y) { const double hw = rnd (std::sqrt (std::max (0.0, 16.0 - (y - 6) * (y - 6)))); P.put (0x000000, 136 - hw, y, hw * 2, 1); }
            P.put (0xffe7a1, 131, 1, 10, 1);
        }
        else
        {
            for (int y = 0; y < 12; ++y)
                for (int x = 120; x < 150; ++x)
                {
                    const double d = std::hypot (x - 136.0, (y - 13) * 1.15);
                    if (d < 11) P.put (d < 7 ? 0xff5cc8 : (d < 9.5 ? 0xc23a96 : 0x7a2266), x, y);
                }
            P.put (0xffd166, 118, 6, 38, 1); P.put (0xb58a2c, 121, 7, 32, 1);
        }
        P.put (0x151b45, 54, 0, 3, 12); P.put (0x151b45, 158, 0, 3, 12); P.put (0x26306b, 57, 0, 101, 1); P.put (0x26306b, 57, 11, 101, 1);
        if (step >= 0) { const double x = 60 + step * 6; P.put (0x6cf2ff, x, 4, 1, 4); P.put (0x6cf2ff, x - 2, 6, 5, 1); }
    }
    else
    {
        P.put (0x3a2a20, 0, 0, 250, 12); P.put (0x5a3a22, 0, 0, 250, 3); P.put (0x7a4a2a, 0, 3, 250, 2);
        P.put (alt ? 0xc2361b : 0x2a1d15, 0, 9, 250, 3);
        if (alt) P.put (0xff8a3e, 0, 9, 250, 1);
        Rng r (7);
        for (int i = 0; i < 60; ++i)
        {
            const double x = std::floor (r() * 250), y = std::floor (r() * 12), b = r();
            P.put (b > 0.9 ? 0x7fd1c7 : (b > 0.5 ? 0x6b5242 : 0x241810), x, y);
        }
        const int dx = 60 + (step < 0 ? 9 : step) * 6;
        P.put (0x0e0805, 54, 5, dx - 54, 4); P.put (0x1d130c, 54, 5, dx - 54, 1);
        for (int x = 58; x < dx - 3; x += 7) P.put (0xffb02e, x, 6, 1, 1);
        P.put (0x6b6f75, dx - 8, 5, 6, 4); P.put (0x9aa0a6, dx - 8, 5, 6, 1); P.put (0xffb02e, dx - 6, 7, 1, 1);
        P.put (0xc9ced4, dx - 2, 5, 2, 4); P.put (0xc9ced4, dx, 6, 2, 2); P.put (0xffffff, dx + 2, 7, 1, 1);
        if (playing) { P.put (0x8a6a4a, dx + 3, 5, 1, 1); P.put (0x8a6a4a, dx + 4, 8, 1, 1); }
    }
}

} // namespace WorldScenes
} // namespace acid::ui
