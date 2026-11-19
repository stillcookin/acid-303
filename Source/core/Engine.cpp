#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace acid
{

Engine::Engine (PatternMailbox& mb, LiveState& ls) : mailbox (mb), live (ls) {}

void Engine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    maxBlock = std::max (1, maxBlockSize);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) maxBlock);

    mono.setSize (1, maxBlock);
    stereo.setSize (2, maxBlock);

    voice.prepare (sr * osFactor);
    drive.prepare (sr * osFactor);
    delay.prepare (sr, 4.0);
    reverb.prepare (sr);

    volume.reset (sr, 0.02);
    volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.volumeDb));
    reset();
}

void Engine::reset()
{
    if (oversampler) oversampler->reset();
    voice.reset();
    drive.reset();
    delay.reset();
    reverb.reset();
    sequencer.reset();
    numHeld = 0;
    previewWasOn = false;
    pendingWorld.reset();
    live.pendingWorld = -1;
    live.playingStep = -1;
    live.running = false;
}

int Engine::getLatencySamples() const
{
    return oversampler ? (int) std::lround (oversampler->getLatencyInSamples()) : 0;
}

void Engine::addEvent (const VoiceEvent& e)
{
    if (numEvents < maxEvents)
        events[(size_t) numEvents++] = e;
}

void Engine::sortEvents()
{
    std::stable_sort (events.begin(), events.begin() + numEvents,
                      [] (const VoiceEvent& a, const VoiceEvent& b) { return a.offset < b.offset; });
}

dsp::VoiceParams Engine::voiceParams() const
{
    return { params.tune, params.waveform, params.cutoff, params.resonance,
             params.envMod, params.decay, params.accent, params.drift, indexOf (activeWorld) };
}

void Engine::requestWorld (World w, bool fromMidi)
{
    if (fromMidi)
    {
        live.midiWorldRequest = indexOf (w);
        live.midiWorldSerial.fetch_add (1);
    }
    if (w == activeWorld)
        pendingWorld.reset();
    else
        pendingWorld = w;
    live.pendingWorld = pendingWorld ? indexOf (*pendingWorld) : -1;
}

void Engine::applyWorld (World w)
{
    activeWorld = w;
    pendingWorld.reset();
    voice.setParams (voiceParams());
    live.world = indexOf (w);
    live.pendingWorld = -1;
}

void Engine::scanWorldMidi (const juce::MidiBuffer& midi, int start, int len)
{
    // world switching from MIDI: CC 102, program change 0-2, keyswitch notes 0-2
    if (! params.worldMidi)
        return;
    for (const auto meta : midi)
    {
        if (meta.samplePosition < start || meta.samplePosition >= start + len)
            continue;
        const auto msg = meta.getMessage();
        std::optional<World> req;
        if (msg.isController())
            req = WorldMidi::fromController (msg.getControllerNumber(), msg.getControllerValue());
        else if (msg.isProgramChange())
            req = WorldMidi::fromProgramChange (msg.getProgramChangeNumber());
        else if (msg.isNoteOn())
            req = WorldMidi::fromKeyswitch (msg.getNoteNumber());
        if (req)
            requestWorld (*req, true);
    }
}

void Engine::collectMidi (const juce::MidiBuffer& midi, int start, int len)
{
    for (const auto meta : midi)
    {
        if (meta.samplePosition < start || meta.samplePosition >= start + len)
            continue;

        const auto msg = meta.getMessage();
        const int off = meta.samplePosition - start;

        if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            numHeld = 0;
            if (params.mode == PlayMode::Midi)
                addEvent ({ off, 2, 0, false, false });
            continue;
        }

        // keyswitches (handled in scanWorldMidi) never play or transpose
        if (params.worldMidi && msg.isNoteOnOrOff() && WorldMidi::isKeyswitch (msg.getNoteNumber()))
            continue;

        if (params.mode == PlayMode::Midi)
        {
            if (msg.isNoteOn())
            {
                const bool legato = numHeld > 0;
                const Held h { msg.getNoteNumber(), msg.getVelocity() >= 100 };
                if (numHeld < (int) held.size())
                    held[(size_t) numHeld++] = h;
                addEvent ({ off, 1, h.note, h.accent, legato });
            }
            else if (msg.isNoteOff())
            {
                const int note = msg.getNoteNumber();
                const bool wasTop = numHeld > 0 && held[(size_t) numHeld - 1].note == note;
                for (int i = 0; i < numHeld; ++i)
                {
                    if (held[(size_t) i].note == note)
                    {
                        for (int j = i; j < numHeld - 1; ++j)
                            held[(size_t) j] = held[(size_t) j + 1];
                        --numHeld;
                        break;
                    }
                }
                if (numHeld == 0)
                    addEvent ({ off, 0, 0, false, false });
                else if (wasTop)
                {
                    const auto& top = held[(size_t) numHeld - 1];
                    addEvent ({ off, 1, top.note, top.accent, true }); // glide back to the held note
                }
            }
        }
        else if (params.mode == PlayMode::SequencerTranspose && msg.isNoteOn())
        {
            // classic 303 key-transpose; C3 (MIDI 60) = no transpose. Latches until the next key.
            transpose = msg.getNoteNumber() - 60;
        }
    }
}

void Engine::applyEvent (const VoiceEvent& e)
{
    switch (e.type)
    {
        case 1: voice.noteOn (e.note, e.accent, e.slide); break;
        default: voice.noteOff(); break;
    }
}

void Engine::renderVoice (float* os, int from, int to)
{
    for (int i = from; i < to; ++i)
        os[i] = drive.process (voice.process());
}

void Engine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi,
                      std::optional<TransportInfo> host)
{
    const int total = buffer.getNumSamples();
    if (total <= 0 || oversampler == nullptr)
        return;

    mailbox.pull (patterns);

    // world parameter: the first block adopts it, later changes are requests
    if (! worldInitialised)
    {
        worldInitialised = true;
        lastParamWorld = params.world;
        applyWorld (params.world);
    }
    else if (params.world != lastParamWorld)
    {
        lastParamWorld = params.world;
        requestWorld (params.world, false);
    }

    // per-block parameter updates
    voice.setParams (voiceParams());
    drive.setParams (params.drive, params.driveTone, params.driveOn ? params.driveMix : 0.0f);
    sequencer.setSwing (params.swing);
    sequencer.setGate (params.gate);
    reverb.setParams (params.reverbSize, params.reverbDamp, params.reverbOn ? params.reverbMix : 0.0f);
    volume.setTargetValue (juce::Decibels::decibelsToGain (params.volumeDb, -60.0f));

    if (params.mode != lastMode)
    {
        if (lastMode == PlayMode::Midi || params.mode == PlayMode::Midi)
        {
            voice.noteOff();
            numHeld = 0;
        }
        if (params.mode != PlayMode::SequencerTranspose)
            transpose = 0;
        lastMode = params.mode;
    }

    const bool hostPlaying = host.has_value() && host->isPlaying;
    if (host.has_value() && host->bpm > 0.0)
        lastBpm = host->bpm;

    for (int start = 0; start < total;)
    {
        int n = std::min (maxBlock, total - start);
        numEvents = 0;

        // ---- transport: host, internal preview clock, or stopped
        const bool preview = ! hostPlaying && live.previewRequested.load();
        TransportInfo t;
        if (hostPlaying)
        {
            t = *host;
            t.ppqPosition += (double) start * (host->bpm / 60.0) / sampleRate;
        }
        else if (preview)
        {
            if (! previewWasOn) previewPpq = 0.0;
            t.isPlaying = true;
            t.ppqPosition = previewPpq;
            t.bpm = lastBpm;
        }
        previewWasOn = preview;
        t.sampleRate = sampleRate;
        t.bpm = t.bpm > 0.0 ? t.bpm : lastBpm;
        const bool clockRunning = t.isPlaying;

        // ---- worlds: sub-blocks end on beat/bar lines so a pending switch lands exactly on one
        const bool quantized = clockRunning && params.worldQuantize != WorldQuantize::Immediate;
        bool atBoundary = false;
        if (quantized)
        {
            const double div = params.worldQuantize == WorldQuantize::Bar ? 4.0 : 1.0;
            const double samplesPerBeat = sampleRate * 60.0 / t.bpm;
            const double phase = t.ppqPosition - std::floor (t.ppqPosition / div) * div;
            atBoundary = phase * samplesPerBeat < 1.0;
            const double toNext = (div - phase) * samplesPerBeat;
            if (! atBoundary && toNext < (double) n)
                n = std::max (1, (int) std::ceil (toNext));
        }
        if (pendingWorld && (! quantized || atBoundary))
            applyWorld (*pendingWorld);

        scanWorldMidi (midi, start, n);
        if (pendingWorld && ! quantized)
            applyWorld (*pendingWorld);

        t.numSamples = n;
        if (params.mode == PlayMode::Midi)
            t.isPlaying = false;

        collectMidi (midi, start, n);

        sequencer.setTranspose (transpose);
        const int ns = sequencer.process (currentPattern(), t, seqEvents.data(), maxEvents);
        for (int i = 0; i < ns; ++i)
        {
            const auto& e = seqEvents[(size_t) i];
            addEvent ({ e.sampleOffset, e.type == SeqEvent::Type::NoteOn ? 1 : 0, e.note, e.accent, e.slide });
        }
        sortEvents();

        live.running = t.isPlaying;
        live.playingStep = t.isPlaying ? sequencer.stepAt (currentPattern(), t.ppqPosition) : -1;

        if (preview)
            previewPpq += (double) n * (t.bpm / 60.0) / sampleRate;

        // ---- voice + drive at 2x
        mono.clear();
        juce::dsp::AudioBlock<float> monoBlock (mono);
        auto sub = monoBlock.getSubBlock (0, (size_t) n);
        auto up = oversampler->processSamplesUp (sub);
        float* os = up.getChannelPointer (0);

        int pos = 0;
        for (int i = 0; i < numEvents; ++i)
        {
            const int e = std::clamp (events[(size_t) i].offset, 0, n - 1) * osFactor;
            renderVoice (os, pos, e);
            applyEvent (events[(size_t) i]);
            pos = e;
        }
        renderVoice (os, pos, n * osFactor);
        oversampler->processSamplesDown (sub);

        // ---- stereo FX
        float* L = stereo.getWritePointer (0);
        float* R = stereo.getWritePointer (1);
        const float* m = mono.getReadPointer (0);
        std::copy (m, m + n, L);
        std::copy (m, m + n, R);

        const double div = delayDivisionsInBeats()[(size_t) std::clamp (params.delayTime, 0, (int) delayDivisionsInBeats().size() - 1)];
        delay.setParams (div * 60.0 / std::max (20.0, t.bpm), params.delayFeedback, params.delayTone,
                         params.delayOn ? params.delayMix : 0.0f, params.delayPingPong);
        delay.process (L, R, n);
        reverb.process (L, R, n);

        // ---- output
        const int numCh = buffer.getNumChannels();
        for (int i = 0; i < n; ++i)
        {
            const float g = volume.getNextValue();
            L[i] *= g;
            R[i] *= g;
            peak = std::max (peak * 0.9995f, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        if (numCh >= 2)
        {
            buffer.copyFrom (0, start, L, n);
            buffer.copyFrom (1, start, R, n);
            for (int c = 2; c < numCh; ++c)
                buffer.clear (c, start, n);
        }
        else if (numCh == 1)
        {
            buffer.copyFrom (0, start, L, n, 0.5f);
            buffer.addFrom (0, start, R, n, 0.5f);
        }

        start += n;
    }

    live.outputPeak = peak;
    live.hostPlaying = hostPlaying;
    live.bpm = lastBpm;
    live.transpose = transpose;
    live.voiceNote = voice.isGateOn() ? (int) std::lround (voice.getCurrentPitch()) : -1;
}

} // namespace acid
