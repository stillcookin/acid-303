#pragma once

#include <cmath>

namespace acid::dsp
{

/** Coefficient for an exponential segment that falls to -40 dB in `seconds`. */
inline float decayCoef (double seconds, double sampleRate)
{
    if (seconds <= 0.0) return 0.0f;
    return (float) std::exp (std::log (0.01) / (seconds * sampleRate));
}

/** One-pole smoothing coefficient for time constant `tau` seconds. */
inline float onePoleCoef (double tau, double sampleRate)
{
    if (tau <= 0.0) return 1.0f;
    return (float) (1.0 - std::exp (-1.0 / (tau * sampleRate)));
}

/** Instant-attack exponential decay (303 MEG / accent envelope). */
class DecayEnvelope
{
public:
    void setDecay (double seconds, double sr) { coef = decayCoef (seconds, sr); }
    void trigger() { value = 1.0f; }
    void reset() { value = 0.0f; }
    float process()
    {
        const float v = value;
        value *= coef;
        if (value < 1.0e-6f) value = 0.0f;
        return v;
    }
    float current() const { return value; }

private:
    float value = 0.0f, coef = 0.999f;
};

/** 303 VEG: short attack, long decay while the gate is held, quick release. */
class AmpEnvelope
{
public:
    void prepare (double sr)
    {
        attackCoef = onePoleCoef (0.0012, sr);
        holdDecayCoef = decayCoef (4.0, sr);
        releaseCoef = decayCoef (0.010, sr);
    }

    void noteOn() { gate = true; attacking = true; }
    void noteOff() { gate = false; attacking = false; }
    void reset() { value = 0.0f; gate = attacking = false; }

    float process()
    {
        if (gate)
        {
            if (attacking)
            {
                value += (1.02f - value) * attackCoef;
                if (value >= 1.0f) { value = 1.0f; attacking = false; }
            }
            else
            {
                value *= holdDecayCoef;
            }
        }
        else
        {
            value *= releaseCoef;
            if (value < 1.0e-6f) value = 0.0f;
        }
        return value;
    }

    bool isGateOn() const { return gate; }
    bool isActive() const { return gate || value > 0.0f; }

private:
    float value = 0.0f;
    bool gate = false, attacking = false;
    float attackCoef = 0.1f, holdDecayCoef = 0.9999f, releaseCoef = 0.99f;
};

} // namespace acid::dsp
