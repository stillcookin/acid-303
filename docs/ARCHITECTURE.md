# Architecture

The code is split so that the **sound engine and all sequencing logic are independent
of the user interface**. You can redesign or replace the front end without touching the
engine, and the engine is unit-tested headlessly.

```
                 ┌──────────────────────────── acid_core (no GUI) ───────────────────────────┐
 host ─ MIDI ──► │ Engine ── Sequencer ──► Voice303 ─► Overdrive ─(2× OS)─► Delay ─► Reverb │ ─► audio
 transport       │   ▲  ▲                                                                     │
 params ──────►  │   │  └─ EngineParams (plain struct, built from ParamSpec order)            │
                 │   └──── PatternMailbox ◄── PatternController ◄─┐   LiveState (atomics) ─┐  │
                 └────────────────────────────────────────────────┼────────────────────────┼──┘
                                                                  │                        │
                 plugin/AcidProcessor (APVTS, state, playhead)    │                        │
                                                                  │      UiContext         │
                 ui/ (any front end) ─────────────────────────────┴── params / patterns / live
```

## Layers

| Layer | Folder | Depends on | Notes |
|---|---|---|---|
| DSP | `Source/dsp` | std + `juce_audio_basics` (reverb) | header-only building blocks |
| Sequencing | `Source/seq` | `juce_core`, `juce_data_structures`, `juce_events` | pattern model, generator, editing API |
| Engine | `Source/core` | dsp + seq + `juce_dsp` | `Engine::process(buffer, midi, transport)` |
| Plugin | `Source/plugin` | core + `juce_audio_processors` | thin host adapter |
| UI | `Source/ui` | core + JUCE GUI | bundled interface, fully replaceable |

`acid_core` is a CMake INTERFACE library listing only non-GUI JUCE modules. The test
target links nothing else, so if core code ever includes a GUI header the tests stop
building.

## The UI contract: `UiContext`

A front end receives exactly one thing, `acid::ui::UiContext` (`Source/ui/UiContext.h`):

- **`params`** (`juce::AudioProcessorValueTreeState&`): every automatable parameter.
  Attach controls by ID: `acid::idOf (acid::Param::Cutoff)`. The full list, with ranges,
  defaults, display names, groups and choice labels, is the declarative table in
  `Source/core/ParamSpec.cpp`, so a new UI can build its controls from that table, as
  `ParamSection` does.
- **`patterns`** (`acid::PatternController&`): the only way to read or change sequencer
  content. Step edits (`toggleNote`, `setNote`, `toggleAccent`, `toggleSlide`, `toggleTie`,
  `cycleOctave`, `toggleLock`, `setLength`), generator actions (`generate`, `mutate`,
  `randomizeAccents/Slides/Octaves`, `shift`, `reverse`), `undo`/`redo` with
  `beginGesture`/`endGesture` grouping, slots, and `exportMidi`. Register a
  `PatternController::Listener` to be told when to redraw.
- **`live`** (`acid::LiveState&`): lock-free atomics to poll on a timer: playing step,
  running/host state, BPM, transpose, output peak, current note. The UI writes only
  `previewRequested`.

Every `PatternController` call runs on the message thread. It records undo, publishes the
new pattern to the audio thread through `PatternMailbox` (never blocking the audio
thread), and notifies listeners.

## Replacing the UI

1. Write a new root component that takes a `UiContext` (see `ui/MainView`).
2. In `plugin/PluginEditor.h`, replace the `MainView view;` member with your component.
   The editor only hosts and scales it.
3. Remove or keep the old `ui/` files and update `target_sources` in `CMakeLists.txt`.

For a restyle that keeps the layout, edit `ui/Theme.h` (colours, fonts, metrics) and
`ui/AcidLookAndFeel.cpp` (knob, button and combo shapes).

A web-technology UI (JUCE 8 `WebBrowserComponent`) can bind to the same three objects:
expose `ParamSpec` and the APVTS through JUCE's web relays, and wrap the
`PatternController` methods as native functions that send the pattern as JSON on
`patternChanged`.

## Worlds

`seq/World.h` defines the worlds (Classic, Space, Underground), the scales each offers and
the MIDI mapping. The *World* parameter is a request; the `Engine` owns the active world
and applies a request immediately when stopped, or exactly on the next beat/bar line while
the transport runs (it splits the sub-block at that line). MIDI requests (CC 102, program
change, keyswitch notes 0–2) go through the same path and are mirrored into the parameter by
`AcidProcessor`'s timer so the host and UI follow.

`PatternController` stores patterns **as written** and publishes one retuned copy per world
(`WorldPatterns`) through the mailbox, so the engine changes pitches on the bar without
waiting for the message thread, and switching back to Classic restores the original notes.
Edits always apply to the written pattern; `pattern()` returns what plays in the current
world, `sourcePattern()` what was written.

## Threading

| Thread | Touches |
|---|---|
| Audio | `Engine`, reads `EngineParams`, pulls from `PatternMailbox`, writes `LiveState` |
| Message | `PatternController`, UI, `PatternMailbox::publish`, reads `LiveState` |

There are no locks on the audio thread except a `SpinLock` **try**-lock in
`PatternMailbox::pull`, which falls back to the previous pattern for one block if contended.

## Timing model

`Sequencer::process` is stateless with respect to time. It derives every event from the
block's PPQ range (`floor` of step boundaries), so output is identical for any block
size and survives loops, jumps and count-ins (negative PPQ). Swing delays odd 16ths by up
to half a step. Slides hold the gate into the next step and flag that note as `slide`, and
the voice then glides without retriggering its envelopes.

## Tests

`Tests/` covers filter stability across sample rates at maximum resonance, slide timing,
sample-exact step placement, block-size independence, swing, ties, count-in, generator
determinism, scale conformity and lock preservation, MIDI export, controller
undo/gesture/state round-trips, and full-engine renders (host, preview, MIDI mode,
oversized blocks, mono output).
