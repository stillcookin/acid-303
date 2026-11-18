#include "MidiExport.h"

#include <algorithm>

namespace acid::MidiExport
{

juce::MidiFile toMidiFile (const Pattern& p, int repeats, double gate, int transpose)
{
    juce::MidiMessageSequence seq;
    const int total = p.length * std::max (1, repeats);
    const int gateTicks = (int) (std::clamp (gate, 0.05, 0.97) * ticksPerStep);
    const int overlap = ticksPerStep / 8;

    int i = 0;
    while (i < total)
    {
        if (p.effectiveGate (i) != Gate::Note)
        {
            ++i;
            continue;
        }

        const int start = i * ticksPerStep;
        const int note = std::clamp (p.midiNoteOf (i) + transpose, 0, 127);
        const auto vel = (juce::uint8) (p[i].accent ? 127 : 90);

        // extend through ties; a slide means "hold into the next step"
        int last = i;
        while (last + 1 < total && p[last + 1].gate == Gate::Tie)
            ++last;

        int end;
        if (p[last].slide && p.effectiveGate (last + 1) != Gate::Rest)
            end = (last + 1) * ticksPerStep + overlap;      // legato into next note
        else if (p[last].slide)
            end = (last + 1) * ticksPerStep;                // held to the end of the step
        else
            end = last * ticksPerStep + gateTicks;

        seq.addEvent (juce::MidiMessage::noteOn (1, note, vel), start);
        seq.addEvent (juce::MidiMessage::noteOff (1, note), end);
        i = last + 1;
    }

    seq.updateMatchedPairs();

    juce::MidiMessageSequence meta;
    meta.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0);
    meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    meta.addEvent (juce::MidiMessage::textMetaEvent (3, "Acid303 pattern"), 0);
    seq.addSequence (meta, 0);
    seq.sort();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);
    file.addTrack (seq);
    return file;
}

bool writeToFile (const Pattern& p, const juce::File& f, int repeats, double gate, int transpose)
{
    auto mf = toMidiFile (p, repeats, gate, transpose);
    f.deleteFile();
    juce::FileOutputStream out (f);
    if (! out.openedOk())
        return false;
    return mf.writeTo (out);
}

} // namespace acid::MidiExport
