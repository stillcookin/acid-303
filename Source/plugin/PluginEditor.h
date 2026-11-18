#pragma once

#include "ui/MainView.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Hosts the UI root and scales it to the window size (fixed aspect ratio).
    Knows nothing about the UI's internals — swap MainView for another
    component built from UiContext to redesign the plugin. */
class AcidEditor final : public juce::AudioProcessorEditor
{
public:
    AcidEditor (juce::AudioProcessor& p, acid::ui::UiContext ctx);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    acid::ui::MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidEditor)
};
