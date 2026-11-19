#pragma once

#include "Reverb.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace acid::dsp
{

/** Octave-up pitch shifter: two crossfaded read heads sweeping a short delay
    line at double speed. Smooth enough for reverb tails. */
class OctaveUp
{
public:
    void prepare (double sampleRate)
    {
        window = std::max (256, (int) (0.045 * sampleRate));
        size = window * 2 + 4;
        buf.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        write = 0;
        d1 = 0.0;
    }

    float process (float x)
    {
        buf[(size_t) write] = x;
        // reading at 2x speed = the delay shrinks by one sample per sample
        d1 -= 1.0;
        if (d1 < 0.0) d1 += window;
        double d2 = d1 + window * 0.5;
        if (d2 >= window) d2 -= window;
        const float g1 = (float) std::sin (3.14159265358979 * d1 / window);
        const float g2 = (float) std::sin (3.14159265358979 * d2 / window);
        const float y = read (d1 + 2.0) * g1 + read (d2 + 2.0) * g2;
        if (++write >= size) write = 0;
        return y * 0.7071f;
    }

private:
    float read (double delay) const
    {
        double rp = (double) write - delay;
        while (rp < 0.0) rp += size;
        const int i0 = (int) rp % size, i1 = (i0 + 1) % size;
        const float frac = (float) (rp - std::floor (rp));
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
    }

    std::vector<float> buf;
    int size = 1, window = 1, write = 0;
    double d1 = 0.0;
};

/** Space's reverb: a large room whose tail is pitched up an octave and fed
    back in, so notes bloom upwards. The amount sets how much climbs. */
class Shimmer
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        verb.prepare (sampleRate);
        upL.prepare (sampleRate);
        upR.prepare (sampleRate);
        tmpL.assign ((size_t) maxBlock, 0.0f);
        tmpR.assign ((size_t) maxBlock, 0.0f);
        fbL.assign ((size_t) maxBlock, 0.0f);
        fbR.assign ((size_t) maxBlock, 0.0f);
        lastWetL.assign ((size_t) maxBlock, 0.0f);
        lastWetR.assign ((size_t) maxBlock, 0.0f);
        reset();
    }

    void reset()
    {
        verb.reset();
        upL.reset();
        upR.reset();
        std::fill (fbL.begin(), fbL.end(), 0.0f);
        std::fill (fbR.begin(), fbR.end(), 0.0f);
        std::fill (lastWetL.begin(), lastWetL.end(), 0.0f);
        std::fill (lastWetR.begin(), lastWetR.end(), 0.0f);
    }

    void setParams (float size01, float shimmer01, float mix01)
    {
        amount = shimmer01;
        mix = mix01;
        verb.setParams (0.55f + 0.45f * size01, 0.35f, 1.0f);
        verb.setDryWet (0.0f, 0.33f); // juce::Reverb scales wet by ~3: this is about unity
    }

    /** The wet signal of the last block (Deep Space sends it back into the echo). */
    const float* lastWet (int ch) const { return ch == 0 ? lastWetL.data() : lastWetR.data(); }

    void process (float* left, float* right, int n)
    {
        n = std::min (n, (int) tmpL.size());
        for (int i = 0; i < n; ++i)
        {
            // the shifted tail of the previous block climbs back in
            tmpL[(size_t) i] = left[i] + fbL[(size_t) (i % (int) fbL.size())];
            tmpR[(size_t) i] = right[i] + fbR[(size_t) (i % (int) fbR.size())];
        }
        verb.process (tmpL.data(), tmpR.data(), n);
        const float fbGain = 0.42f * amount;
        for (int i = 0; i < n; ++i)
        {
            const float wl = tmpL[(size_t) i], wr = tmpR[(size_t) i];
            // soft-limited, so the climbing tail can never run away
            fbL[(size_t) i] = std::tanh (upL.process (wl) * fbGain * 1.5f) / 1.5f;
            fbR[(size_t) i] = std::tanh (upR.process (wr) * fbGain * 1.5f) / 1.5f;
            lastWetL[(size_t) i] = wl;
            lastWetR[(size_t) i] = wr;
            left[i] += wl * mix * 0.8f;
            right[i] += wr * mix * 0.8f;
        }
        for (int i = n; i < (int) fbL.size(); ++i)
            fbL[(size_t) i] = fbR[(size_t) i] = 0.0f;
    }

private:
    StereoReverb verb;
    OctaveUp upL, upR;
    std::vector<float> tmpL, tmpR, fbL, fbR, lastWetL, lastWetR;
    float amount = 0.45f, mix = 0.4f;
};

} // namespace acid::dsp
