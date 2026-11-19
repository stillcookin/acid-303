#include "PluginEditor.h"

using namespace acid::ui;

AcidEditor::AcidEditor (juce::AudioProcessor& p, UiContext ctx)
    : AudioProcessorEditor (p), view (ctx)
{
    addAndMakeVisible (view);

    const double aspect = (double) theme::baseWidth / (double) theme::baseHeight;
    setResizable (true, true);
    setResizeLimits ((int) (theme::baseWidth * 0.7), (int) (theme::baseHeight * 0.7),
                     theme::baseWidth * 2, theme::baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (aspect);
    setSize (theme::baseWidth, theme::baseHeight);
}

void AcidEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void AcidEditor::resized()
{
    const float scale = (float) getWidth() / (float) theme::baseWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setBounds (0, 0, theme::baseWidth, theme::baseHeight);
}
