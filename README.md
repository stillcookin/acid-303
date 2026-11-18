# Acid303

A 303-style acid bassline instrument (VST3 / AU) built for writing and generating
acid lines quickly in Ableton Live: a compact step piano roll with accents and
glides, a seeded pseudo-random pattern generator, a vintage-modelled mono voice and
a built-in overdrive → delay → reverb chain.

![Acid303 UI](docs/screenshot.png)

> Acid303 is an independent project and is not affiliated with Roland Corporation.

## Features

- **Vintage voice**: band-limited saw / square (with the 303's sagging, rounded square),
  zero-delay-feedback diode-style ladder filter with bass-thinning resonance, filter
  envelope (MEG), accent sweep circuit (consecutive accents get brighter), and
  constant-time slide. Runs 2× oversampled. "Age" adds analogue pitch drift.
- **Step piano roll**: 1–32 steps, one octave of notes per step plus octave (-1..+2),
  accent, slide, tie and lock lanes. Glides are drawn between notes. Click to set or clear,
  drag to paint, right-click to rest.
- **Pattern generator**: scale and root aware (minor, phrygian, dorian, harmonic minor,
  minor pentatonic, blues, major, chromatic). Density, accent, slide and octave-jump
  probabilities. Motif-based (call and response) so results sound like basslines rather
  than noise. Also: **Mutate**, re-roll only accents/slides/octaves, shift, reverse,
  per-step locks, undo/redo. Every result has a **seed** you can type back in.
- **16 pattern slots** saved with your Live set (shift-click a slot to copy into it).
- **Host sync**: follows Live's tempo and transport sample-accurately, with swing and gate.
  **Preview** runs the pattern on an internal clock while Live is stopped.
- **Play modes**: *Sequencer*, *Seq + Transpose* (incoming MIDI keys transpose the pattern,
  C3 = no transpose), and *MIDI* (play the voice from clips: overlapping notes slide,
  velocity ≥ 100 = accent).
- **Drag MIDI**: drag the current pattern straight into a Live clip slot.
- **FX**: asymmetric overdrive with tone, tempo-synced delay (1/32 … 1/2, dotted/triplet,
  ping-pong, damping), reverb. All parameters are automatable.

## Installing a build

Download the zip for your platform from the GitHub Actions artifacts or Releases page.

- **macOS**: copy `Acid303.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and/or `Acid303.component`
  to `~/Library/Audio/Plug-Ins/Components/`. The builds are ad-hoc signed (not notarised),
  so clear the quarantine flag first:
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Acid303.vst3`
- **Windows**: copy `Acid303.vst3` to `C:\Program Files\Common Files\VST3\`.

In Live: rescan plugins, drop **Acid303** on a MIDI track and press play.

## Building from source

Requirements: CMake ≥ 3.22, a C++20 compiler (Xcode 14+, Visual Studio 2022, GCC 11+ / Clang 14+).
JUCE and Catch2 are fetched automatically (pinned versions).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Plugins end up in `build/Acid303_artefacts/Release/{VST3,AU,Standalone}`.
On macOS add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for a universal binary.
On Linux install JUCE's dependencies first (see `.github/workflows/build.yml`).

Options: `-DACID_BUILD_PLUGIN=OFF` builds only the headless engine, tests and tools,
which is handy for fast DSP iteration.

### Offline render tool

`acid_render` renders a pattern through the full engine to a WAV file, with no DAW needed:

```bash
./build/acid_render_artefacts/Release/acid_render out.wav --seed 99 --bars 8 --reso 0.9 --envmod 0.8 --delay
```

## Project layout

```
Source/
  core/    Engine, parameter spec, LiveState       ─┐
  dsp/     oscillator, ladder, envelopes, voice, FX  ├─ acid_core: no GUI dependency
  seq/     pattern, sequencer, generator, controller ─┘
  plugin/  AudioProcessor / Editor (thin adapters)
  ui/      the bundled JUCE interface (replaceable)
Tests/     Catch2 unit tests (DSP, timing, generator, state, engine)
tools/     acid_render
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for how the engine and UI are separated
and how to build a new front end.

## License

AGPL-3.0-or-later (see [LICENSE](LICENSE)), as required by JUCE 8's open-source
license. If you want to distribute closed-source builds you need a commercial JUCE license.
