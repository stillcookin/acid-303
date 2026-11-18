#include "PatternController.h"
#include "MidiExport.h"

namespace acid
{

PatternController::PatternController (PatternMailbox& mailboxToUse)
    : mailbox (mailboxToUse)
{
    // factory content: every slot starts with a different generated line
    for (int s = 0; s < PatternBank::numSlots; ++s)
        bank.slots[(size_t) s] = Generator::generate (Pattern {}, genSettings, 303u + (uint32_t) s * 7919u);
    mailbox.publish (pattern());
}

// ---------------------------------------------------------------- internals

void PatternController::pushUndo()
{
    if (inGesture)
    {
        if (gestureRecorded) return;
        gestureRecorded = true;
    }
    undoStack.push_back ({ slot, pattern() });
    if (undoStack.size() > maxHistory)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void PatternController::commit (const Pattern& newPattern)
{
    if (newPattern == pattern())
        return;
    pushUndo();
    bank.slots[(size_t) slot] = newPattern;
    publishAndNotify();
}

void PatternController::modify (const std::function<void (Pattern&)>& fn)
{
    Pattern p = pattern();
    fn (p);
    commit (p);
}

void PatternController::publishAndNotify()
{
    mailbox.publish (pattern());
    notifyPattern();
}

void PatternController::notifyPattern()
{
    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    if (mm == nullptr || mm->isThisTheMessageThread())
    {
        listeners.call ([] (Listener& l) { l.patternChanged(); });
        return;
    }
    juce::WeakReference<PatternController> weak (this);
    juce::MessageManager::callAsync ([weak]
    {
        if (auto* self = weak.get())
            self->listeners.call ([] (Listener& l) { l.patternChanged(); });
    });
}

uint32_t PatternController::nextSeed()
{
    return (uint32_t) random.nextInt() ^ (++opCounter * 2654435761u);
}

// ---------------------------------------------------------------- editing

void PatternController::beginGesture() { inGesture = true; gestureRecorded = false; }
void PatternController::endGesture() { inGesture = false; gestureRecorded = false; }

void PatternController::toggleNote (int step, int semitone)
{
    modify ([&] (Pattern& p)
    {
        auto& s = p[step];
        if (s.gate == Gate::Note && s.semitone == semitone)
        {
            s.gate = Gate::Rest;
        }
        else
        {
            s.gate = Gate::Note;
            s.semitone = (int8_t) juce::jlimit (0, 12, semitone);
        }
        Generator::tidy (p);
    });
}

void PatternController::setNote (int step, int semitone)
{
    modify ([&] (Pattern& p)
    {
        auto& s = p[step];
        s.gate = Gate::Note;
        s.semitone = (int8_t) juce::jlimit (0, 12, semitone);
    });
}

void PatternController::setStep (int step, const Step& st)
{
    modify ([&] (Pattern& p) { p[step] = st; });
}

void PatternController::setGate (int step, Gate g)
{
    modify ([&] (Pattern& p) { p[step].gate = g; Generator::tidy (p); });
}

void PatternController::toggleAccent (int step)
{
    modify ([&] (Pattern& p)
    {
        auto& s = p[step];
        s.accent = ! s.accent;
        if (s.accent && s.gate == Gate::Rest) s.gate = Gate::Note;
    });
}

void PatternController::toggleSlide (int step)
{
    modify ([&] (Pattern& p)
    {
        auto& s = p[step];
        s.slide = ! s.slide;
        if (s.slide && s.gate == Gate::Rest) s.gate = Gate::Note;
    });
}

void PatternController::toggleTie (int step)
{
    modify ([&] (Pattern& p)
    {
        auto& s = p[step];
        s.gate = s.gate == Gate::Tie ? Gate::Note : Gate::Tie;
        Generator::tidy (p);
    });
}

void PatternController::toggleLock (int step)
{
    modify ([&] (Pattern& p) { p[step].locked = ! p[step].locked; });
}

void PatternController::setOctave (int step, int octave)
{
    modify ([&] (Pattern& p) { p[step].octave = (int8_t) juce::jlimit (Pattern::minOctave, Pattern::maxOctave, octave); });
}

void PatternController::cycleOctave (int step, int direction)
{
    modify ([&] (Pattern& p)
    {
        int o = p[step].octave + (direction >= 0 ? 1 : -1);
        if (o > Pattern::maxOctave) o = Pattern::minOctave;
        if (o < Pattern::minOctave) o = Pattern::maxOctave;
        p[step].octave = (int8_t) o;
    });
}

void PatternController::setLength (int numSteps)
{
    modify ([&] (Pattern& p) { p.length = juce::jlimit (1, Pattern::maxSteps, numSteps); });
}

void PatternController::clearPattern()
{
    modify ([] (Pattern& p)
    {
        for (auto& s : p.steps)
            if (! s.locked) s = Step {};
    });
}

void PatternController::setPattern (const Pattern& p) { commit (p); }

// ---------------------------------------------------------------- generator

void PatternController::setGeneratorSettings (const GeneratorSettings& s)
{
    if (s == genSettings) return;
    genSettings = s;
    listeners.call ([] (Listener& l) { l.generatorSettingsChanged(); });
}

void PatternController::generate() { generateWithSeed (nextSeed()); }

void PatternController::generateWithSeed (uint32_t seed)
{
    genSettings.seed = seed;
    commit (Generator::generate (pattern(), genSettings, seed));
    listeners.call ([] (Listener& l) { l.generatorSettingsChanged(); });
}

void PatternController::mutate()            { commit (Generator::mutate (pattern(), genSettings, nextSeed())); }
void PatternController::randomizeAccents()  { commit (Generator::randomizeAccents (pattern(), genSettings, nextSeed())); }
void PatternController::randomizeSlides()   { commit (Generator::randomizeSlides (pattern(), genSettings, nextSeed())); }
void PatternController::randomizeOctaves()  { commit (Generator::randomizeOctaves (pattern(), genSettings, nextSeed())); }
void PatternController::shift (int dir)     { commit (Generator::shift (pattern(), dir)); }
void PatternController::reverse()           { commit (Generator::reverse (pattern())); }

// ---------------------------------------------------------------- history & slots

void PatternController::undo()
{
    if (undoStack.empty()) return;
    auto e = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back ({ e.slot, bank.slots[(size_t) e.slot] });
    bank.slots[(size_t) e.slot] = e.pattern;
    slot = e.slot;
    publishAndNotify();
}

void PatternController::redo()
{
    if (redoStack.empty()) return;
    auto e = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back ({ e.slot, bank.slots[(size_t) e.slot] });
    bank.slots[(size_t) e.slot] = e.pattern;
    slot = e.slot;
    publishAndNotify();
}

void PatternController::selectSlot (int newSlot)
{
    newSlot = juce::jlimit (0, PatternBank::numSlots - 1, newSlot);
    if (newSlot == slot) return;
    slot = newSlot;
    publishAndNotify();
}

void PatternController::copySlot (int from, int to)
{
    from = juce::jlimit (0, PatternBank::numSlots - 1, from);
    to = juce::jlimit (0, PatternBank::numSlots - 1, to);
    if (from == to) return;
    const int prev = slot;
    slot = to;
    commit (bank.slots[(size_t) from]);
    slot = prev;
    publishAndNotify();
}

// ---------------------------------------------------------------- persistence

juce::ValueTree PatternController::toValueTree() const
{
    juce::ValueTree v ("Patterns");
    v.setProperty ("slot", slot, nullptr);
    for (const auto& p : bank.slots)
        v.appendChild (p.toValueTree(), nullptr);
    v.appendChild (genSettings.toValueTree(), nullptr);
    return v;
}

void PatternController::fromValueTree (const juce::ValueTree& v)
{
    if (! v.hasType ("Patterns"))
        return;

    int idx = 0;
    for (auto child : v)
    {
        if (child.hasType ("Pattern") && idx < PatternBank::numSlots)
            bank.slots[(size_t) idx++] = Pattern::fromValueTree (child);
        else if (child.hasType ("Generator"))
            genSettings = GeneratorSettings::fromValueTree (child);
    }
    slot = juce::jlimit (0, PatternBank::numSlots - 1, (int) v.getProperty ("slot", 0));
    undoStack.clear();
    redoStack.clear();
    publishAndNotify();
    listeners.call ([] (Listener& l) { l.generatorSettingsChanged(); });
}

bool PatternController::exportMidi (const juce::File& file, int repeats, double gate, int transpose) const
{
    return MidiExport::writeToFile (pattern(), file, repeats, gate, transpose);
}

} // namespace acid
