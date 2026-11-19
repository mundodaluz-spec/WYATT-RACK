#include "Theme.h"
#include "BinaryData.h"
#include "../Engine/Catalog.h"

namespace wr::ui
{
juce::Colour familyColour (int t)
{
    switch ((BlockType) t)
    {
        case BlockType::Compressor: case BlockType::Gate: case BlockType::Limitador: case BlockType::DeEsser: return col::amber;
        case BlockType::EQ: case BlockType::Filtro:                                                          return juce::Colour (0xffe8dcc0);
        case BlockType::Reverb: case BlockType::Delay: case BlockType::Estereo:                              return col::blue;
        case BlockType::Chorus: case BlockType::Modulacao: case BlockType::Tremolo:                          return col::violet;
        case BlockType::Saturacao: case BlockType::LoFi:                                                     return col::red;
        default: return juce::Colour (0xff55585f);
    }
}

juce::Font bc (float h, int weight)
{
    static auto m = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize);
    static auto s = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    static auto b = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    return juce::Font (juce::FontOptions (weight >= 700 ? b : (weight >= 600 ? s : m)).withHeight (h));
}

const Assets& Assets::get()
{
    static Assets a = []
    {
        Assets x;
        auto load = [] (const void* d, int n) { return juce::ImageCache::getFromMemory (d, n); };
        x.metal = load (BinaryData::metal_jpg, BinaryData::metal_jpgSize);
        x.knob = load (BinaryData::knob_png, BinaryData::knob_pngSize);
        x.btnRed = load (BinaryData::btn_red_png, BinaryData::btn_red_pngSize);
        x.btnCream = load (BinaryData::btn_cream_png, BinaryData::btn_cream_pngSize);
        x.btnDark = load (BinaryData::btn_dark_png, BinaryData::btn_dark_pngSize);
        x.logo = load (BinaryData::logo_png, BinaryData::logo_pngSize);
        return x;
    }();
    return a;
}

void fillMetal (juce::Graphics& g, juce::Rectangle<float> r, float darken, float corner)
{
    const auto& a = Assets::get();
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path p; p.addRoundedRectangle (r, corner);
    g.reduceClipRegion (p);
    // a textura cobre a tela inteira na mesma posição: as chapas parecem cortadas da mesma placa
    g.setTiledImageFill (a.metal, 0, 0, 1.0f);
    g.fillRect (r);
    if (darken > 0) { g.setColour (juce::Colours::black.withAlpha (darken)); g.fillRect (r); }
}

void drawScrew (juce::Graphics& g, float x, float y)
{
    const float d = 11;
    juce::ColourGradient cg (juce::Colour (0xffd8d8d8), x + d * 0.35f, y + d * 0.3f, juce::Colour (0xff232325), x + d, y + d, true);
    cg.addColour (0.45, juce::Colour (0xff6d6d70));
    g.setGradientFill (cg); g.fillEllipse (x, y, d, d);
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (x, y, d, d, 0.8f);
    juce::Path slot; slot.addRectangle (-d * 0.36f, -0.7f, d * 0.72f, 1.4f);
    g.setColour (juce::Colour (0xff1a1a1b));
    g.fillPath (slot, juce::AffineTransform::rotation (0.6f).translated (x + d / 2, y + d / 2));
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, bool screws)
{
    // brilho vermelho em volta
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (col::red.withAlpha (0.018f * (float) (7 - i)));
        g.drawRoundedRectangle (r.expanded ((float) i * 1.6f), 10 + (float) i, 2.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r.translated (0, 4).expanded (1), 10);
    fillMetal (g, r, 0.18f, 10);
    juce::ColourGradient sh (juce::Colours::white.withAlpha (0.035f), 0, r.getY(), juce::Colours::black.withAlpha (0.25f), 0, r.getBottom(), false);
    g.setGradientFill (sh); g.fillRoundedRectangle (r, 10);
    g.setColour (juce::Colour (0xff2b2b2f)); g.drawRoundedRectangle (r, 10, 1.0f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r.expanded (1), 11, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 10, r.getRight() - 10);
    if (screws)
        for (auto p : { juce::Point<float> (r.getX() + 8, r.getY() + 8), { r.getRight() - 19, r.getY() + 8 },
                        { r.getX() + 8, r.getBottom() - 19 }, { r.getRight() - 19, r.getBottom() - 19 } })
            drawScrew (g, p.x, p.y);
}

void drawTitle (juce::Graphics& g, const juce::String& t, float x, float y, float size)
{
    g.setFont (bc (size, 700));
    const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), t);
    g.setColour (col::cream);
    g.drawText (t, juce::Rectangle<float> (x, y, w + 4, size * 1.2f), juce::Justification::centredLeft);
    juce::ColourGradient lg (col::red.withAlpha (0.0f), x - 4, 0, col::red.withAlpha (0.0f), x + w * 1.2f, 0, false);
    lg.addColour (0.12, col::red); lg.addColour (0.7, col::red);
    g.setGradientFill (lg);
    g.fillRoundedRectangle (x - 4, y + size * 1.2f + 1, w * 1.2f + 4, 3, 1.5f);
}

void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (r.translated (0, 1), 6, 1);
    g.setColour (col::display); g.fillRoundedRectangle (r, 6);
    for (int i = 0; i < 5; ++i)
    {   // sombra interna
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawRoundedRectangle (r.reduced ((float) i * 0.8f), 6, 1.2f);
    }
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 6, 1);
}

void drawStripBase (juce::Graphics& g, juce::Rectangle<float> r, bool active)
{
    g.setColour (juce::Colour (active ? 0xff141416 : 0xff111113)); g.fillRoundedRectangle (r, 7);
    juce::ColourGradient sh (juce::Colours::white.withAlpha (0.05f), 0, r.getY(), juce::Colours::black.withAlpha (0.35f), 0, r.getBottom(), false);
    g.setGradientFill (sh); g.fillRoundedRectangle (r, 7);
    g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 6, r.getRight() - 6);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 7, 1);
}

static void drawSliced (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> rf)
{
    // 3 fatias: as pontas mantêm a proporção, o meio estica. Posições inteiras e 1 px de sobreposição: sem fresta.
    const auto r = rf.toNearestInt();
    const int iw = img.getWidth(), ih = img.getHeight();
    const float scale = (float) r.getHeight() / (float) ih;
    const int cap = juce::jmin (ih / 2, iw / 3);
    const int capW = juce::jmin (juce::roundToInt ((float) cap * scale), r.getWidth() / 2);
    g.drawImage (img, r.getX(), r.getY(), capW + 1, r.getHeight(), 0, 0, cap, ih);
    g.drawImage (img, r.getRight() - capW - 1, r.getY(), capW + 1, r.getHeight(), iw - cap, 0, cap, ih);
    g.drawImage (img, r.getX() + capW, r.getY(), r.getWidth() - 2 * capW, r.getHeight(), cap, 0, iw - 2 * cap, ih);
    g.drawImage (img, r.getRight() - capW - 1, r.getY(), capW + 1, r.getHeight(), iw - cap, 0, cap, ih);
}

void drawKey (juce::Graphics& g, juce::Rectangle<float> r, KeyKind k, const juce::String& text, float fs, bool enabled, bool down)
{
    const auto& a = Assets::get();
    const auto& img = k == KeyKind::Red ? a.btnRed : (k == KeyKind::Cream ? a.btnCream : a.btnDark);
    juce::Graphics::ScopedSaveState ss (g);
    if (! enabled) g.setOpacity (0.45f);
    drawSliced (g, img, down ? r.reduced (0.5f).translated (0, 0.5f) : r);
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.15f)); g.fillRoundedRectangle (r.reduced (3), 4); }
    g.setFont (bc (fs, 600));
    g.setColour (k == KeyKind::Cream ? juce::Colour (0xff151515) : (k == KeyKind::Red ? juce::Colours::white : col::cream));
    g.drawFittedText (text, r.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
}

void drawKnob (juce::Graphics& g, juce::Rectangle<float> c, float pos, bool valueArc)
{
    const auto& a = Assets::get();
    const float d = juce::jmin (c.getWidth(), c.getHeight());
    c = c.withSizeKeepingCentre (d, d);
    const float a0 = -2.356194f, a1 = 2.356194f, ang = a0 + juce::jlimit (0.0f, 1.0f, pos) * (a1 - a0);
    const float r = d / 2 + d * 0.11f, sw = juce::jmax (2.0f, d * 0.05f);
    const auto ctr = c.getCentre();
    juce::Path track; track.addCentredArc (ctr.x, ctr.y, r, r, 0, a0, a1, true);
    g.setColour (juce::Colour (0xffe9dfcd).withAlpha (0.85f));
    g.strokePath (track, juce::PathStrokeType (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (valueArc && pos > 0.01f)
    {
        juce::Path v; v.addCentredArc (ctr.x, ctr.y, r, r, 0, a0, ang, true);
        g.setColour (col::red.withAlpha (0.35f));
        g.strokePath (v, juce::PathStrokeType (sw * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::red);
        g.strokePath (v, juce::PathStrokeType (sw * 1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.drawImage (a.knob, c, juce::RectanglePlacement::stretchToFit);
    const float p0 = d * 0.10f, p1 = d * 0.36f;
    juce::Line<float> ln (ctr.x + p0 * std::sin (ang), ctr.y - p0 * std::cos (ang), ctr.x + p1 * std::sin (ang), ctr.y - p1 * std::cos (ang));
    g.setColour (col::red);
    g.drawLine (ln, juce::jmax (2.0f, d * 0.045f));
}

void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool on)
{
    if (on)
    {
        g.setColour (c.withAlpha (0.25f)); g.fillEllipse (r.expanded (r.getWidth() * 0.35f));
        juce::ColourGradient cg (c.brighter (0.9f), r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.35f, c.darker (0.6f), r.getRight(), r.getBottom(), true);
        cg.addColour (0.55, c);
        g.setGradientFill (cg);
    }
    else
    {
        juce::ColourGradient cg (juce::Colour (0xff555555), r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.35f, juce::Colour (0xff0c0c0c), r.getRight(), r.getBottom(), true);
        g.setGradientFill (cg);
    }
    g.fillEllipse (r);
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (r, 1);
}

void drawSegments (juce::Graphics& g, float x, float y, int n, float w, float h, float level, int redFrom)
{
    const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, level) * (float) n);
    for (int i = 0; i < n; ++i)
    {
        const auto c = i >= redFrom ? col::red : col::cream;
        const bool on = i < lit;
        auto r = juce::Rectangle<float> (x + (float) i * (w + 2), y, w, h);
        if (on) { g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (r.expanded (1.5f), 2.5f); }
        g.setColour (on ? c : c.withAlpha (0.13f));
        g.fillRoundedRectangle (r, 1.5f);
    }
}

// ---------------------------------------------------------------- KeyButton
KeyButton::KeyButton (const juce::String& t, KeyKind off, KeyKind on, float fs) : juce::Button (t), offKind (off), onKind (on), fontSize (fs) {}

void KeyButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    drawKey (g, getLocalBounds().toFloat(), getToggleState() ? onKind : offKind, getButtonText(), fontSize, isEnabled(), down);
    if (over && isEnabled()) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3), 4); }
}

// ---------------------------------------------------------------- Knob
Knob::Knob()
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (200);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

juce::Rectangle<float> Knob::knobArea() const
{
    auto r = getLocalBounds().toFloat();
    if (layout == Layout::Inline) return { r.getX() + 1, r.getCentreY() - knobSize / 2, knobSize, knobSize };
    if (layout == Layout::Bare) return r.withSizeKeepingCentre (knobSize, knobSize);
    return { r.getCentreX() - knobSize / 2, r.getY() + nameSize + 12, knobSize, knobSize };
}

void Knob::resized()
{
    // a área de clique é o próprio knob (com margem para o arco)
    slider.setBounds (knobArea().expanded (knobSize * 0.14f).toNearestInt());
}

void Knob::paint (juce::Graphics& g)
{
    const auto txt = format ? format (slider.getValue()) : juce::String (slider.getValue(), 1);
    auto r = getLocalBounds().toFloat();
    if (layout == Layout::Inline)
    {
        auto t = r.withTrimmedLeft (knobSize + 6);
        g.setFont (bc (nameSize, 600)); g.setColour (col::label);
        g.drawText (name, t.withHeight (r.getHeight() / 2).withTrimmedBottom (1), juce::Justification::bottomLeft);
        g.setFont (bc (valueSize, 500)); g.setColour (col::cream);
        g.drawFittedText (txt, t.withTrimmedTop (r.getHeight() / 2).toNearestInt(), juce::Justification::topLeft, 1, 0.8f);
    }
    else if (layout == Layout::Stacked)
    {
        g.setFont (bc (nameSize, 600)); g.setColour (col::cream);
        g.drawFittedText (name, r.removeFromTop (nameSize + 4).toNearestInt(), juce::Justification::centred, 1, 0.8f);
        g.setFont (bc (valueSize, 500)); g.setColour (col::cream);
        g.drawFittedText (txt, getLocalBounds().removeFromBottom ((int) valueSize + 6), juce::Justification::centred, 1, 0.8f);
    }
}

// ---------------------------------------------------------------- LookAndFeel
KnobLook::KnobLook()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141416));
    setColour (juce::PopupMenu::textColourId, col::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::red);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, col::red);
    setColour (juce::ComboBox::textColourId, col::cream);
    setColour (juce::TextEditor::textColourId, col::cream);
    setColour (juce::TextEditor::highlightColourId, col::red.withAlpha (0.4f));
    setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    setColour (juce::CaretComponent::caretColourId, col::red);
    setColour (juce::Label::textColourId, col::cream);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff141416));
    setColour (juce::AlertWindow::textColourId, col::cream);
    setColour (juce::AlertWindow::outlineColourId, col::grid2);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1d1e22));
    setColour (juce::TextButton::textColourOffId, col::cream);
    setColour (juce::ToggleButton::textColourId, col::cream);
    setColour (juce::ToggleButton::tickColourId, col::red);
    setColour (juce::ToggleButton::tickDisabledColourId, col::dim);
    setColour (juce::HyperlinkButton::textColourId, col::online);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff141416));
    setColour (juce::TooltipWindow::textColourId, col::cream);
    setColour (juce::TooltipWindow::outlineColourId, col::grid2);
}

void KnobLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float d = juce::jmin (r.getWidth(), r.getHeight()) / 1.28f;   // a área inclui a margem do arco
    drawKnob (g, r.withSizeKeepingCentre (d, d), pos, s.isEnabled());
}

void KnobLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    drawDisplay (g, juce::Rectangle<float> (0, 0, (float) w, (float) h));
    juce::Path a; const float ax = (float) w - 14, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3);
    g.setColour (col::dim); g.fillPath (a);
}

void KnobLook::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    l.setFont (getComboBoxFont (box));
}

void KnobLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colour (0xff141416));
    g.setColour (col::grid2); g.drawRect (0, 0, w, h);
}
} // namespace wr::ui
