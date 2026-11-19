#pragma once

#include "Envelopes.h"

#include <cmath>

namespace acid::dsp
{

/** Bit-depth and sample-rate reduction with dry/wet mix. Mono; runs inside the
    oversampled voice path like the other distortions.
    amount: 16 bits down to 3; rate: 1 (no hold) .. ~48 samples held. */
class Bitcrusher
{
public:
    void prepare (double sampleRate)
    {
        smooth = onePoleCoef (0.01, sampleRate);
        reset();
    }

    void reset() { held = 0.0f; counter = 0.0f; }

    void setParams (float amount01, float rate01, float mix01)
    {
        targetAmount = amount01;
        targetRate = rate01;
        targetMix = mix01;
    }

    float process (float x)
    {
        amount += (targetAmount - amount) * smooth;
        rate += (targetRate - rate) * smooth;
        mix += (targetMix - mix) * smooth;

        const float hold = 1.0f + (1.0f - rate) * (1.0f - rate) * 47.0f;
        counter += 1.0f;
        if (counter >= hold)
        {
            counter -= hold;
            const float bits = 16.0f - amount * 13.0f;
            const float steps = std::exp2 (bits - 1.0f);
            const float v = std::tanh (x * 1.4f);
            held = std::round (v * steps) / steps;
        }
        return x + (held - x) * mix;
    }

private:
    float smooth = 0.01f;
    float amount = 0.3f, rate = 0.6f, mix = 1.0f;
    float targetAmount = 0.3f, targetRate = 0.6f, targetMix = 1.0f;
    float held = 0.0f, counter = 0.0f;
};

} // namespace acid::dsp
