#pragma once

#include "Envelopes.h"

#include <cmath>

namespace acid::dsp
{

/** Asymmetric diode-style overdrive with post tone control and dry/wet mix.
    Mono; the Engine runs it inside the oversampled voice path. */
class Overdrive
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        smooth = onePoleCoef (0.01, sr);
        dcCoef = (float) std::exp (-2.0 * 3.14159265358979323846 * 15.0 / sr);
        reset();
    }

    void reset() { lp = dcX = dcY = 0.0f; }

    void setParams (float amount01, float tone01, float mix01)
    {
        targetDrive = amount01;
        targetTone = tone01;
        targetMix = mix01;
    }

    float process (float x)
    {
        drive += (targetDrive - drive) * smooth;
        tone += (targetTone - tone) * smooth;
        mix += (targetMix - mix) * smooth;

        const float pre = std::pow (10.0f, (drive * 34.0f) / 20.0f); // 0..34 dB
        const float bias = 0.22f;
        const float v = x * pre;
        float y = std::tanh (v + bias) - std::tanh (bias);           // asymmetric → even harmonics
        y = y >= 0.0f ? y : y * 0.85f;

        // dc blocker
        const float dc = dcCoef * (dcY + y - dcX);
        dcX = y;
        dcY = dc;
        y = dc;

        // tone: one-pole low-pass 900 Hz .. 16 kHz
        const float fc = 900.0f * std::pow (2.0f, tone * 4.15f);
        const float a = 1.0f - std::exp (-2.0f * 3.14159265f * fc / (float) sr);
        lp += (y - lp) * a;

        const float makeup = 1.0f / (1.0f + drive * 1.6f);
        const float wet = lp * makeup * 1.4f;
        return x + (wet - x) * mix;
    }

private:
    double sr = 44100.0;
    float smooth = 0.01f;
    float drive = 0.3f, tone = 0.6f, mix = 1.0f;
    float targetDrive = 0.3f, targetTone = 0.6f, targetMix = 1.0f;
    float lp = 0.0f, dcX = 0.0f, dcY = 0.0f, dcCoef = 0.999f;
};

} // namespace acid::dsp
