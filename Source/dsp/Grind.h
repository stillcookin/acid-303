#pragma once

#include "Envelopes.h"

#include <cmath>

namespace acid::dsp
{

/** Underground's distortion: a sine wavefolder blended with asymmetric
    clipping, a little level-stepping crunch, and a low "weight" boost.
    Mono; runs in the oversampled voice path (after the VCA, or before the
    filter in Molten Core). */
class Grind
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        smooth = onePoleCoef (0.01, sr);
        dcCoef = (float) std::exp (-2.0 * 3.14159265358979323846 * 15.0 / sr);
        lowCoef = 1.0f - (float) std::exp (-2.0 * 3.14159265358979323846 * 160.0 / sr);
        reset();
    }

    void reset() { low = dcX = dcY = 0.0f; }

    /** grit 0..1 (fold/crunch), weight 0..1 (low boost), mix 0..1 */
    void setParams (float grit01, float weight01, float mix01)
    {
        targetGrit = grit01;
        targetWeight = weight01;
        targetMix = mix01;
    }

    float process (float x)
    {
        grit += (targetGrit - grit) * smooth;
        weight += (targetWeight - weight) * smooth;
        mix += (targetMix - mix) * smooth;

        const float v = x * (1.0f + grit * 9.0f);
        const float folded = std::sin (v * 1.3f);
        const float clipped = std::tanh (v + 0.3f) - 0.2913126f; // tanh(0.3)
        float y = folded * (0.35f + 0.4f * grit) + clipped * (0.65f - 0.4f * grit);

        // stepped crunch, stronger with grit
        const float steps = 96.0f - grit * 72.0f;
        y = std::round (y * steps) / steps;

        // dc blocker
        const float dc = dcCoef * (dcY + y - dcX);
        dcX = y;
        dcY = dc;
        y = dc;

        // weight: add back the lows the folding thins out
        low += (y - low) * lowCoef;
        y += low * weight * 1.6f;

        const float wet = y / (1.0f + grit * 0.7f + weight * 0.5f);
        return x + (wet - x) * mix;
    }

private:
    double sr = 44100.0;
    float smooth = 0.01f, dcCoef = 0.999f, lowCoef = 0.01f;
    float grit = 0.5f, weight = 0.5f, mix = 0.7f;
    float targetGrit = 0.5f, targetWeight = 0.5f, targetMix = 0.7f;
    float low = 0.0f, dcX = 0.0f, dcY = 0.0f;
};

} // namespace acid::dsp
