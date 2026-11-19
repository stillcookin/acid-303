#pragma once

#include "EngineParams.h"
#include "LiveState.h"
#include "../dsp/Delay.h"
#include "../dsp/Cavern.h"
#include "../dsp/Grind.h"
#include "../dsp/Overdrive.h"
#include "../dsp/PhaseChorus.h"
#include "../dsp/Shimmer.h"
#include "../dsp/SpaceEcho.h"
#include "../dsp/Reverb.h"
#include "../dsp/Voice303.h"
#include "../seq/PatternController.h"
#include "../seq/Sequencer.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>
#include <optional>

namespace acid
{

/** Host-independent instrument engine. The plugin's AudioProcessor is a thin
    adapter around this; tests and the offline render tool drive it directly.

    Signal flow per world (2× oversampled up to the stereo stage):
      Classic      Voice → Overdrive → Delay → Reverb      (HOT: Overdrive before the filter)
      Space        Voice → Phase/Chorus → Space Echo → Shimmer
                   (DEEP SPACE: Echo → Phase/Chorus → Shimmer, shimmer fed back into the echo)
      Underground  Voice → Grind → Ping-Pong → Cavern      (MOLTEN CORE: Grind before the filter,
                   accents push the grind and the ping-pong feedback)
    After a world switch the previous world's echoes and reverb ring out and fade. */
class Engine
{
public:
    Engine (PatternMailbox& mailbox, LiveState& liveState);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setParams (const EngineParams& p) { params = p; }

    /** hostTransport: std::nullopt when the host provides no position info. */
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi,
                  std::optional<TransportInfo> hostTransport);

    int getLatencySamples() const;

private:
    struct VoiceEvent
    {
        int offset;
        int type;   // 0 off, 1 on, 2 all-off
        int note;
        bool accent;
        bool slide;
    };

    void collectMidi (const juce::MidiBuffer& midi, int start, int len);
    void scanWorldMidi (const juce::MidiBuffer& midi, int start, int len);
    void requestWorld (World w, bool fromMidi);
    void applyWorld (World w);
    dsp::VoiceParams voiceParams() const;
    const Pattern& currentPattern() const { return patterns[(size_t) indexOf (activeWorld)]; }
    void addEvent (const VoiceEvent& e);
    void sortEvents();
    void applyEvent (const VoiceEvent& e);
    void renderVoice (float* osData, int fromOs, int toOs);
    void updateFxParams (double bpm);
    void runWorldFx (World w, bool alt, float* L, float* R, int n);

    PatternMailbox& mailbox;
    LiveState& live;

    EngineParams params = EngineParams::defaults();
    WorldPatterns patterns {};

    // worlds: the parameter (or incoming MIDI) requests, the engine applies on the beat/bar
    World activeWorld = World::Classic, lastParamWorld = World::Classic;
    std::optional<World> pendingWorld;
    bool worldInitialised = false;
    Sequencer sequencer;
    dsp::Voice303 voice;
    // Classic
    dsp::Overdrive drive;
    dsp::StereoDelay delay;
    dsp::StereoReverb reverb;
    // Space
    dsp::PhaseChorus phase;
    dsp::SpaceEcho echo;
    dsp::Shimmer shimmer;
    // Underground
    dsp::Grind grind;
    dsp::StereoDelay pong;
    dsp::Cavern cavern;

    int postShaper = 1;          ///< in the oversampled path after the VCA: 0 none, 1 overdrive, 2 grind
    float kick = 0.0f;           ///< accent energy (Molten Core)
    float worldGain = 1.0f;      ///< loudness match for the current world and chain (RMS, not peaks)
    float duckEnv = 0.0f;        ///< Deep Space: follows the dry signal so the wash ducks under notes
    juce::AudioBuffer<float> tail, deepFeedback, duckBuf;
    World tailWorld = World::Classic;
    bool tailAlt = false;
    int tailSamples = 0, tailLength = 1;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> mono, stereo;
    int maxBlock = 512;
    PlayMode lastMode = PlayMode::Sequencer;

    double sampleRate = 44100.0;
    static constexpr int osFactor = 2;

    // events of the current block
    static constexpr int maxEvents = 512;
    std::array<VoiceEvent, maxEvents> events {};
    int numEvents = 0;
    std::array<SeqEvent, maxEvents> seqEvents {};

    // MIDI-mode note stack
    struct Held { int note; bool accent; };
    std::array<Held, 16> held {};
    int numHeld = 0;
    int transpose = 0;

    // internal clock for preview when the host is stopped
    double previewPpq = 0.0;
    bool previewWasOn = false;
    double lastBpm = 120.0;

    juce::SmoothedValue<float> volume;
    float peak = 0.0f;
};

} // namespace acid
