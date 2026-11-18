#pragma once

#include "DiodeLadder.h"
#include "Envelopes.h"
#include "Oscillator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace acid::dsp
{

struct VoiceParams
{
    float tune = 0.0f;       ///< semitones
    int waveform = 0;        ///< 0 saw, 1 square
    float cutoff = 0.35f;    ///< 0..1
    float resonance = 0.65f; ///< 0..1
    float envMod = 0.5f;     ///< 0..1
    float decay = 0.4f;      ///< 0..1 → 200 ms .. 2 s
    float accent = 0.6f;     ///< 0..1
    float drift = 0.15f;     ///< 0..1 analogue pitch instability
};

/** Monophonic 303-style voice: osc → diode ladder → VCA, with MEG, accent
    sweep circuit and constant-time slide. Runs at whatever rate it is
    prepared with (the Engine runs it oversampled). */
class Voice303
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        osc.prepare (sr);
        filter.prepare (sr);
        amp.prepare (sr);
        accentEnv.setDecay (0.20, sr);
        glideCoef = onePoleCoef (0.022, sr);       // ≈ 60-70 ms to arrive
        sweepChargeCoef = onePoleCoef (0.035, sr);
        sweepDischargeCoef = onePoleCoef (0.180, sr);
        driftCoef = onePoleCoef (0.25, sr);
        paramSmoothCoef = onePoleCoef (0.004, sr);
        reset();
    }

    void reset()
    {
        osc.reset();
        filter.reset();
        amp.reset();
        meg.reset();
        accentEnv.reset();
        sweep = 0.0f;
        accentOn = false;
        gliding = false;
    }

    void setParams (const VoiceParams& p)
    {
        params = p;
        filter.setResonance (p.resonance);
    }

    /** slide: glide from the current pitch without retriggering envelopes
        (only effective while the gate is already held). */
    void noteOn (int midiNote, bool accent, bool slide)
    {
        targetPitch = (float) midiNote;
        accentOn = accent;

        if (slide && amp.isGateOn())
        {
            gliding = true;
            return;
        }

        gliding = false;
        currentPitch = targetPitch;
        notePitchOffset = (nextRandom() * 2.0f - 1.0f) * 0.06f * params.drift; // per-note tuning error
        const double decaySec = accent ? 0.20 : 0.2 * std::pow (10.0, (double) params.decay);
        meg.setDecay (decaySec, sr);
        meg.trigger();
        if (accent)
            accentEnv.trigger();
        amp.noteOn();
    }

    void noteOff() { amp.noteOff(); }

    bool isActive() const { return amp.isActive(); }
    bool isGateOn() const { return amp.isGateOn(); }
    float getCurrentPitch() const { return currentPitch; }

    float process()
    {
        // pitch: slide + drift
        if (gliding)
            currentPitch += (targetPitch - currentPitch) * glideCoef;

        if (--driftCounter <= 0)
        {
            driftCounter = (int) (sr * 0.15);
            driftTarget = nextRandom() * 2.0f - 1.0f;
        }
        driftValue += (driftTarget - driftValue) * driftCoef;

        const float pitch = currentPitch + params.tune + notePitchOffset + driftValue * 0.12f * params.drift;
        osc.setFrequency (440.0 * std::exp2 ((pitch - 69.0) / 12.0));

        // smoothed knob values (avoid zipper noise)
        cutoffS += (params.cutoff - cutoffS) * paramSmoothCoef;
        envModS += (params.envMod - envModS) * paramSmoothCoef;
        accentS += (params.accent - accentS) * paramSmoothCoef;

        // envelopes
        const float megV = meg.process();
        const float accV = accentEnv.process() * (accentOn ? 1.0f : 0.0f);
        const float sweepTarget = accV * accentS;
        sweep += (sweepTarget - sweep) * (sweepTarget > sweep ? sweepChargeCoef : sweepDischargeCoef);

        // filter frequency (303-style: env mod also pulls the base cutoff down)
        const float envOct = envModS * 4.8f;
        const float baseOct = cutoffS * 4.6f - envModS * 1.1f;
        const float oct = baseOct + megV * envOct * (accentOn ? 0.6f : 1.0f) + sweep * 3.2f;
        filter.setCutoff (170.0 * std::exp2 ((double) oct));

        const float o = osc.process (params.waveform == 0 ? Oscillator::Wave::Saw : Oscillator::Wave::Square);
        float y = filter.process (o * 0.9f);
        y *= 1.0f + 0.5f * params.resonance; // partial loudness compensation

        const float ampV = amp.process();
        const float accentGain = 1.0f + (accentOn ? accentS * (0.35f + 0.9f * accV) : 0.0f);
        return y * ampV * accentGain * 0.55f;
    }

private:
    float nextRandom()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng & 0xffffff) / (float) 0xffffff;
    }

    double sr = 88200.0;
    VoiceParams params;
    Oscillator osc;
    DiodeLadder filter;
    AmpEnvelope amp;
    DecayEnvelope meg, accentEnv;

    float targetPitch = 36.0f, currentPitch = 36.0f, notePitchOffset = 0.0f;
    bool gliding = false, accentOn = false;
    float glideCoef = 0.001f;
    float sweep = 0.0f, sweepChargeCoef = 0.001f, sweepDischargeCoef = 0.0001f;
    float cutoffS = 0.35f, envModS = 0.5f, accentS = 0.6f, paramSmoothCoef = 0.01f;

    uint32_t rng = 0x1234567u;
    int driftCounter = 0;
    float driftTarget = 0.0f, driftValue = 0.0f, driftCoef = 0.0001f;
};

} // namespace acid::dsp
