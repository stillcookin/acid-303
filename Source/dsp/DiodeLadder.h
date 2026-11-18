#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace acid::dsp
{

/** Zero-delay-feedback 4-pole ladder tuned towards the 303 diode ladder.

    - Four TPT one-pole low-passes whose cutoffs are not equal (the first stage
      sits higher), which gives the ~18 dB/oct 303 slope and a softer peak.
    - Resonance feedback passes through a one-pole high-pass, so as resonance
      rises the low end thins out (the classic 303 "bass loss").
    - A tanh saturator on the summed input keeps self-oscillation bounded and
      adds the squelchy drive.
    The linear feedback loop is solved implicitly each sample (no unit delay). */
class DiodeLadder
{
public:
    DiodeLadder() { computeMaxFeedback(); }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
        setFeedbackHighpass (120.0);
    }

    void reset()
    {
        s.fill (0.0);
        hpS = 0.0;
    }

    void setFeedbackHighpass (double hz)
    {
        const double g = std::tan (pi * std::min (hz, 0.45 * sr) / sr);
        hpG = g / (1.0 + g);
    }

    /** resonance 0..1; 1 ≈ edge of self-oscillation. */
    void setResonance (float r)
    {
        r = std::clamp (r, 0.0f, 1.0f);
        k = kMax * 0.985 * (double) r;
    }

    void setCutoff (double hz)
    {
        const double nyq = 0.48 * sr;
        for (size_t i = 0; i < 4; ++i)
        {
            const double f = std::clamp (hz * poleRatio[i], 10.0, nyq);
            const double g = std::tan (pi * f / sr);
            G[i] = g / (1.0 + g);
        }
    }

    float process (float xIn)
    {
        const double x = (double) xIn;

        // Express y4 = gamma * u + sigma, where u is the ladder input.
        double gamma = 1.0, sigma = 0.0;
        for (size_t i = 0; i < 4; ++i)
        {
            // stage: y = G x + (1 - G) s
            sigma = G[i] * sigma + (1.0 - G[i]) * s[i];
            gamma *= G[i];
        }

        // feedback high-pass: hp(y) = (1 - hpG) y - (1 - hpG) hpS
        const double a = 1.0 - hpG;
        const double hpOffset = -(1.0 - hpG) * hpS;
        double u = (x - k * (a * sigma + hpOffset)) / (1.0 + k * a * gamma);

        u = std::tanh (u);

        double y = u;
        for (size_t i = 0; i < 4; ++i)
        {
            const double v = (y - s[i]) * G[i];
            const double out = v + s[i];
            s[i] = out + v;
            y = out;
        }

        // update feedback HPF state (TPT one-pole low-pass inside the HPF)
        {
            const double v = (y - hpS) * hpG;
            const double lp = v + hpS;
            hpS = lp + v;
        }

        // flush denormals in the states
        for (auto& st : s)
            if (std::abs (st) < 1.0e-20) st = 0.0;

        return (float) y;
    }

    double maxFeedback() const { return kMax; }

private:
    void computeMaxFeedback()
    {
        // find w where total phase of the 4 poles = -180 degrees, then k = 1/|H(w)|
        auto phase = [this] (double w)
        {
            double p = 0.0;
            for (auto r : poleRatio) p += std::atan (w / r);
            return p;
        };
        double lo = 0.0, hi = 1000.0;
        for (int it = 0; it < 100; ++it)
        {
            const double mid = 0.5 * (lo + hi);
            (phase (mid) < pi ? lo : hi) = mid;
        }
        double mag = 1.0;
        for (auto r : poleRatio)
            mag /= std::sqrt (1.0 + (lo / r) * (lo / r));
        kMax = 1.0 / mag;
    }

    static constexpr double pi = 3.14159265358979323846;
    static constexpr std::array<double, 4> poleRatio { 2.0, 1.0, 1.0, 1.0 };

    double sr = 44100.0;
    std::array<double, 4> G { 0.5, 0.5, 0.5, 0.5 };
    std::array<double, 4> s {};
    double hpG = 0.0, hpS = 0.0;
    double k = 0.0, kMax = 4.0;
};

} // namespace acid::dsp
