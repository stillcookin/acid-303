#pragma once

#include "Pattern.h"

namespace acid
{

struct TransportInfo
{
    bool isPlaying = false;
    double ppqPosition = 0.0; ///< quarter notes at the first sample of the block
    double bpm = 120.0;
    double sampleRate = 44100.0;
    int numSamples = 0;
};

struct SeqEvent
{
    enum class Type : uint8_t { NoteOff = 0, NoteOn = 1 };

    int sampleOffset = 0;
    Type type = Type::NoteOff;
    int note = 0;
    bool accent = false;
    bool slide = false;
    int step = 0;
};

/** Converts a Pattern + host transport into sample-accurate voice events.
    Stateless with respect to time: events depend only on the PPQ range of the
    block, so results are identical regardless of block size and survive
    loops / jumps in the host timeline. */
class Sequencer
{
public:
    static constexpr double stepLengthPpq = 0.25; // 16th notes

    void setSwing (float swing01) { swing = swing01; }
    void setGate (float gate01) { gate = gate01; }
    void setTranspose (int semis) { transpose = semis; }

    /** Writes up to maxEvents events (sorted by offset, offs before ons),
        returns the number written. */
    int process (const Pattern& pattern, const TransportInfo& t, SeqEvent* out, int maxEvents);

    /** PPQ at which global step k starts (including swing). */
    double stepStartPpq (long long k) const;

    /** Step index (within the pattern) playing at the given PPQ. */
    int stepAt (const Pattern& pattern, double ppq) const;

    void reset() { wasPlaying = false; }

private:
    float swing = 0.0f, gate = 0.55f;
    int transpose = 0;
    bool wasPlaying = false;
};

} // namespace acid
