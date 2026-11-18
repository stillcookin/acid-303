#pragma once

#include <atomic>

namespace acid
{

/** Lock-free values shared between the audio thread and any UI.
    The engine writes the "status" fields; a UI polls them on a timer.
    `previewRequested` is the one value a UI writes (internal-clock preview
    when the host transport is stopped). */
struct LiveState
{
    // engine → UI
    std::atomic<int> playingStep { -1 };       ///< step index, -1 when not running
    std::atomic<bool> running { false };       ///< sequencer is advancing (host or preview)
    std::atomic<bool> hostPlaying { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> transpose { 0 };          ///< semitones from Seq+Transpose mode
    std::atomic<float> outputPeak { 0.0f };    ///< decaying peak meter, linear
    std::atomic<int> voiceNote { -1 };         ///< current MIDI note while the gate is on

    // UI → engine
    std::atomic<bool> previewRequested { false };
};

} // namespace acid
