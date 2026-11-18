#include "Sequencer.h"

#include <algorithm>
#include <cmath>

namespace acid
{

double Sequencer::stepStartPpq (long long k) const
{
    const double base = (double) k * stepLengthPpq;
    const bool odd = (k & 1) != 0;
    return odd ? base + (double) swing * 0.5 * stepLengthPpq : base;
}

int Sequencer::stepAt (const Pattern& pattern, double ppq) const
{
    auto k = (long long) std::floor (ppq / stepLengthPpq);
    if (ppq < stepStartPpq (k))
        --k;
    return (int) (((k % pattern.length) + pattern.length) % pattern.length);
}

int Sequencer::process (const Pattern& pattern, const TransportInfo& t, SeqEvent* out, int maxEvents)
{
    int count = 0;
    auto push = [&] (const SeqEvent& e)
    {
        if (count < maxEvents)
            out[count++] = e;
    };

    if (t.numSamples <= 0 || t.bpm <= 0.0 || t.sampleRate <= 0.0)
        return 0;

    if (! t.isPlaying)
    {
        if (wasPlaying)
            push ({ 0, SeqEvent::Type::NoteOff });
        wasPlaying = false;
        return count;
    }
    wasPlaying = true;

    const double samplesPerPpq = t.sampleRate * 60.0 / t.bpm;
    const double ppqStart = t.ppqPosition;
    const double ppqEnd = ppqStart + (double) t.numSamples / samplesPerPpq;

    auto inRange = [&] (double ppq) { return ppq >= ppqStart && ppq < ppqEnd; };
    auto offsetOf = [&] (double ppq)
    {
        return std::clamp ((int) std::floor ((ppq - ppqStart) * samplesPerPpq + 1.0e-6), 0, t.numSamples - 1);
    };

    const auto kFirst = (long long) std::floor (ppqStart / stepLengthPpq) - 2;
    const auto kLast = (long long) std::floor (ppqEnd / stepLengthPpq) + 1;
    const double gateFrac = std::clamp ((double) gate, 0.05, 0.97);

    for (auto k = kFirst; k <= kLast; ++k)
    {
        const int i = (int) (((k % pattern.length) + pattern.length) % pattern.length);
        const double start = stepStartPpq (k);
        const double next = stepStartPpq (k + 1);
        const auto eff = pattern.effectiveGate (i);
        const auto& step = pattern[i];

        if (inRange (start))
        {
            if (eff == Gate::Rest)
            {
                push ({ offsetOf (start), SeqEvent::Type::NoteOff, 0, false, false, i });
            }
            else if (eff == Gate::Note)
            {
                const bool slideIn = pattern.effectiveGate (i - 1) != Gate::Rest && pattern[i - 1].slide;
                push ({ offsetOf (start), SeqEvent::Type::NoteOn, pattern.midiNoteOf (i) + transpose,
                        step.accent, slideIn, i });
            }
        }

        if (eff != Gate::Rest)
        {
            const bool holds = step.slide || pattern[i + 1].gate == Gate::Tie;
            if (! holds)
            {
                const double off = start + gateFrac * (next - start);
                if (inRange (off))
                    push ({ offsetOf (off), SeqEvent::Type::NoteOff, 0, false, false, i });
            }
        }
    }

    // insertion sort: by offset, NoteOff before NoteOn at the same sample
    for (int a = 1; a < count; ++a)
    {
        auto e = out[a];
        int b = a - 1;
        while (b >= 0 && (out[b].sampleOffset > e.sampleOffset
                          || (out[b].sampleOffset == e.sampleOffset && out[b].type > e.type)))
        {
            out[b + 1] = out[b];
            --b;
        }
        out[b + 1] = e;
    }
    return count;
}

} // namespace acid
