#pragma once

#include "Envelopes.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace acid::dsp
{

/** Stereo tempo-synced delay with damping and low-cut in the feedback loop,
    soft saturation on feedback, and optional ping-pong. */
class StereoDelay
{
public:
    void prepare (double sampleRate, double maxSeconds = 4.0)
    {
        sr = sampleRate;
        size = (int) (maxSeconds * sr) + 4;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        timeSmooth = onePoleCoef (0.08, sr);
        mixSmooth = onePoleCoef (0.02, sr);
        hpCoef = (float) std::exp (-2.0 * 3.14159265358979323846 * 120.0 / sr);
        reset();
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        write = 0;
        lpL = lpR = hpXL = hpYL = hpXR = hpYR = 0.0f;
        delaySamples = targetDelay;
    }

    void setParams (double delaySeconds, float feedback, float tone01, float mix01, bool pingPong)
    {
        targetDelay = std::clamp (delaySeconds * sr, 1.0, (double) size - 4.0);
        fb = std::clamp (feedback, 0.0f, 0.95f);
        const float fc = 1200.0f * std::pow (2.0f, tone01 * 3.7f); // 1.2 .. 15.6 kHz
        lpCoef = 1.0f - std::exp (-2.0f * 3.14159265f * fc / (float) sr);
        targetMix = mix01;
        pp = pingPong;
    }

    void process (float* left, float* right, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            delaySamples += (targetDelay - delaySamples) * timeSmooth;
            mix += (targetMix - mix) * mixSmooth;

            const float dl = read (bufL, delaySamples);
            const float dr = read (bufR, delaySamples);

            const float inL = left[n], inR = right[n];
            float fbL = filter (dl, lpL, hpXL, hpYL);
            float fbR = filter (dr, lpR, hpXR, hpYR);
            fbL = std::tanh (fbL * fb * 1.1f) / 1.1f;
            fbR = std::tanh (fbR * fb * 1.1f) / 1.1f;

            if (pp)
            {
                // mono input enters left only; taps bounce L→R→L
                bufL[(size_t) write] = 0.5f * (inL + inR) + fbR;
                bufR[(size_t) write] = fbL;
            }
            else
            {
                bufL[(size_t) write] = inL + fbL;
                bufR[(size_t) write] = inR + fbR;
            }

            if (++write >= size) write = 0;

            left[n] = inL + dl * mix;
            right[n] = inR + dr * mix;
        }
    }

private:
    float read (const std::vector<float>& buf, double delay) const
    {
        double rp = (double) write - delay;
        while (rp < 0.0) rp += size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float frac = (float) (rp - i0);
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
    int size = 1, write = 0;
    double delaySamples = 1000.0, targetDelay = 1000.0;
    float timeSmooth = 0.001f, mixSmooth = 0.001f;
    float fb = 0.4f, lpCoef = 0.5f, hpCoef = 0.99f, mix = 0.0f, targetMix = 0.25f;
    bool pp = true;
    float lpL = 0, lpR = 0, hpXL = 0, hpYL = 0, hpXR = 0, hpYR = 0;
};

} // namespace acid::dsp
