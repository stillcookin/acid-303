#pragma once

#include <cmath>

namespace acid::dsp
{

/** Band-limited (PolyBLEP) saw / square oscillator with 303-style colouring.
    The square is not a clean pulse: the 303 derives it from the saw through a
    transistor waveshaper and AC coupling, which gives a rounded edge and a
    sagging top. We approximate that with a soft-clipped PolyBLEP pulse fed
    through a gentle one-pole high-pass (coupling cap) and low-pass. */
class Oscillator
{
public:
    enum class Wave { Saw = 0, Square = 1 };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
        const double pi = 3.14159265358979323846;
        hpCoef = (float) std::exp (-2.0 * pi * 45.0 / sr);    // coupling cap → top sag
        lpCoef = (float) (1.0 - std::exp (-2.0 * pi * 9000.0 / sr)); // edge rounding
    }

    void reset() { phase = 0.0; hpX = hpY = 0.0f; lpY = 0.0f; }

    void setFrequency (double hz) { inc = hz / sr; if (inc > 0.49) inc = 0.49; }

    float process (Wave wave)
    {
        const double t = phase;
        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;

        float out;
        if (wave == Wave::Saw)
        {
            // 303 saw ramps down; slight curvature from the charging cap.
            float s = (float) (1.0 - 2.0 * t);
            s += (float) polyBlep (t, inc);
            out = s - 0.08f * (s * s - 0.333f);
        }
        else
        {
            float sq = t < 0.5 ? 1.0f : -1.0f;
            sq += (float) polyBlep (t, inc);
            sq -= (float) polyBlep (std::fmod (t + 0.5, 1.0), inc);

            // coupling high-pass → sloped top
            const float hp = hpCoef * (hpY + sq - hpX);
            hpX = sq;
            hpY = hp;
            // edge rounding
            lpY += lpCoef * (hp - lpY);
            out = 0.9f * lpY;
        }
        return out;
    }

private:
    static double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    double sr = 44100.0, phase = 0.0, inc = 0.0;
    float hpCoef = 0.0f, hpX = 0.0f, hpY = 0.0f;
    float lpCoef = 1.0f, lpY = 0.0f;
};

} // namespace acid::dsp
