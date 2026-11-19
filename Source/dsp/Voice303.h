#pragma once

#include "DiodeLadder.h"
#include "Envelopes.h"
#include "Grind.h"
#include "Oscillator.h"
#include "Overdrive.h"

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
    float drift = 0.15f;     ///< 0..1 the "character" knob: Classic = pitch drift (Age),
                             ///< Space = pitch + slow filter drift, Underground = sub level
    int world = 0;           ///< 0 Classic, 1 Space, 2 Underground (acid::World)
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
        preDrive.prepare (sr);
        preGrind.prepare (sr);
        accentEnv.setDecay (0.20, sr);
        glideCoef = onePoleCoef (0.022, sr);       // ≈ 60-70 ms to arrive
        glideCoefLong = onePoleCoef (0.033, sr);   // Space: 1.5× longer slides
        wanderCoef = onePoleCoef (1.5, sr);
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
        subPhase = 0.0;
        wanderValue = 0.0f;
        preDrive.reset();
        preGrind.reset();
    }

    /** Distortion BEFORE the filter (the HOT and MOLTEN CORE chains):
        mode 0 off, 1 overdrive, 2 grind; a/b/mix are that unit's three knobs. */
    void setPreShaper (int mode, float a, float b, float mix)
    {
        preMode = mode;
        if (mode == 1) preDrive.setParams (a, b, mix);
        else if (mode == 2) preGrind.setParams (a, b, mix);
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
        notePitchOffset = (nextRandom() * 2.0f - 1.0f) * 0.06f * pitchDrift(); // per-note tuning error
        double decaySec = accent ? 0.20 : 0.2 * std::pow (10.0, (double) params.decay);
        if (params.world == 2 && ! accent)
            decaySec *= 0.85; // Underground: tighter
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
            currentPitch += (targetPitch - currentPitch) * (params.world == 1 ? glideCoefLong : glideCoef);

        if (--driftCounter <= 0)
        {
            driftCounter = (int) (sr * 0.15);
            driftTarget = nextRandom() * 2.0f - 1.0f;
        }
        driftValue += (driftTarget - driftValue) * driftCoef;

        const float pitch = currentPitch + params.tune + notePitchOffset + driftValue * 0.12f * pitchDrift();
        const double freq = 440.0 * std::exp2 ((pitch - 69.0) / 12.0);
        osc.setFrequency (freq);

        // Space: the filter wanders slowly (a new target every ~2 s, eased over ~1.5 s)
        if (params.world == 1)
        {
            if (--wanderCounter <= 0)
            {
                wanderCounter = (int) (sr * 2.0);
                wanderTarget = nextRandom() * 2.0f - 1.0f;
            }
            wanderValue += (wanderTarget - wanderValue) * wanderCoef;
        }
        else
        {
            wanderValue *= 0.9999f;
        }

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
        const float wanderOct = wanderValue * 0.4f * params.drift; // ≈ ±0.14 oct at the default
        const float oct = baseOct + megV * envOct * (accentOn ? 0.6f : 1.0f) + sweep * 3.2f + wanderOct;
        filter.setCutoff (170.0 * std::exp2 ((double) oct));

        float o = osc.process (params.waveform == 0 ? Oscillator::Wave::Saw : Oscillator::Wave::Square);
        if (preMode == 1) o = preDrive.process (o) * 0.85f;
        else if (preMode == 2) o = preGrind.process (o) * 0.85f;
        float y = filter.process (o * 0.9f);
        y *= 1.0f + 0.5f * params.resonance; // partial loudness compensation

        // Underground: a clean sine one octave down, level on the character knob
        if (params.world == 2)
        {
            subPhase += freq * 0.5 / sr;
            subPhase -= std::floor (subPhase);
            y += (float) std::sin (subPhase * 6.283185307179586) * params.drift * 0.5f;
        }

        const float ampV = amp.process();
        const float accentAmt = accentS * (params.world == 1 ? 0.8f : (params.world == 2 ? 1.1f : 1.0f));
        const float accentGain = 1.0f + (accentOn ? accentAmt * (0.35f + 0.9f * accV) : 0.0f);
        const float worldTrim = params.world == 2 ? 0.88f : 1.0f; // keep Underground (sub + accent) level with Classic
        return y * ampV * accentGain * 0.55f * worldTrim;
    }

private:
    /** Underground uses the character knob for the sub, so pitch drift stays subtle there. */
    float pitchDrift() const { return params.world == 2 ? 0.05f : params.drift; }

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
    float glideCoef = 0.001f, glideCoefLong = 0.001f;
    Overdrive preDrive;
    Grind preGrind;
    int preMode = 0;
    double subPhase = 0.0;
    int wanderCounter = 0;
    float wanderTarget = 0.0f, wanderValue = 0.0f, wanderCoef = 0.0001f;
    float sweep = 0.0f, sweepChargeCoef = 0.001f, sweepDischargeCoef = 0.0001f;
    float cutoffS = 0.35f, envModS = 0.5f, accentS = 0.6f, paramSmoothCoef = 0.01f;

    uint32_t rng = 0x1234567u;
    int driftCounter = 0;
    float driftTarget = 0.0f, driftValue = 0.0f, driftCoef = 0.0001f;
};

} // namespace acid::dsp
