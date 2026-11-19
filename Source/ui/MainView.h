#pragma once

#include "GeneratorPanel.h"
#include "HeaderBar.h"
#include "ParamPanels.h"
#include "PianoRoll.h"
#include "RungsLookAndFeel.h"
#include "UiContext.h"
#include "WorldView.h"

namespace acid::ui
{

/** Root of the bundled UI, laid out at theme::baseWidth × baseHeight.
    The editor scales it, so all layout here is in fixed design units.
    Follows the active world: palette, effect names, World view scene.
    To redesign the front end, replace this class (or the whole ui/ folder);
    it only depends on UiContext. */
class MainView final : public juce::Component,
                       private juce::Timer
{
public:
    explicit MainView (UiContext ctx);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyWorld (World w, bool alt);
    void setWorldView (bool on);

    UiContext ctx;
    int shownWorld = -1;
    bool shownAlt = false;
    float flash = 0.0f;
    RungsLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    HeaderBar header;
    ParamSection voice, seq;
    PianoRoll roll;
    WorldView worldView;
    GeneratorPanel generator;
    ParamSection drive, delay, reverb;
};

} // namespace acid::ui
