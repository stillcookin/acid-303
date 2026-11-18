#pragma once

#include "AcidLookAndFeel.h"
#include "GeneratorPanel.h"
#include "HeaderBar.h"
#include "ParamPanels.h"
#include "PianoRoll.h"
#include "UiContext.h"

namespace acid::ui
{

/** Root of the bundled UI, laid out at theme::baseWidth × baseHeight.
    The editor scales it, so all layout here is in fixed design units.
    To redesign the front end, replace this class (or the whole ui/ folder);
    it only depends on UiContext. */
class MainView final : public juce::Component
{
public:
    explicit MainView (UiContext ctx);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AcidLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    HeaderBar header;
    ParamSection voice, seq;
    PianoRoll roll;
    GeneratorPanel generator;
    ParamSection drive, delay, reverb;
};

} // namespace acid::ui
