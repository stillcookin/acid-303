// Offline renderer: generates (or loads the factory) pattern and renders it to a WAV
// file through the full Engine. Useful for listening tests and regression checks
// without a DAW.
//
//   acid_render out.wav [--seed N] [--bars N] [--bpm N] [--scale N] [--root N]
//                       [--cutoff 0..1] [--reso 0..1] [--envmod 0..1] [--decay 0..1]
//                       [--accent 0..1] [--drive 0..1] [--delay] [--reverb] [--square]
//                       [--world 0|1|2] [--alt]   (Classic, Space, Underground; alt = chain toggle)

#include "core/Engine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <iostream>

using namespace acid;

int main (int argc, char* argv[])
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);

    if (args.isEmpty() || args[0].startsWith ("-"))
    {
        std::cerr << "usage: acid_render out.wav [--seed N] [--bars N] [--bpm N] [--scale N] [--root N]\n"
                     "       [--cutoff x] [--reso x] [--envmod x] [--decay x] [--accent x] [--drive x]\n"
                     "       [--delay] [--reverb] [--square] [--world 0|1|2] [--alt]\n";
        return 1;
    }

    auto value = [&] (const char* name, double def)
    {
        const int i = args.indexOf (name);
        return i >= 0 && i + 1 < args.size() ? args[i + 1].getDoubleValue() : def;
    };
    auto flag = [&] (const char* name) { return args.contains (name); };

    const double sr = 48000.0;
    const int block = 256;
    const double bpm = value ("--bpm", 126.0);
    const int bars = (int) value ("--bars", 4.0);

    PatternMailbox mailbox;
    LiveState live;
    PatternController patterns (mailbox);
    Engine engine (mailbox, live);

    const auto world = worldFromIndex ((int) value ("--world", 0));
    patterns.setWorld (world);

    auto gs = patterns.generatorSettings();
    gs.scale = args.contains ("--scale") ? (int) value ("--scale", 0) : gs.scale;
    gs.root = (int) value ("--root", 0);
    patterns.setGeneratorSettings (gs);
    if (args.contains ("--seed"))
        patterns.generateWithSeed ((uint32_t) value ("--seed", 303));

    auto p = EngineParams::defaults();
    p.cutoff = (float) value ("--cutoff", p.cutoff);
    p.resonance = (float) value ("--reso", p.resonance);
    p.envMod = (float) value ("--envmod", p.envMod);
    p.decay = (float) value ("--decay", p.decay);
    p.accent = (float) value ("--accent", p.accent);
    p.drive = (float) value ("--drive", p.drive);
    p.delayOn = flag ("--delay");
    p.reverbOn = flag ("--reverb");
    p.waveform = flag ("--square") ? 1 : 0;
    p.world = world;
    p.worldAlt = flag ("--alt");
    engine.setParams (p);
    engine.prepare (sr, block);

    const int totalSamples = (int) (bars * 4 * 60.0 / bpm * sr);
    juce::AudioBuffer<float> out (2, totalSamples + (int) sr);
    juce::AudioBuffer<float> buf (2, block);

    double ppq = 0.0;
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        const int n = std::min (block, out.getNumSamples() - pos);
        buf.setSize (2, n, false, false, true);
        buf.clear();
        const bool playing = pos < totalSamples;
        engine.process (buf, {}, TransportInfo { playing, ppq, bpm, sr, n });
        ppq += n / sr * bpm / 60.0;
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, pos, buf, c, 0, n);
    }

    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (args[0]);
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (stream == nullptr)
    {
        std::cerr << "cannot write " << file.getFullPathName() << "\n";
        return 1;
    }
    auto opts = juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24);
    auto writer = wav.createWriterFor (stream, opts);
    if (writer == nullptr)
        return 1;
    writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
    writer.reset();

    std::cout << "wrote " << file.getFullPathName() << " (" << bars << " bars @ " << bpm
              << " BPM, peak " << out.getMagnitude (0, out.getNumSamples()) << ")\n";
    return 0;
}
