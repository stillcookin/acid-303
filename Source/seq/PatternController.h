#pragma once

#include "Generator.h"
#include "Pattern.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <vector>

namespace acid
{

/** Single-producer (message thread) / single-consumer (audio thread) handoff
    of the currently playing pattern. The audio thread never blocks: if the
    lock is busy it keeps playing its previous copy for one more block. */
class PatternMailbox
{
public:
    void publish (const Pattern& p)
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pending = p;
        dirty.store (true, std::memory_order_release);
    }

    /** Audio thread: refreshes `dst` if a new pattern is available. */
    void pull (Pattern& dst)
    {
        if (! dirty.load (std::memory_order_acquire))
            return;
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked())
        {
            dst = pending;
            dirty.store (false, std::memory_order_relaxed);
        }
    }

private:
    juce::SpinLock lock;
    Pattern pending;
    std::atomic<bool> dirty { false };
};

/** The editing API for patterns. This is the ONLY interface a UI needs to
    read and change sequencer content (besides plain parameters), so any front
    end — the bundled JUCE editor, a redesigned one, or a WebView UI — can be
    built on top of it without touching the engine.

    Message-thread only. Every mutating call records undo, publishes the
    pattern to the audio thread and notifies listeners. */
class PatternController
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void patternChanged() {}             ///< current pattern content or slot changed
        virtual void generatorSettingsChanged() {}
    };

    explicit PatternController (PatternMailbox& mailboxToUse);

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // ---- reading -------------------------------------------------------
    const Pattern& pattern() const { return bank.slots[(size_t) slot]; }
    const Pattern& patternInSlot (int s) const { return bank.slots[(size_t) juce::jlimit (0, PatternBank::numSlots - 1, s)]; }
    int currentSlot() const { return slot; }
    const GeneratorSettings& generatorSettings() const { return genSettings; }

    // ---- step editing --------------------------------------------------
    /** Group several edits (e.g. a mouse drag) into one undo step. */
    void beginGesture();
    void endGesture();

    /** Piano-roll click: sets the step's note; clicking the same note again rests it. */
    void toggleNote (int step, int semitone);
    void setNote (int step, int semitone);           ///< sets semitone and makes the step a Note
    void setStep (int step, const Step& s);
    void setGate (int step, Gate g);
    void toggleAccent (int step);
    void toggleSlide (int step);
    void toggleTie (int step);
    void toggleLock (int step);
    void setOctave (int step, int octave);
    void cycleOctave (int step, int direction);
    void setLength (int numSteps);
    void clearPattern();
    void setPattern (const Pattern& p);              ///< replace current slot's pattern

    // ---- generator -----------------------------------------------------
    void setGeneratorSettings (const GeneratorSettings& s);
    void generate();                                  ///< new random seed
    void generateWithSeed (uint32_t seed);
    void mutate();
    void randomizeAccents();
    void randomizeSlides();
    void randomizeOctaves();
    void shift (int direction);
    void reverse();

    // ---- history & slots -----------------------------------------------
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo();
    void redo();
    void selectSlot (int newSlot);
    void copySlot (int from, int to);

    // ---- persistence & export -----------------------------------------
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);
    bool exportMidi (const juce::File& file, int repeats, double gate, int transpose = 0) const;

private:
    struct HistoryEntry { int slot; Pattern pattern; };

    void modify (const std::function<void (Pattern&)>& fn);
    void commit (const Pattern& newPattern);
    void pushUndo();
    void publishAndNotify();
    void notifyPattern();
    uint32_t nextSeed();

    PatternMailbox& mailbox;
    PatternBank bank;
    int slot = 0;
    GeneratorSettings genSettings;
    std::vector<HistoryEntry> undoStack, redoStack;
    bool inGesture = false, gestureRecorded = false;
    uint32_t opCounter = 0;
    juce::Random random;
    juce::ListenerList<Listener> listeners;

    static constexpr size_t maxHistory = 100;

    JUCE_DECLARE_WEAK_REFERENCEABLE (PatternController)
    JUCE_DECLARE_NON_COPYABLE (PatternController)
};

} // namespace acid
