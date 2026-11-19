#include "ParamPanels.h"

#include <algorithm>

namespace acid::ui
{

ParamControl::ParamControl (juce::AudioProcessorValueTreeState& state, Param p) : param (p)
{
    const auto& spec = specOf (p);
    setTitle (spec.name);
    caption = juce::String (spec.name).toUpperCase();

    switch (spec.kind)
    {
        case ParamSpec::Kind::Float:
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setPopupDisplayEnabled (true, true, nullptr);
            slider.setTooltip (spec.name);
            addAndMakeVisible (slider);
            sliderAttachment = std::make_unique<SliderAttachment> (state, spec.id, slider);
            slider.setDoubleClickReturnValue (true, spec.defaultValue);
            break;

        case ParamSpec::Kind::Choice:
            for (int i = 0; i < (int) spec.choices.size(); ++i)
                combo.addItem (spec.choices[(size_t) i], i + 1);
            addAndMakeVisible (combo);
            comboAttachment = std::make_unique<ComboAttachment> (state, spec.id, combo);
            break;

        case ParamSpec::Kind::Bool:
            toggle.setButtonText (spec.name);
            addAndMakeVisible (toggle);
            buttonAttachment = std::make_unique<ButtonAttachment> (state, spec.id, toggle);
            break;
    }
}

void ParamControl::paint (juce::Graphics& g)
{
    if (getKind() == ParamSpec::Kind::Bool || ! showCaption)
        return;
    g.setColour (theme::pal (*this).textDim);
    g.setFont (theme::labelFont (16.0f));
    auto area = getKind() == ParamSpec::Kind::Float ? slider.getBounds() : combo.getBounds();
    g.drawText (caption,
                juce::Rectangle<int> (0, area.getBottom() + 1, getWidth(), 14), juce::Justification::centred, false);
}

void ParamControl::resized()
{
    auto r = getLocalBounds();
    switch (getKind())
    {
        case ParamSpec::Kind::Float:
        {
            // square knob, caption directly beneath, block centred vertically
            const int side = juce::jmax (16, juce::jmin (r.getWidth(), r.getHeight() - 15, 72));
            slider.setBounds (r.withSizeKeepingCentre (side, side).withY (r.getY() + (r.getHeight() - side - 15) / 2));
            break;
        }
        case ParamSpec::Kind::Choice:
            if (showCaption)
                combo.setBounds (r.withSizeKeepingCentre (r.getWidth(), 24).translated (0, -7));
            else
                combo.setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
            break;
        case ParamSpec::Kind::Bool:
            toggle.setBounds (r.withSizeKeepingCentre (r.getWidth(), 24));
            break;
    }
}

// --------------------------------------------------------------------------

ParamSection::ParamSection (juce::AudioProcessorValueTreeState& state, juce::String t,
                            std::vector<Param> params, std::optional<Param> enableParam)
    : title (std::move (t))
{
    for (auto p : params)
    {
        controls.push_back (std::make_unique<ParamControl> (state, p));
        addAndMakeVisible (*controls.back());
    }
    if (enableParam)
    {
        enableToggle = std::make_unique<juce::ToggleButton> ("ON");
        addAndMakeVisible (*enableToggle);
        enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, idOf (*enableParam), *enableToggle);
    }
}

ParamControl* ParamSection::controlFor (Param p)
{
    for (auto& c : controls)
        if (c->getParam() == p)
            return c.get();
    return nullptr;
}

void ParamSection::paint (juce::Graphics& g)
{
    const auto& p = theme::pal (*this);
    theme::pixelPanel (g, getLocalBounds(), p.panel, p.panelHi, p.panelLo, p.edge);
    g.setColour (p.title);
    g.setFont (theme::titleFont (8.0f));
    g.drawText (title, getLocalBounds().reduced (12, 0).removeFromTop (26), juce::Justification::centredLeft, true);
}

void ParamSection::resized()
{
    auto r = getLocalBounds().reduced (6, 0);
    auto header = r.removeFromTop (24);
    if (enableToggle)
        enableToggle->setBounds (header.removeFromRight (58).reduced (0, 3).translated (-3, 2));
    r.removeFromBottom (6);

    std::vector<ParamControl*> knobs, others;
    for (auto& c : controls)
        (c->getKind() == ParamSpec::Kind::Float ? knobs : others).push_back (c.get());

    // tall sections put choices/toggles on their own row above the knobs
    const bool stacked = r.getHeight() >= 140 && ! others.empty() && ! knobs.empty();
    if (stacked)
    {
        auto row = r.removeFromTop (30);
        const int w = row.getWidth() / (int) others.size();
        for (auto* c : others)
        {
            c->setShowCaption (false);
            c->setBounds (row.removeFromLeft (w).reduced (3, 2));
        }
        others.clear();
    }

    const int fixed = (int) others.size() * 84;
    const int knobW = knobs.empty() ? 0 : juce::jmax (40, (r.getWidth() - fixed) / (int) knobs.size());
    for (auto& c : controls)
    {
        if (c->getKind() == ParamSpec::Kind::Float)
            c->setBounds (r.removeFromLeft (knobW).reduced (2, 0));
        else if (std::find (others.begin(), others.end(), c.get()) != others.end())
            c->setBounds (r.removeFromLeft (84).reduced (2, 0));
    }
}

} // namespace acid::ui
