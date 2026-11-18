#include "AcidLookAndFeel.h"

namespace acid::ui
{

AcidLookAndFeel::AcidLookAndFeel()
{
    using namespace theme;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, panelRaised);
    setColour (juce::TextButton::buttonOnColourId, acid);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::ComboBox::backgroundColourId, panelRaised);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, acid);
    setColour (juce::PopupMenu::highlightedTextColourId, background);
    setColour (juce::TooltipWindow::backgroundColourId, panelRaised);
    setColour (juce::TooltipWindow::textColourId, text);
}

void AcidLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace theme;
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = juce::jmax (2.5f, radius * 0.14f);

    juce::Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    g.setColour (outline);
    g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f,
                            juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (slider.isEnabled() ? acid : textDim);
    g.strokePath (valueArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius - track * 2.2f;
    g.setColour (panelRaised);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (outline);
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR, 3.0f, knobR * 0.55f, 1.5f);
    g.setColour (text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void AcidLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace theme;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? acid.withAlpha (0.18f) : panelRaised);
    g.fillRoundedRectangle (r, corner);
    g.setColour (on ? acid : (highlighted ? textDim : outline));
    g.drawRoundedRectangle (r, corner, 1.0f);

    auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.32f);
    g.setColour (on ? acid : outline);
    g.fillEllipse (led);

    g.setColour (on ? text : textDim);
    g.setFont (font (11.5f, true));
    g.drawText (b.getButtonText(), r.withTrimmedRight (4.0f), juce::Justification::centred);
}

void AcidLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                            bool highlighted, bool down)
{
    using namespace theme;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
    if (down) c = c.brighter (0.15f);
    else if (highlighted) c = c.brighter (0.07f);
    g.setColour (c);
    g.fillRoundedRectangle (r, corner);
    g.setColour (outline);
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void AcidLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace theme;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, corner);
    g.setColour (box.hasKeyboardFocus (true) ? acid : outline);
    g.drawRoundedRectangle (r, corner, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (textDim);
    g.fillPath (arrow);
}

} // namespace acid::ui
