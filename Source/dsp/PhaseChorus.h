#pragma once

#include "Envelopes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace acid::dsp
{

/** Space's modulation: a six-stage phaser and a two-voice chorus sharing one
    LFO (the right channel runs a quarter cycle ahead for width). */
class PhaseChorus
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (0.05 * sr) + 4;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        smooth = onePoleCoef (0.02, sr);
        reset();
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        write = 0;
        apL.fill (0.0f);
        apR.fill (0.0f);
        fbL = fbR = 0.0f;
    }

    void setParams (float rate01, float depth01, float mix01)
    {
        rateHz = 0.05f + rate01 * rate01 * 3.5f;
        targetDepth = depth01;
        targetMix = mix01;
    }

    void process (float* left, float* right, int numSamples)
    {
        const float twoPi = 6.2831853f;
        for (int n = 0; n < numSamples; ++n)
        {
            depth += (targetDepth - depth) * smooth;
            mix += (targetMix - mix) * smooth;
            phase += rateHz / (float) sr;
            if (phase >= 1.0f) phase -= 1.0f;

            const float lfoL = std::sin (twoPi * phase);
            const float lfoR = std::sin (twoPi * (phase + 0.25f));

            const float inL = left[n], inR = right[n];
            bufL[(size_t) write] = inL;
            bufR[(size_t) write] = inR;

            const float pL = phaser (inL + fbL * 0.45f * depth, lfoL, apL);
            const float pR = phaser (inR + fbR * 0.45f * depth, lfoR, apR);
            fbL = pL;
            fbR = pR;

            const double baseMs = 7.0, swingMs = 1.0 + depth * 7.0;
            const float cL = read (bufL, (baseMs + swingMs * (0.5 + 0.5 * lfoL)) * 0.001 * sr);
            const float cR = read (bufR, (baseMs + swingMs * (0.5 + 0.5 * lfoR)) * 0.001 * sr);

            if (++write >= size) write = 0;

            const float wetL = 0.5f * (inL + pL) * 0.6f + cL * 0.5f;
            const float wetR = 0.5f * (inR + pR) * 0.6f + cR * 0.5f;
            left[n] = inL + (wetL - inL) * mix;
            right[n] = inR + (wetR - inR) * mix;
        }
    }

private:
    float phaser (float x, float lfo, std::array<float, 6>& z)
    {
        const float f = 250.0f * std::pow (2.0f, (0.5f + 0.5f * lfo) * (1.5f + depth * 2.5f));
        const float t = std::tan (3.14159265f * std::min (f, (float) sr * 0.45f) / (float) sr);
        const float a = (1.0f - t) / (1.0f + t);
        for (auto& s : z)
        {
            const float y = -a * x + s;
            s = x + a * y;
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

    double sr = 44100.0;
    std::vector<float> bufL, bufR;
    int size = 1, write = 0;
    std::array<float, 6> apL {}, apR {};
    float fbL = 0.0f, fbR = 0.0f, phase = 0.0f, rateHz = 0.3f;
    float smooth = 0.001f, depth = 0.6f, targetDepth = 0.6f, mix = 0.0f, targetMix = 0.5f;
};

} // namespace acid::dsp
