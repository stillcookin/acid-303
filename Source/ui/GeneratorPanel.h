#pragma once

#include "Theme.h"
#include "UiContext.h"

namespace acid::ui
{

/** Controls for the pseudo-random pattern generator. Generator settings are
    not host parameters; they live in PatternController (saved with the set). */
class GeneratorPanel final : public juce::Component,
                             private PatternController::Listener
{
public:
    explicit GeneratorPanel (UiContext ctx);
    ~GeneratorPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::String caption;
    };

    void generatorSettingsChanged() override { refresh(); repaint(); }
    void patternChanged() override { refreshButtons(); }
    void refresh();
    void refreshButtons();
    void pushSettings();
    juce::TextButton& addButton (juce::TextButton& b, const juce::String& text, const juce::String& tip,
                                 std::function<void()> fn);

    UiContext ctx;
    int scaleListWorld = -1; ///< world whose scales are currently listed in scaleBox
    juce::ComboBox scaleBox, rootBox, rangeBox;
    std::array<Knob, 5> knobs; // density, accent, slide, octave, mutate
    juce::TextButton generateButton, mutateButton, accentsButton, slidesButton, octavesButton,
                     leftButton, rightButton, reverseButton, undoButton, redoButton, clearButton;
    juce::Label seedLabel;
    bool updating = false;
};

} // namespace acid::ui
