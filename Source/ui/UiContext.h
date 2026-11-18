#pragma once

#include "core/LiveState.h"
#include "core/ParamSpec.h"
#include "seq/PatternController.h"

namespace juce { class AudioProcessorValueTreeState; }

namespace acid::ui
{

/** The complete contract between the engine and any user interface.

    - params:   host-automatable parameters; attach controls by acid::idOf (Param::X)
    - patterns: read / edit / generate / undo / export sequencer patterns
    - live:     lock-free status for meters & playhead (poll on a timer)

    A redesigned front end only needs this struct — nothing in core/, dsp/ or
    seq/ depends on the UI. */
struct UiContext
{
    juce::AudioProcessorValueTreeState& params;
    PatternController& patterns;
    LiveState& live;
};

} // namespace acid::ui
