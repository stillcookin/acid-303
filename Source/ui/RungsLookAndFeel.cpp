#include "RungsLookAndFeel.h"

namespace acid::ui
{

RungsLookAndFeel::RungsLookAndFeel()
{
    applyColours();
}

void RungsLookAndFeel::setWorld (World w)
{
    world = w;
    palette = &theme::palette (w);
    applyColours();
}

void RungsLookAndFeel::applyColours()
{
    const auto& p = *palette;
    setColour (juce::ResizableWindow::backgroundColourId, p.background);
    setColour (juce::Label::textColourId, p.text);
    setColour (juce::Slider::textBoxTextColourId, p.text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, p.raised);
    setColour (juce::TextButton::buttonOnColourId, p.acid);
    setColour (juce::TextButton::textColourOffId, p.text);
    setColour (juce::TextButton::textColourOnId, p.onAcid);
    setColour (juce::ComboBox::backgroundColourId, p.raised);
    setColour (juce::ComboBox::textColourId, p.text);
    setColour (juce::ComboBox::outlineColourId, p.edge);
    setColour (juce::ComboBox::arrowColourId, p.text);
    setColour (juce::PopupMenu::backgroundColourId, p.raised);
    setColour (juce::PopupMenu::textColourId, p.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.acid);
    setColour (juce::PopupMenu::highlightedTextColourId, p.onAcid);
    setColour (juce::TooltipWindow::backgroundColourId, p.plate);
    setColour (juce::TooltipWindow::textColourId, p.plateText);
    setColour (juce::TooltipWindow::outlineColourId, p.edge);
    setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, p.rollBg);
    setColour (juce::TextEditor::textColourId, p.acid);
    setColour (juce::TextEditor::highlightColourId, p.acid.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, p.acid);
}

void RungsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float, float, juce::Slider& slider)
{
    theme::pixelKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, *palette, slider.isEnabled());
}

void RungsLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto& p = *palette;
    auto r = b.getLocalBounds();
    const bool on = b.getToggleState();
    theme::pixelPanel (g, r, highlighted ? p.raised.brighter (0.06f) : p.raised, p.raisedHi, p.raisedLo, p.edge);

    auto inner = r.reduced (theme::bevel * 2);
    auto led = inner.removeFromLeft (inner.getHeight()).withSizeKeepingCentre (6, 6);
    g.setColour (on ? p.acid : p.laneEdge);
    g.fillRect (led);

    g.setColour (on ? p.text : p.textDim);
    g.setFont (theme::labelFont (17.0f));
    g.drawText (b.getButtonText(), inner.withTrimmedRight (2), juce::Justification::centred, false);
}

void RungsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                             bool highlighted, bool down)
{
    const auto& p = *palette;
    const bool on = b.getToggleState();
    auto fill = on ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
    if (highlighted && ! down) fill = fill.brighter (0.06f);
    const auto hi = on ? p.acidHi : p.raisedHi;
    const auto lo = on ? p.acidLo : p.raisedLo;
    // pressed: swap the bevel
    theme::pixelPanel (g, b.getLocalBounds(), fill, down ? lo : hi, down ? hi : lo, p.edge);
}

void RungsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool on = b.getToggleState();
    auto col = b.findColour (on ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
    if (! b.isEnabled()) col = palette->textDim.withAlpha (0.6f);
    g.setColour (col);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 1 : 0), juce::Justification::centred, false);
}

void RungsLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box)
{
    const auto& p = *palette;
    theme::pixelPanel (g, { 0, 0, w, h }, box.findColour (juce::ComboBox::backgroundColourId),
                       down ? p.raisedLo : p.raisedHi, down ? p.raisedHi : p.raisedLo, p.edge);
    // pixel chevron
    g.setColour (p.text);
    const int cx = w - 13, cy = h / 2 - 2;
    g.fillRect (cx, cy, 7, 2);
    g.fillRect (cx + 2, cy + 2, 3, 2);
}

void RungsLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void RungsLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    const auto& p = *palette;
    theme::pixelPanel (g, { 0, 0, w, h }, p.raised, p.raisedHi, p.raisedLo, p.edge);
}

} // namespace acid::ui
