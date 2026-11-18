#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace acid::dsp
{

/** Thin wrapper over juce::Reverb (Freeverb) with a single mix control.
    Kept behind this interface so it can be swapped for an FDN later. */
class StereoReverb
{
public:
    void prepare (double sampleRate)
    {
        reverb.setSampleRate (sampleRate);
        reverb.reset();
    }

    void reset() { reverb.reset(); }

    void setParams (float size01, float damp01, float mix01)
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.35f + 0.6f * size01;
        p.damping = damp01;
        p.wetLevel = 0.45f * mix01;
        p.dryLevel = 1.0f - 0.5f * mix01;
        p.width = 1.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p); // juce::Reverb smooths these internally
    }

    void process (float* left, float* right, int numSamples)
    {
        reverb.processStereo (left, right, numSamples);
    }

private:
    juce::Reverb reverb;
};

} // namespace acid::dsp
