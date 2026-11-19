#pragma once

#include "Envelopes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace acid::dsp
{

/** Space's tape-style echo: wow and flutter on the delay time, saturation and
    darkening in the feedback loop, and (Tape on) a diffusing smear that blurs
    each repeat a little more than the last. */
class SpaceEcho
{
public:
    void prepare (double sampleRate, double maxSeconds = 4.0)
    {
        sr = sampleRate;
        size = (int) (maxSeconds * sr) + 8;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        timeSmooth = onePoleCoef (0.25, sr);
        mixSmooth = onePoleCoef (0.02, sr);
        hpCoef = (float) std::exp (-2.0 * 3.14159265358979323846 * 90.0 / sr);
        const double scale = sr / 44100.0;
        const int lens[] { 113, 337, 521, 211, 389, 593 };
        for (size_t i = 0; i < diff.size(); ++i)
        {
            diff[i].buf.assign ((size_t) std::max (2, (int) (lens[i] * scale)), 0.0f);
            diff[i].pos = 0;
        }
        reset();
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        for (auto& d : diff) std::fill (d.buf.begin(), d.buf.end(), 0.0f);
        write = 0;
        lpL = lpR = hxL = hyL = hxR = hyR = 0.0f;
        delaySamples = targetDelay;
    }

    /** wow 0..1, tape: saturation + darker repeats + smear */
    void setParams (double delaySeconds, float feedback, float wow01, float mix01, bool tapeOn)
    {
        targetDelay = std::clamp (delaySeconds * sr, 8.0, (double) size - 8.0);
        fb = std::clamp (feedback, 0.0f, 0.97f);
        wow = wow01;
        targetMix = mix01;
        tape = tapeOn;
        const float fc = tape ? 3200.0f : 6500.0f;
        lpCoef = 1.0f - std::exp (-2.0f * 3.14159265f * fc / (float) sr);
    }

    /** Extra signal into the echo input (Deep Space feeds the shimmer back here). */
    void process (float* left, float* right, int numSamples, const float* extraL = nullptr, const float* extraR = nullptr)
    {
        const float twoPi = 6.2831853f;
        for (int n = 0; n < numSamples; ++n)
        {
            delaySamples += (targetDelay - delaySamples) * timeSmooth;
            mix += (targetMix - mix) * mixSmooth;
            wowPhase += 0.45f / (float) sr;
            flutterPhase += 6.3f / (float) sr;
            if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
            const double mod = 1.0 + wow * (0.007 * std::sin (twoPi * wowPhase) + 0.0018 * std::sin (twoPi * flutterPhase));

            const float dl = read (bufL, delaySamples * mod);
            const float dr = read (bufR, delaySamples * mod * 1.031);

            float fl = filter (dl, lpL, hxL, hyL), fr = filter (dr, lpR, hxR, hyR);
            const float drive = tape ? 1.8f : 1.1f;
            fl = std::tanh (fl * fb * drive) / drive;
            fr = std::tanh (fr * fb * drive) / drive;
            if (tape)
            {
                fl = smear (fl, 0);
                fr = smear (fr, 3);
            }

            const float inL = left[n] + (extraL ? extraL[n] : 0.0f);
            const float inR = right[n] + (extraR ? extraR[n] : 0.0f);
            bufL[(size_t) write] = inL + fl;
            bufR[(size_t) write] = inR + fr;
            if (++write >= size) write = 0;

            left[n] += dl * mix;
            right[n] += dr * mix;
        }
    }

private:
    struct Diffuser { std::vector<float> buf; int pos = 0; };

    float smear (float x, size_t first)
    {
        for (size_t i = first; i < first + 3; ++i)
        {
            auto& d = diff[i];
            const float delayed = d.buf[(size_t) d.pos];
            const float y = -0.5f * x + delayed;
            d.buf[(size_t) d.pos] = x + 0.5f * y;
            if (++d.pos >= (int) d.buf.size()) d.pos = 0;
            x = y;
        }
        return x;
    }

    float read (const std::vector<float>& buf, double delay) const
    {
        double rp = (double) write - delay;
        while (rp < 0.0) rp += size;
        const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
        const float frac = (float) (rp - std::floor (rp));
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
    }

    float filter (float x, float& lp, float& hx, float& hy) const
    {
        lp += (x - lp) * lpCoef;
        const float h = hpCoef * (hy + lp - hx);
        hx = lp;
        hy = h;
        return h;
    }

    double sr = 44100.0;
    std::vector<float> bufL, bufR;
    std::array<Diffuser, 6> diff;
    int size = 1, write = 0;
    double delaySamples = 1000.0, targetDelay = 1000.0;
    float timeSmooth = 0.001f, mixSmooth = 0.001f, hpCoef = 0.99f, lpCoef = 0.5f;
    float fb = 0.5f, wow = 0.4f, mix = 0.0f, targetMix = 0.3f;
    float wowPhase = 0.0f, flutterPhase = 0.0f;
    bool tape = true;
    float lpL = 0, lpR = 0, hxL = 0, hyL = 0, hxR = 0, hyR = 0;
};

} // namespace acid::dsp
