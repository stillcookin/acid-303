#pragma once

#include "EngineParams.h"
#include "LiveState.h"
#include "../dsp/Delay.h"
#include "../dsp/Overdrive.h"
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

    Signal flow: [sequencer / MIDI] → Voice303 → Overdrive (2× oversampled)
                 → StereoDelay → StereoReverb → Volume */
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
    void addEvent (const VoiceEvent& e);
    void sortEvents();
    void applyEvent (const VoiceEvent& e);
    void renderVoice (float* osData, int fromOs, int toOs);

    PatternMailbox& mailbox;
    LiveState& live;

    EngineParams params = EngineParams::defaults();
    Pattern pattern;
    Sequencer sequencer;
    dsp::Voice303 voice;
    dsp::Overdrive drive;
    dsp::StereoDelay delay;
    dsp::StereoReverb reverb;
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
