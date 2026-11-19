#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <map>

using namespace acid;

AcidProcessor::AcidProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Params", createParameterLayout())
{
    for (const auto& s : paramSpecs())
    {
        rawParams[(size_t) static_cast<int> (s.param)] = apvts.getRawParameterValue (s.id);
        jassert (rawParams[(size_t) static_cast<int> (s.param)] != nullptr);
    }
    startTimerHz (30);
}

void AcidProcessor::timerCallback()
{
    // MIDI asked for a world: move the parameter so Live, automation and the UI follow
    const unsigned serial = live.midiWorldSerial.load();
    if (serial != lastMidiWorldSerial)
    {
        lastMidiWorldSerial = serial;
        if (auto* p = apvts.getParameter (idOf (Param::World)))
        {
            const float v = p->convertTo0to1 ((float) live.midiWorldRequest.load());
            if (std::abs (p->getValue() - v) > 1.0e-4f)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (v);
                p->endChangeGesture();
            }
        }
    }

    // the engine switched (on the beat/bar): retune what the UI shows
    patterns.setWorld (worldFromIndex (live.world.load()));
}

juce::AudioProcessorValueTreeState::ParameterLayout AcidProcessor::createParameterLayout()
{
    using Kind = ParamSpec::Kind;

    static const std::map<juce::String, juce::String> groupNames {
        { "voice", "Voice" }, { "seq", "Sequencer" }, { "drive", "Overdrive" }, { "delay", "Delay" }, { "reverb", "Reverb" },
        { "world", "World" }
    };

    std::map<juce::String, std::unique_ptr<juce::AudioProcessorParameterGroup>> groups;
    std::vector<juce::String> groupOrder;

    for (const auto& s : paramSpecs())
    {
        auto& g = groups[s.group];
        if (g == nullptr)
        {
            g = std::make_unique<juce::AudioProcessorParameterGroup> (s.group, groupNames.at (s.group), "|");
            groupOrder.push_back (s.group);
        }

        const juce::ParameterID pid { s.id, 1 };
        switch (s.kind)
        {
            case Kind::Float:
            {
                juce::NormalisableRange<float> range (s.minValue, s.maxValue);
                if (s.skewCentre > s.minValue && s.skewCentre < s.maxValue)
                    range.setSkewForCentre (s.skewCentre);
                g->addChild (std::make_unique<juce::AudioParameterFloat> (
                    pid, s.name, range, s.defaultValue, juce::AudioParameterFloatAttributes().withLabel (s.unit)));
                break;
            }
            case Kind::Choice:
            {
                juce::StringArray choices;
                for (auto* c : s.choices) choices.add (c);
                g->addChild (std::make_unique<juce::AudioParameterChoice> (pid, s.name, choices, (int) s.defaultValue));
                break;
            }
            case Kind::Bool:
                g->addChild (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.defaultValue >= 0.5f));
                break;
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& name : groupOrder)
        layout.add (std::move (groups[name]));
    return layout;
}

bool AcidProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AcidProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

void AcidProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    std::array<float, numParams> values {};
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = rawParams[i]->load (std::memory_order_relaxed);
    engine.setParams (EngineParams::fromRaw (values));

    std::optional<TransportInfo> transport;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            TransportInfo t;
            t.isPlaying = pos->getIsPlaying();
            t.ppqPosition = pos->getPpqPosition().orFallback (0.0);
            t.bpm = pos->getBpm().orFallback (120.0);
            transport = t;
        }
    }

    engine.process (buffer, midi, transport);
    midi.clear();
}

juce::AudioProcessorEditor* AcidProcessor::createEditor()
{
    return new AcidEditor (*this, getUiContext());
}

void AcidProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("Acid303");
    root.setProperty ("version", JucePlugin_VersionString, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (patterns.toValueTree(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void AcidProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("Acid303"))
        return;

    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
        apvts.replaceState (params);

    auto pats = root.getChildWithName ("Patterns");
    if (pats.isValid())
    {
        // PatternController is message-thread only
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            patterns.fromValueTree (pats);
        else
            juce::MessageManager::callAsync ([this, pats] { patterns.fromValueTree (pats); });
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AcidProcessor();
}
