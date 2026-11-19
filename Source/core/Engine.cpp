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
    phase.prepare (sr);
    echo.prepare (sr, 4.0);
    shimmer.prepare (sr, maxBlock);
    grind.prepare (sr * osFactor);
    pong.prepare (sr, 4.0);
    cavern.prepare (sr, maxBlock);
    tail.setSize (2, maxBlock);
    deepFeedback.setSize (2, maxBlock);
    duckBuf.setSize (3, maxBlock);
    tailLength = (int) (sr * 4.0);

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
    phase.reset();
    echo.reset();
    shimmer.reset();
    grind.reset();
    pong.reset();
    cavern.reset();
    deepFeedback.clear();
    duckEnv = 0.0f;
    tailSamples = 0;
    kick = 0.0f;
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
    if (w != activeWorld && worldInitialised)
    {
        // the outgoing world's echoes and reverb keep ringing and fade out
        tailWorld = activeWorld;
        tailAlt = params.worldAlt;
        tailSamples = tailLength;
    }
    if (w == tailWorld)
        tailSamples = 0; // back before the tail ended: its effects simply carry on
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
        case 1:
            voice.noteOn (e.note, e.accent, e.slide);
            if (e.accent) kick = 1.0f;
            live.notePitch = e.note;
            live.noteAccent = e.accent;
            live.noteSlide = e.slide;
            live.noteSerial.fetch_add (1);
            break;
        default: voice.noteOff(); break;
    }
}

void Engine::renderVoice (float* os, int from, int to)
{
    switch (postShaper)
    {
        case 1:  for (int i = from; i < to; ++i) os[i] = drive.process (voice.process()) * worldGain; break;
        case 2:  for (int i = from; i < to; ++i) os[i] = grind.process (voice.process()) * worldGain; break;
        default: for (int i = from; i < to; ++i) os[i] = voice.process() * worldGain; break;
    }
}

namespace
{
    inline float softCeiling (float x) noexcept
    {
        constexpr float knee = 0.6f, room = 0.355f; // knee at 0.6, ceiling 0.955
        const float a = std::abs (x);
        if (a <= knee) return x;
        return std::copysign (knee + room * std::tanh ((a - knee) / room), x);
    }

    // loudness match per world and chain (measured as RMS on a typical pattern);
    // the chain toggle sits about 1 dB above its world's default so it reads as a lift
    constexpr float GAIN_C = 0.86f, GAIN_CH = 0.90f;
    constexpr float GAIN_S = 8.6f,  GAIN_SD = 10.2f;
    constexpr float GAIN_U = 1.12f, GAIN_UM = 1.62f;
}

void Engine::updateFxParams (double bpm)
{
    const auto& p = params;
    const bool alt = p.worldAlt;
    const float driveMix = p.driveOn ? p.driveMix : 0.0f;

    // oversampled stage: where the drive sits, and whether it runs before the filter
    switch (activeWorld)
    {
        case World::Classic:
            // HOT stacks a drive before the filter on top of the usual one after it
            postShaper = 1;
            worldGain = alt ? GAIN_CH : GAIN_C;
            drive.setParams (p.drive, p.driveTone, driveMix);
            voice.setPreShaper (alt ? 1 : 0, p.drive, p.driveTone, driveMix);
            break;
        case World::Underground:
        {
            // Molten Core stacks a grind before the filter on top of the usual one after it
            postShaper = 2;
            worldGain = alt ? GAIN_UM : GAIN_U;
            const float grit = std::min (1.0f, p.drive + (alt ? kick * 0.35f : 0.0f));
            grind.setParams (grit, p.driveTone, driveMix);
            voice.setPreShaper (alt ? 2 : 0, grit, p.driveTone, driveMix);
            break;
        }
        case World::Space:
            postShaper = 0;
            worldGain = alt ? GAIN_SD : GAIN_S; // no distortion stage in Space
            voice.setPreShaper (0, 0, 0, 0);
            break;
    }

    // stereo stage (all worlds, so a fading tail keeps its settings)
    const double div = delayDivisionsInBeats()[(size_t) std::clamp (p.delayTime, 0, (int) delayDivisionsInBeats().size() - 1)];
    const double delaySec = div * 60.0 / std::max (20.0, bpm);
    const float delayMix = p.delayOn ? p.delayMix : 0.0f, reverbMix = p.reverbOn ? p.reverbMix : 0.0f;

    delay.setParams (delaySec, p.delayFeedback, p.delayTone, delayMix, p.delayPingPong);
    reverb.setParams (p.reverbSize, p.reverbDamp, reverbMix);

    phase.setParams (p.drive, p.driveTone, driveMix);                                     // Rate, Depth, Mix
    echo.setParams (delaySec, p.delayFeedback, p.delayTone, delayMix, p.delayPingPong);   // Wow, Tape on/off
    shimmer.setParams (p.reverbSize, p.reverbDamp, reverbMix);                            // Size, Shimmer, Mix

    pong.setParams (delaySec, p.delayFeedback, p.delayPingPong ? 0.12f : 0.7f, delayMix, true); // Rock Damp
    pong.setWidth (p.delayTone * 2.0f);                                                   // Width
    pong.setFeedbackBoost (activeWorld == World::Underground && alt ? kick * 0.15f : 0.0f);
    cavern.setParams (p.reverbSize, p.reverbDamp, reverbMix);                             // Depth, Drip, Mix
}

void Engine::runWorldFx (World w, bool alt, float* L, float* R, int n)
{
    switch (w)
    {
        case World::Classic:
            delay.process (L, R, n);
            reverb.process (L, R, n);
            break;
        case World::Space:
            if (alt)
            {
                // Deep Space: echo first (with the last block's shimmer fed back in), then phase, then shimmer.
                // The wash ducks while a note plays: keep the dry hit and follow its level.
                const float att = 1.0f - std::exp (-1.0f / (0.002f * (float) sampleRate));
                const float rel = 1.0f - std::exp (-1.0f / (0.15f * (float) sampleRate));
                float* dryL = duckBuf.getWritePointer (0);
                float* dryR = duckBuf.getWritePointer (1);
                float* env = duckBuf.getWritePointer (2);
                for (int i = 0; i < n; ++i)
                {
                    dryL[i] = L[i];
                    dryR[i] = R[i];
                    const float a = 0.5f * (std::abs (L[i]) + std::abs (R[i]));
                    duckEnv += (a > duckEnv ? att : rel) * (a - duckEnv);
                    env[i] = std::min (1.0f, duckEnv * 6.0f);
                }
                echo.process (L, R, n, deepFeedback.getReadPointer (0), deepFeedback.getReadPointer (1));
                phase.process (L, R, n);
                shimmer.process (L, R, n);
                for (int c = 0; c < 2; ++c)
                {
                    const float* wet = shimmer.lastWet (c);
                    float* fb = deepFeedback.getWritePointer (c);
                    for (int i = 0; i < n; ++i)
                        fb[i] = std::tanh (wet[i] * 0.22f) * (1.0f - 0.7f * env[i]);
                }
                for (int i = 0; i < n; ++i)
                {
                    const float d = 0.55f * env[i];
                    L[i] += d * (dryL[i] - L[i]);
                    R[i] += d * (dryR[i] - R[i]);
                }
            }
            else
            {
                phase.process (L, R, n);
                echo.process (L, R, n);
                shimmer.process (L, R, n);
            }
            break;
        case World::Underground:
            pong.process (L, R, n);
            cavern.process (L, R, n);
            break;
    }
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
    sequencer.setSwing (params.swing);
    sequencer.setGate (params.gate);
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

        updateFxParams (t.bpm);
        kick *= (float) std::exp (-(double) n / (0.25 * sampleRate));

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

        runWorldFx (activeWorld, params.worldAlt, L, R, n);

        // the previous world's tail, fading out over a few seconds
        if (tailSamples > 0 && tailWorld != activeWorld)
        {
            float* tl = tail.getWritePointer (0);
            float* tr = tail.getWritePointer (1);
            std::fill (tl, tl + n, 0.0f);
            std::fill (tr, tr + n, 0.0f);
            runWorldFx (tailWorld, tailAlt, tl, tr, n);
            for (int i = 0; i < n; ++i)
            {
                const float g = (float) std::max (0, tailSamples - i) / (float) tailLength;
                L[i] += tl[i] * g;
                R[i] += tr[i] * g;
            }
            tailSamples -= n;
            if (tailSamples <= 0)
            {
                // silence the outgoing world's effects so they start clean next time
                if (tailWorld == World::Classic) { delay.reset(); reverb.reset(); }
                else if (tailWorld == World::Space) { echo.reset(); shimmer.reset(); phase.reset(); deepFeedback.clear(); }
                else { pong.reset(); cavern.reset(); }
            }
        }

        // ---- soft ceiling: transparent below about -4.4 dBFS, rounds peaks off towards -0.4 dBFS
        for (int c = 0; c < 2; ++c)
        {
            float* x = stereo.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                x[i] = softCeiling (x[i]);
        }

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
