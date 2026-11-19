#pragma once

#include "Theme.h"
#include "UiContext.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <optional>
#include <vector>

namespace acid::ui
{

/** A parameter control created from its ParamSpec: knob (Float), combo (Choice)
    or toggle (Bool), with a caption. Layout-agnostic: the parent sets bounds. */
class ParamControl final : public juce::Component
{
public:
    ParamControl (juce::AudioProcessorValueTreeState& state, Param p);

    void resized() override;
    void paint (juce::Graphics&) override;

    void setShowCaption (bool show) { showCaption = show; resized(); repaint(); }
    /** Replaces the caption from ParamSpec (e.g. a knob whose role depends on the world). */
    void setCaption (const juce::String& c) { if (c != caption) { caption = c; repaint(); } }
    void setTooltip (const juce::String& t) { slider.setTooltip (t); combo.setTooltip (t); toggle.setTooltip (t); }
    Param getParam() const { return param; }
    ParamSpec::Kind getKind() const { return specOf (param).kind; }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    Param param;
    bool showCaption = true;
    juce::String caption;
    juce::Slider slider;
    juce::ComboBox combo;
    juce::ToggleButton toggle;
    std::unique_ptr<SliderAttachment> sliderAttachment;
    std::unique_ptr<ComboAttachment> comboAttachment;
    std::unique_ptr<ButtonAttachment> buttonAttachment;
};

/** Titled group of parameter controls laid out in a row, with an optional
    on/off parameter rendered as a toggle in the title bar. */
class ParamSection final : public juce::Component
{
public:
    ParamSection (juce::AudioProcessorValueTreeState& state, juce::String title,
                  std::vector<Param> params, std::optional<Param> enableParam = std::nullopt);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The control for `p`, or nullptr if this section has none. */
    ParamControl* controlFor (Param p);

private:
    juce::String title;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::unique_ptr<juce::ToggleButton> enableToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
};

} // namespace acid::ui
