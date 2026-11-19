#pragma once

#include "core/Engine.h"
#include "ui/UiContext.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Thin host adapter: owns the parameters (APVTS) and the GUI-free engine
    objects, converts host transport/parameters for the Engine, and persists
    state. Contains no DSP or sequencing logic of its own. */
class AcidProcessor final : public juce::AudioProcessor,
                            private juce::Timer
{
public:
    AcidProcessor();
    ~AcidProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Everything a UI needs — hand this to any editor implementation. */
    acid::ui::UiContext getUiContext() { return { apvts, patterns, live }; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    /** Message thread: mirrors the engine's active world into the
        PatternController, and MIDI world requests into the World parameter. */
    void timerCallback() override;

    acid::PatternMailbox mailbox;
    acid::LiveState live;
    acid::PatternController patterns { mailbox };
    acid::Engine engine { mailbox, live };

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, acid::numParams> rawParams {};
    unsigned lastMidiWorldSerial = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidProcessor)
};
