#pragma once

#include "Reverb.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace acid::dsp
{

/** Underground's reverb: a deep, pre-delayed room with water drips — sparse,
    quickly decaying high plinks fed into the reverb only, never the dry path.
    Depth sets size and pre-delay, Drip sets how bright and how wet the cave is. */
class Cavern
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        verb.prepare (sampleRate);
        preSize = (int) (0.12 * sr) + 4;
        preL.assign ((size_t) preSize, 0.0f);
        preR.assign ((size_t) preSize, 0.0f);
        tmpL.assign ((size_t) maxBlock, 0.0f);
        tmpR.assign ((size_t) maxBlock, 0.0f);
        reset();
    }

    void reset()
    {
        verb.reset();
        std::fill (preL.begin(), preL.end(), 0.0f);
        std::fill (preR.begin(), preR.end(), 0.0f);
        prePos = 0;
        dripAmp = 0.0f;
    }

    void setParams (float depth01, float drip01, float mix01)
    {
        depth = depth01;
        drip = drip01;
        mix = mix01;
        verb.setParams (0.6f + 0.4f * depth01, 0.75f - 0.6f * drip01, 1.0f);
        verb.setDryWet (0.0f, 0.33f);
        preDelay = (int) ((0.025 + 0.07 * depth01) * sr);
    }

    void process (float* left, float* right, int n)
    {
        n = std::min (n, (int) tmpL.size());
        const float dripRate = drip * 2.5f / (float) sr; // drips per sample
        const float dripDecay = std::exp (-1.0f / (0.035f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            preL[(size_t) prePos] = left[i];
            preR[(size_t) prePos] = right[i];
            int rp = prePos - preDelay;
            if (rp < 0) rp += preSize;
            float inL = preL[(size_t) rp], inR = preR[(size_t) rp];
            if (++prePos >= preSize) prePos = 0;

            if (random() < dripRate)
            {
                dripAmp = 0.05f + 0.06f * random();
                dripFreq = 1600.0f + 2600.0f * random();
                dripPan = random();
                dripPhase = 0.0f;
            }
            if (dripAmp > 1.0e-4f)
            {
                dripPhase += dripFreq / (float) sr;
                dripFreq *= 1.00004f; // drips rise slightly as they ring
                const float s = std::sin (6.2831853f * dripPhase) * dripAmp;
                dripAmp *= dripDecay;
                inL += s * (1.0f - dripPan);
                inR += s * dripPan;
            }
            tmpL[(size_t) i] = inL;
            tmpR[(size_t) i] = inR;
        }
        verb.process (tmpL.data(), tmpR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            left[i] += tmpL[(size_t) i] * mix * 0.9f;
            right[i] += tmpR[(size_t) i] * mix * 0.9f;
        }
    }

private:
    float random()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng & 0xffffff) / (float) 0xffffff;
    }

    double sr = 44100.0;
    StereoReverb verb;
    std::vector<float> preL, preR, tmpL, tmpR;
    int preSize = 1, prePos = 0, preDelay = 1000;
    float depth = 0.7f, drip = 0.35f, mix = 0.35f;
    float dripAmp = 0.0f, dripFreq = 2000.0f, dripPan = 0.5f, dripPhase = 0.0f;
    uint32_t rng = 0x2545F491u;
};

} // namespace acid::dsp
