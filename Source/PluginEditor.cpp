#include "PluginEditor.h"
#include "BinaryData.h"
#include <map>

namespace wr
{
// ==================================================================== paleta e fontes
juce::Colour ui::familyColour (int t)
{
    switch ((BlockType) t)
    {
        case BlockType::Compressor: case BlockType::Gate: case BlockType::Limitador: case BlockType::DeEsser: return juce::Colour (0xffffb238); // dinâmica
        case BlockType::EQ: case BlockType::Filtro:                                                          return juce::Colour (0xffe8dcc0); // timbre
        case BlockType::Reverb: case BlockType::Delay: case BlockType::Estereo:                              return juce::Colour (0xff6fb6ff); // espaço
        case BlockType::Chorus: case BlockType::Modulacao: case BlockType::Tremolo:                          return juce::Colour (0xffb98cff); // movimento
        case BlockType::Saturacao: case BlockType::LoFi:                                                     return juce::Colour (0xffff5a4a); // caráter
        default: return juce::Colour (0xff55585f);
    }
}

juce::Font ui::font (float h, bool bold, bool black)
{
    static auto reg = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    static auto bld = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
    static auto blk = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBlack_ttf, BinaryData::InterBlack_ttfSize);
    return juce::Font (juce::FontOptions (black ? blk : (bold ? bld : reg)).withHeight (h));
}

static juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

// ==================================================================== LookAndFeel
RackLookAndFeel::RackLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, ui::metal);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::red.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, ui::amber);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::arrowColourId, ui::dim);
    setColour (juce::TextEditor::textColourId, ui::amber);
    setColour (juce::TextEditor::highlightColourId, ui::amber.withAlpha (0.3f));
    setColour (juce::TextEditor::highlightedTextColourId, ui::amber);
    setColour (juce::CaretComponent::caretColourId, ui::amber);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::TextButton::textColourOffId, ui::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::AlertWindow::backgroundColourId, ui::metal);
    setColour (juce::AlertWindow::textColourId, ui::text);
    setColour (juce::AlertWindow::outlineColourId, ui::line);
    setColour (juce::ToggleButton::textColourId, ui::text);
    setColour (juce::ToggleButton::tickColourId, ui::amber);
    setColour (juce::ToggleButton::tickDisabledColourId, ui::dim);
    setColour (juce::HyperlinkButton::textColourId, ui::online);
}

void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const float rad = d * 0.5f;
    const float ang = a0 + pos * (a1 - a0);

    // trilha
    juce::Path track; track.addCentredArc (c.x, c.y, rad - 1.5f, rad - 1.5f, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff0d0e10)); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (c.x, c.y, rad - 1.5f, rad - 1.5f, 0, a0, ang, true);
    g.setColour (col.withAlpha (s.isEnabled() ? 1.0f : 0.35f));
    g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // corpo do knob
    const float kr = rad - 5.0f;
    juce::ColourGradient grad (juce::Colour (0xff3a3d45), c.x, c.y - kr, juce::Colour (0xff17181c), c.x, c.y + kr, false);
    g.setGradientFill (grad); g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colour (0xff050506)); g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
    // ponteiro
    juce::Path p; p.addRoundedRectangle (-1.2f, -kr + 1.5f, 2.4f, kr * 0.55f, 1.2f);
    g.setColour (ui::text); g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void RackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (b.getProperties()["lamp"])
    {   // lâmpada liga/desliga
        const auto lc = b.findColour (juce::TextButton::buttonOnColourId);
        auto c = r.withSizeKeepingCentre (16, 16);
        if (on) { g.setColour (lc.withAlpha (0.25f)); g.fillEllipse (c.expanded (4)); }
        juce::ColourGradient lg (on ? lc.brighter (0.5f) : juce::Colour (0xff2a2b30), c.getCentreX(), c.getY(), on ? lc.darker (0.3f) : juce::Colour (0xff111215), c.getCentreX(), c.getBottom(), false);
        g.setGradientFill (lg); g.fillEllipse (c);
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (c, 1.2f);
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.15f)); g.drawEllipse (c.expanded (1.5f), 1); }
        return;
    }
    auto fill = base;
    if (on) fill = b.findColour (juce::TextButton::buttonOnColourId);
    if (down) fill = fill.brighter (0.15f); else if (over) fill = fill.brighter (0.07f);
    g.setColour (fill); g.fillRoundedRectangle (r, 4);
    g.setColour (on ? fill.brighter (0.3f) : ui::line); g.drawRoundedRectangle (r, 4, 1);
}

void RackLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

void RackLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff101114)); g.fillRoundedRectangle (r, 4);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId)); g.drawRoundedRectangle (r, 4, 1);
    juce::Path a; const float ax = (float) w - 14, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (ui::dim); g.fillPath (a);
}

void RackLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    l.setFont (getComboBoxFont (box));
}

void RackLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (ui::metal);
    g.setColour (ui::line); g.drawRect (0, 0, w, h);
}

void RackLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& te)
{
    g.setColour (te.findColour (juce::TextEditor::backgroundColourId)); g.fillRect (0, 0, w, h);
}

// ==================================================================== Knob
Knob::Knob()
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.onValueChange = [this] { repaint(); };
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
}

void Knob::attach (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (inlineLayout)
    {   // knob à esquerda, nome e valor ao lado
        const int d = juce::jmin (r.getHeight() - 4, 40, r.getWidth() / 2);
        slider.setBounds (r.removeFromLeft (d + 2).withSizeKeepingCentre (d, d));
        r.removeFromLeft (4);
        textArea = r.withSizeKeepingCentre (r.getWidth(), 30);
    }
    else
    {
        r.removeFromTop (12); r.removeFromBottom (13);
        slider.setBounds (r.withSizeKeepingCentre (r.getHeight() + 6, r.getHeight()));
        textArea = {};
    }
    slider.setColour (juce::Slider::rotarySliderFillColourId, col);
}

void Knob::paint (juce::Graphics& g)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, col);
    const auto v = fmt ? fmt (slider.getValue()) : juce::String (slider.getValue(), 1);
    if (inlineLayout)
    {
        auto t = textArea;
        g.setFont (ui::font (9.5f, true)); g.setColour (ui::dim);
        const bool fits = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) <= t.getWidth();
        g.drawFittedText (fits ? name : shortLabel, t.removeFromTop (14), juce::Justification::bottomLeft, 1, 0.75f);
        g.setFont (ui::font (12.0f, true)); g.setColour (ui::text);
        g.drawFittedText (v, t, juce::Justification::topLeft, 1, 0.75f);
        return;
    }
    g.setFont (ui::font (9.5f, true));
    g.setColour (ui::dim);
    g.drawFittedText (name, getLocalBounds().removeFromTop (12), juce::Justification::centred, 1);
    g.setFont (ui::font (10.5f));
    g.setColour (ui::text);
    g.drawFittedText (v, getLocalBounds().removeFromBottom (13), juce::Justification::centred, 1);
}

// ==================================================================== slot
static juce::String shortParamName (const juce::String& n)
{
    static const std::map<juce::String, juce::String> m {
        { u8 ("CORTE GRAVE"), u8 ("C. GRAVE") }, { u8 ("CORTE AGUDO"), u8 ("C. AGUDO") }, { u8 ("MÉDIO FREQ"), u8 ("FREQ MÉD") },
        { u8 ("REPETIÇÕES"), u8 ("REPET.") }, { u8 ("DECAIMENTO"), u8 ("DECAIM.") }, { u8 ("MODULAÇÃO"), u8 ("MODUL.") },
        { u8 ("PING-PONG"), u8 ("PING-P.") }, { u8 ("GRAVES MONO"), u8 ("GRV MONO") }, { u8 ("PROFUNDIDADE"), u8 ("PROF.") },
        { u8 ("RESSONÂNCIA"), u8 ("RESSON.") }, { u8 ("REDUÇÃO MÁX"), u8 ("REDUÇÃO") }, { u8 ("VELOCIDADE"), u8 ("VELOC.") },
        { u8 ("FREQUÊNCIA"), u8 ("FREQ.") }, { u8 ("OSCILAÇÃO"), u8 ("OSCIL.") }, { u8 ("PRÉ-DELAY"), u8 ("PRÉ-DLY") },
        { u8 ("REALIM."), u8 ("REALIM.") }, { u8 ("LFO PROF."), u8 ("LFO PR.") }, { u8 ("ENVELOPE"), u8 ("ENVEL.") },
        { u8 ("TAMANHO"), u8 ("TAMANHO") }, { u8 ("LARGURA"), u8 ("LARGURA") } };
    auto it = m.find (n);
    return it != m.end() ? it->second : n;
}

SlotStrip::SlotStrip (WyattRackProcessor& p, int i) : index (i), proc (p)
{
    typeBox.setTextWhenNothingSelected (u8 ("+  ADICIONAR BLOCO"));
    for (int t = 1; t < kNumTypes; ++t) typeBox.addItem (u8 (info (t).name), t);
    typeBox.addSeparator();
    typeBox.addItem (u8 ("— tirar este bloco"), 100);
    typeBox.onChange = [this]
    {
        const int id = typeBox.getSelectedId();
        if (id <= 0) return;
        proc.setSlotType (index, id == 100 ? 0 : id);
    };
    addAndMakeVisible (typeBox);

    onBtn.setClickingTogglesState (true);
    onBtn.getProperties().set ("lamp", true);
    onBtn.setTooltip ("Liga/desliga este bloco");
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, WyattRackProcessor::slotId (i, "on"), onBtn);
    addAndMakeVisible (onBtn);

    const char* key[] = { "p1", "p2", "p3", "p4", "p5", "p6" };
    for (int k = 0; k < kNumParams; ++k)
    {
        knobs[k].attach (proc.apvts, WyattRackProcessor::slotId (i, key[k]));
        knobs[k].setInline (true);
        addChildComponent (knobs[k]);
    }
    mix.attach (proc.apvts, WyattRackProcessor::slotId (i, "mix"));
    mix.setLabel ("MIX");
    mix.setInline (true);
    mix.setFormatter ([] (double v) { return juce::String (juce::roundToInt (v)) + "%"; });
    addChildComponent (mix);

    for (auto* b : { &upBtn, &downBtn, &delBtn }) { b->setColour (juce::TextButton::buttonColourId, ui::metalHi); addChildComponent (b); }
    upBtn.setTooltip ("Subir na cadeia"); downBtn.setTooltip ("Descer na cadeia"); delBtn.setTooltip ("Tirar o bloco");
    upBtn.onClick = [this] { proc.moveSlot (blockIndex(), blockIndex() - 1); };
    downBtn.onClick = [this] { proc.moveSlot (blockIndex(), blockIndex() + 1); };
    delBtn.onClick = [this] { proc.clearSlot (index); };
    refresh();
}

int SlotStrip::blockIndex() const
{
    int n = 0;
    for (int s = 0; s < index; ++s) if ((int) proc.apvts.getRawParameterValue (WyattRackProcessor::slotId (s, "tipo"))->load() > 0) ++n;
    return n;
}

void SlotStrip::refresh()
{
    const int t = (int) proc.apvts.getRawParameterValue (WyattRackProcessor::slotId (index, "tipo"))->load();
    if (t == shownType) return;
    shownType = t;
    typeBox.setSelectedId (t > 0 ? t : 0, juce::dontSendNotification);
    const auto col = ui::familyColour (t);
    const auto& bi = info (t);
    for (int k = 0; k < kNumParams; ++k)
    {
        const auto& ps = bi.params[(size_t) k];
        knobs[k].setVisible (t > 0 && ps.used());
        if (! ps.used()) continue;
        knobs[k].setLabel (u8 (ps.name), shortParamName (u8 (ps.name)));
        knobs[k].setColour (col);
        knobs[k].setFormatter ([t, k] (double v)
        {
            const auto& sp = info (t).params[(size_t) k];
            return juce::String::fromUTF8 (sp.format (sp.toReal ((float) v)).c_str());
        });
        knobs[k].slider.setTooltip (u8 (ps.name));
    }
    mix.setVisible (t > 0); mix.setColour (col);
    for (auto* b : { &upBtn, &downBtn, &delBtn }) b->setVisible (t > 0);
    onBtn.setVisible (t > 0);
    onBtn.setColour (juce::TextButton::buttonOnColourId, col.withAlpha (0.9f));
    onBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff101114));
    typeBox.setColour (juce::ComboBox::outlineColourId, t > 0 ? col.withAlpha (0.6f) : ui::line);
    typeBox.setColour (juce::ComboBox::textColourId, t > 0 ? ui::text : ui::dim);
    typeBox.setTooltip (u8 (bi.description));
    resized();
    repaint();
}

void SlotStrip::paint (juce::Graphics& g)
{
    const int t = shownType;
    auto r = getLocalBounds().toFloat().reduced (0, 1.5f);
    // painel escovado
    juce::ColourGradient grad (t > 0 ? ui::metalHi : juce::Colour (0xff18191d), 0, r.getY(), t > 0 ? ui::metal : juce::Colour (0xff141518), 0, r.getBottom(), false);
    g.setGradientFill (grad); g.fillRoundedRectangle (r, 3);
    g.setColour (juce::Colours::white.withAlpha (t > 0 ? 0.05f : 0.02f));
    for (float yy = r.getY() + 3; yy < r.getBottom() - 2; yy += 3) g.drawHorizontalLine ((int) yy, r.getX() + 4, r.getRight() - 4);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 3, 1);

    // número + LED
    const auto col = ui::familyColour (t);
    g.setFont (ui::font (15, false, true));
    g.setColour (t > 0 ? ui::text.withAlpha (0.85f) : ui::dim.withAlpha (0.5f));
    g.drawText (juce::String (index + 1), juce::Rectangle<float> (r.getX(), r.getY(), 30, r.getHeight()), juce::Justification::centred);
    const bool on = proc.apvts.getRawParameterValue (WyattRackProcessor::slotId (index, "on"))->load() >= 0.5f;
    auto led = juce::Rectangle<float> (r.getX() + 30, r.getY() + 8, 4, r.getHeight() - 16);
    g.setColour (t > 0 && on ? col : col.withAlpha (0.18f)); g.fillRoundedRectangle (led, 2);
    if (t > 0 && on) { g.setColour (col.withAlpha (0.25f)); g.fillRoundedRectangle (led.expanded (2.5f), 3); }

    if (t <= 0)
    {
        g.setFont (ui::font (11));
        g.setColour (ui::dim.withAlpha (0.6f));
        g.drawText (u8 ("slot livre — escolha um bloco ou peça lá em cima"), r.withTrimmedLeft (230), juce::Justification::centredLeft);
        return;
    }
    // medidor de atividade
    auto m = juce::Rectangle<float> (r.getRight() - 9, r.getY() + 7, 4, r.getHeight() - 14);
    g.setColour (juce::Colour (0xff0b0b0d)); g.fillRoundedRectangle (m, 2);
    const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (meter, -60.0f) + 60.0f) / 60.0f);
    g.setColour (meter > 0.98f ? ui::red : col); g.fillRoundedRectangle (m.withTop (m.getBottom() - m.getHeight() * lv), 2);
}

void SlotStrip::resized()
{
    auto r = getLocalBounds().reduced (0, 4);
    r.removeFromLeft (40);
    typeBox.setBounds (r.removeFromLeft (152).withSizeKeepingCentre (152, 26));
    r.removeFromLeft (8);
    onBtn.setBounds (r.removeFromLeft (22).withSizeKeepingCentre (22, 22));
    r.removeFromLeft (8);
    r.removeFromRight (14);
    auto btns = r.removeFromRight (84);
    const int bw = 24;
    upBtn.setBounds (btns.removeFromLeft (bw + 4).withSizeKeepingCentre (bw, 22));
    downBtn.setBounds (btns.removeFromLeft (bw + 4).withSizeKeepingCentre (bw, 22));
    delBtn.setBounds (btns.removeFromLeft (bw + 4).withSizeKeepingCentre (bw, 22));
    mix.setBounds (r.removeFromRight (82));
    r.removeFromRight (6);
    const int kw = r.getWidth() / kNumParams;
    for (int k = 0; k < kNumParams; ++k) knobs[k].setBounds (r.removeFromLeft (kw));
}

// ==================================================================== configurações da IA
namespace
{
class SettingsPanel final : public juce::Component
{
public:
    SettingsPanel (WyattRackProcessor& p, std::function<void()> onClose) : proc (p), close (std::move (onClose))
    {
        keyBox.setPasswordCharacter ((juce::juce_wchar) 0x2022);
        keyBox.setText (proc.apiKey(), false);
        keyBox.setTextToShowWhenEmpty ("sk-ant-...", ui::dim);
        keyBox.setColour (juce::TextEditor::backgroundColourId, ui::vfdBg);
        keyBox.setFont (ui::font (15));
        keyBox.setIndents (10, 8);
        addAndMakeVisible (keyBox);

        model.addItem (u8 ("Rápido e econômico (Claude Haiku 5.5)"), 1);
        model.addItem (u8 ("Caprichado (Claude Sonnet 5.5)"), 2);
        model.setSelectedId (proc.aiModel().contains ("sonnet") ? 2 : 1, juce::dontSendNotification);
        model.setColour (juce::ComboBox::outlineColourId, ui::line);
        addAndMakeVisible (model);

        adapt.setButtonText (u8 ("Ajustar limiares ao nível da faixa (análise automática)"));
        adapt.setToggleState (proc.adaptToTrack, juce::dontSendNotification);
        addAndMakeVisible (adapt);

        link.setButtonText (u8 ("Criar uma chave em console.anthropic.com"));
        link.setURL (juce::URL ("https://console.anthropic.com/settings/keys"));
        link.setFont (ui::font (13), false, juce::Justification::centredLeft);
        addAndMakeVisible (link);

        save.setColour (juce::TextButton::buttonColourId, ui::red);
        save.onClick = [this]
        {
            proc.setApiKey (keyBox.getText(), model.getSelectedId() == 2 ? "claude-sonnet-5-5" : "claude-haiku-5-5");
            proc.adaptToTrack = adapt.getToggleState();
            close();
        };
        cancel.setColour (juce::TextButton::buttonColourId, ui::metalHi);
        cancel.onClick = [this] { close(); };
        addAndMakeVisible (save); addAndMakeVisible (cancel);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.7f));
        auto p = panel().toFloat();
        g.setColour (ui::metal); g.fillRoundedRectangle (p, 8);
        g.setColour (ui::line); g.drawRoundedRectangle (p, 8, 1);
        auto r = panel().reduced (28, 22);
        g.setFont (ui::font (20, false, true)); g.setColour (ui::text);
        g.drawText (u8 ("IA AVANÇADA"), r.removeFromTop (28), juce::Justification::centredLeft);
        g.setFont (ui::font (13)); g.setColour (ui::dim);
        g.drawFittedText (u8 ("O cérebro offline resolve os pedidos comuns na hora, sem internet. Para pedidos livres e criativos, "
                              "o WYATT RACK pode perguntar a uma IA online. O som continua 100% no plugin: a IA só escolhe os blocos e os valores.\n\n"
                              "Cole aqui a sua chave da API da Anthropic. Ela fica salva só neste computador e o uso é cobrado na sua conta."),
                          r.removeFromTop (110), juce::Justification::topLeft, 7);
        g.setFont (ui::font (11, true)); g.setColour (ui::amber);
        g.drawText (u8 ("CHAVE DA API"), keyBox.getBounds().translated (0, -16).withHeight (14), juce::Justification::left);
        g.drawText (u8 ("MODELO"), model.getBounds().translated (0, -16).withHeight (14), juce::Justification::left);
    }

    void resized() override
    {
        auto r = panel().reduced (28, 22);
        r.removeFromTop (28 + 110 + 18);
        keyBox.setBounds (r.removeFromTop (36)); r.removeFromTop (24);
        model.setBounds (r.removeFromTop (30).withWidth (320)); r.removeFromTop (12);
        adapt.setBounds (r.removeFromTop (26)); r.removeFromTop (4);
        link.setBounds (r.removeFromTop (22));
        auto b = r.removeFromBottom (34);
        save.setBounds (b.removeFromRight (120)); b.removeFromRight (10);
        cancel.setBounds (b.removeFromRight (100));
    }
    void mouseDown (const juce::MouseEvent& e) override { if (! panel().contains (e.getPosition())) close(); }

    juce::TextEditor keyBox;
private:
    juce::Rectangle<int> panel() const { return getLocalBounds().withSizeKeepingCentre (600, 440); }
    WyattRackProcessor& proc;
    std::function<void()> close;
    juce::ComboBox model;
    juce::ToggleButton adapt;
    juce::HyperlinkButton link;
    juce::TextButton save { "SALVAR" }, cancel { "FECHAR" };
};
}

// ==================================================================== editor
static const char* kChips[] = { "voz gospel de igreja", "mais brilho", "menos reverb", "telefone", "anos 80 sonhador", "me surpreenda", "zera tudo" };

WyattRackEditor::WyattRackEditor (WyattRackProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    promptBox.setTextToShowWhenEmpty (u8 ("Descreva o som que você quer…  ex.: voz quente, na cara, com reverb de igreja"), ui::amber.withAlpha (0.35f));
    promptBox.setFont (ui::font (19, true));
    promptBox.setColour (juce::TextEditor::backgroundColourId, ui::vfdBg);
    promptBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    promptBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    promptBox.setIndents (14, 0);
    promptBox.setJustification (juce::Justification::centredLeft);
    promptBox.onReturnKey = [this] { submitPrompt(); };
    promptBox.setText (proc.lastPrompt(), false);
    addAndMakeVisible (promptBox);

    createBtn.setColour (juce::TextButton::buttonColourId, ui::red);
    createBtn.setTooltip (u8 ("Montar o tratamento (Enter)"));
    createBtn.onClick = [this] { submitPrompt(); };
    addAndMakeVisible (createBtn);

    for (auto* c : kChips)
    {
        auto b = std::make_unique<juce::TextButton> (u8 (c));
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff17140d));
        b->setColour (juce::TextButton::textColourOffId, ui::amber.withAlpha (0.8f));
        const juce::String txt = u8 (c);
        b->onClick = [this, txt] { promptBox.setText (txt, false); submitPrompt(); };
        addAndMakeVisible (*b);
        chips.push_back (std::move (b));
    }

    const char* modes[] = { "OFFLINE", "HÍBRIDO", "ONLINE" };
    const char* tips[] = { "Só o cérebro offline: rápido, sem internet",
                           "Offline primeiro; pedidos que ele não entende vão para a IA avançada",
                           "Todo pedido vai para a IA avançada (precisa de internet)" };
    for (int i = 0; i < 3; ++i)
    {
        modeBtns[i].setButtonText (u8 (modes[i]));
        modeBtns[i].setTooltip (u8 (tips[i]));
        modeBtns[i].setRadioGroupId (77);
        modeBtns[i].setClickingTogglesState (true);
        modeBtns[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff101114));
        modeBtns[i].setColour (juce::TextButton::buttonOnColourId, i == 0 ? ui::amber.darker (0.3f) : (i == 1 ? ui::red : juce::Colour (0xff2f6fae)));
        modeBtns[i].onClick = [this, i] { if (modeBtns[i].getToggleState()) proc.setAiMode ((AiMode) i); };
        addAndMakeVisible (modeBtns[i]);
    }
    gearBtn.setButtonText (u8 ("\xe2\x9a\x99  IA"));
    gearBtn.setTooltip (u8 ("Configurar a IA avançada"));
    gearBtn.setColour (juce::TextButton::buttonColourId, ui::metalHi);
    gearBtn.onClick = [this] { openSettings(); };
    addAndMakeVisible (gearBtn);

    for (int s = 0; s < kNumSlots; ++s)
    {
        slots.push_back (std::make_unique<SlotStrip> (proc, s));
        addAndMakeVisible (*slots.back());
    }

    undoBtn.setButtonText (u8 ("\xe2\x86\xb6  DESFAZER"));
    redoBtn.setButtonText (u8 ("REFAZER  \xe2\x86\xb7"));
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    libBtn.onClick = [this] { showLibraryMenu(); };
    saveBtn.onClick = [this] { askSaveName(); };
    for (auto* b : { &undoBtn, &redoBtn, &libBtn, &saveBtn }) { b->setColour (juce::TextButton::buttonColourId, ui::metalHi); addAndMakeVisible (*b); }

    inKnob.attach (proc.apvts, "entrada"); inKnob.setLabel ("ENTRADA");
    outKnob.attach (proc.apvts, "saida"); outKnob.setLabel (u8 ("SAÍDA"));
    for (auto* k : { &inKnob, &outKnob })
    {
        k->setColour (ui::text);
        k->setFormatter ([] (double v) { return (v > 0 ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB"; });
        k->slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (*k);
    }
    bypassBtn.setClickingTogglesState (true);
    bypassBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff101114));
    bypassBtn.setColour (juce::TextButton::buttonOnColourId, ui::red);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassBtn);
    addAndMakeVisible (bypassBtn);

    proc.addChangeListener (this);
    refreshAll();
    setResizable (true, true);
    setResizeLimits (980, 720, 1600, 1100);
    setSize (1100, 780);
    startTimerHz (30);
}

WyattRackEditor::~WyattRackEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void WyattRackEditor::submitPrompt()
{
    const auto t = promptBox.getText().trim();
    if (t.isEmpty()) return;
    proc.ask (t);
    refreshAll();
}

void WyattRackEditor::openSettings()
{
    settings = std::make_unique<SettingsPanel> (proc, [this] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<WyattRackEditor> (this)] { if (sp) sp->closeSettings(); }); });
    addAndMakeVisible (*settings);
    settings->setBounds (getLocalBounds());
}

void WyattRackEditor::closeSettings()
{
    settings.reset();
    refreshAll();
}

void WyattRackEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refreshAll(); }

void WyattRackEditor::refreshAll()
{
    for (auto& s : slots) s->refresh();
    for (int i = 0; i < 3; ++i) modeBtns[i].setToggleState ((int) proc.aiMode() == i, juce::dontSendNotification);
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
    const bool busy = proc.isThinking();
    createBtn.setEnabled (! busy);
    createBtn.setButtonText (busy ? juce::String ("...") : juce::String ("CRIAR"));
    repaint();
}

void WyattRackEditor::timerCallback()
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& st = *slots[(size_t) s];
        const float a = proc.slotActivity (s);
        st.meter = std::max (a, st.meter * 0.85f);
        st.refresh();
        st.repaint (st.getWidth() - 14, 0, 14, st.getHeight());
    }
    inMeter = std::max (proc.rack.inPeak.load(), inMeter * 0.85f);
    outMeter = std::max (proc.rack.outPeak.load(), outMeter * 0.85f);
    repaint (footerArea);
    if (proc.isThinking()) { ++thinkingPhase; repaint (replyArea); }
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
}

void WyattRackEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);

    // cabeçalho
    auto h = headerArea.reduced (20, 0);
    g.setFont (ui::font (28, false, true));
    g.setColour (ui::text);
    const int w1 = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), "WYATT ");
    g.drawText ("WYATT ", h.removeFromLeft (w1), juce::Justification::centredLeft);
    g.setColour (ui::red);
    g.drawText ("RACK", h.removeFromLeft (100), juce::Justification::centredLeft);
    g.setFont (ui::font (12));
    g.setColour (ui::dim);
    g.drawText (u8 ("WYATT Sounds  ·  fale o som, o rack monta"), h.removeFromLeft (300), juce::Justification::centredLeft);

    const bool key = proc.hasApiKey();
    auto st = juce::Rectangle<int> (modeBtns[0].getX() - 150, headerArea.getY(), 140, headerArea.getHeight());
    g.setFont (ui::font (11));
    g.setColour (key ? ui::online : ui::dim);
    g.drawText (key ? u8 ("IA avançada pronta") : (proc.aiMode() == AiMode::Offline ? juce::String() : u8 ("IA avançada sem chave")), st, juce::Justification::centredRight);

    // console VFD
    auto c = consoleArea.toFloat();
    g.setColour (juce::Colour (0xff050505)); g.fillRoundedRectangle (c, 8);
    g.setColour (ui::line); g.drawRoundedRectangle (c.reduced (0.5f), 8, 1);
    auto disp = promptBox.getBounds().toFloat().expanded (4);
    g.setColour (ui::vfdBg); g.fillRoundedRectangle (disp, 5);
    g.setColour (ui::amber.withAlpha (0.18f)); g.drawRoundedRectangle (disp, 5, 1);

    // resposta
    auto r = replyArea.reduced (20, 2);
    const auto& lg = proc.log();
    g.setFont (ui::font (10.5f, true));
    if (proc.isThinking())
    {
        g.setColour (ui::online);
        g.drawText (u8 ("IA AVANÇADA"), r.removeFromTop (16), juce::Justification::left);
        g.setFont (ui::font (15));
        juce::String dots; for (int i = 0; i < (thinkingPhase / 8) % 4; ++i) dots << ".";
        g.drawText (u8 ("Pensando no seu som") + dots, r, juce::Justification::topLeft);
    }
    else if (! lg.empty())
    {
        const auto& e = lg.back();
        g.setColour (e.online ? ui::online : ui::amber);
        g.drawText (e.online ? u8 ("IA AVANÇADA") : u8 ("CÉREBRO OFFLINE"), r.removeFromTop (16), juce::Justification::left);
        g.setFont (ui::font (14.5f));
        g.setColour (e.ok ? ui::text : ui::text.withAlpha (0.7f));
        g.drawFittedText (e.reply, r, juce::Justification::topLeft, 3, 1.0f);
    }
    else
    {
        g.setColour (ui::dim);
        g.drawText (u8 ("COMO USAR"), r.removeFromTop (16), juce::Justification::left);
        g.setFont (ui::font (14.5f));
        g.drawFittedText (u8 ("Escreva como você falaria com o técnico: estilo, instrumento, artista de referência, \"mais brilho\", \"sem delay\", \"compressor com ratio 4\". "
                              "Depois ajuste os knobs à vontade — tudo é automatizável na DAW."), r, juce::Justification::topLeft, 3, 1.0f);
    }

    // rack: moldura com orelhas e parafusos
    auto rk = rackArea.toFloat();
    g.setColour (juce::Colour (0xff0a0a0c)); g.fillRoundedRectangle (rk, 6);
    for (auto ear : { rk.withWidth (26), rk.withLeft (rk.getRight() - 26) })
    {
        juce::ColourGradient eg (ui::metalHi, ear.getX(), 0, ui::metal, ear.getRight(), 0, false);
        g.setGradientFill (eg); g.fillRoundedRectangle (ear, 5);
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto& sl = *slots[(size_t) s];
            const float cy = (float) sl.getBounds().getCentreY();
            g.setColour (juce::Colour (0xff09090a)); g.fillEllipse (ear.getCentreX() - 4.5f, cy - 4.5f, 9, 9);
            g.setColour (juce::Colour (0xff6a6d75)); g.drawEllipse (ear.getCentreX() - 4.5f, cy - 4.5f, 9, 9, 1.2f);
            g.drawLine (ear.getCentreX() - 2.5f, cy, ear.getCentreX() + 2.5f, cy, 1.2f);
        }
    }

    // rodapé: medidores
    auto meterAt = [&] (juce::Rectangle<int> m, float v, const juce::String& label)
    {
        g.setFont (ui::font (9.5f, true)); g.setColour (ui::dim);
        g.drawText (label, m.removeFromTop (12), juce::Justification::centred);
        auto bar = m.reduced (0, 4).toFloat();
        g.setColour (juce::Colour (0xff0b0b0d)); g.fillRoundedRectangle (bar, 2);
        const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 60.0f);
        g.setColour (v > 0.98f ? ui::red : ui::amber.withAlpha (0.9f));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * lv), 2);
    };
    const int mx = inKnob.getX() - 150;
    meterAt ({ mx, footerArea.getY() + 10, 120, 24 }, inMeter, "IN");
    meterAt ({ mx, footerArea.getY() + 34, 120, 24 }, outMeter, "OUT");
}

void WyattRackEditor::paintOverChildren (juce::Graphics& g)
{
    // brilho sutil de vidro no display
    if (settings) return;
    auto d = promptBox.getBounds().toFloat();
    juce::ColourGradient gl (juce::Colours::white.withAlpha (0.045f), 0, d.getY(), juce::Colours::transparentWhite, 0, d.getCentreY(), false);
    g.setGradientFill (gl); g.fillRect (d.withHeight (d.getHeight() * 0.5f));
}

void WyattRackEditor::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (56);
    {
        auto h = headerArea.reduced (20, 13);
        gearBtn.setBounds (h.removeFromRight (64)); h.removeFromRight (12);
        for (int i = 2; i >= 0; --i) { modeBtns[i].setBounds (h.removeFromRight (84)); h.removeFromRight (2); }
    }
    consoleArea = r.removeFromTop (112).reduced (20, 4);
    {
        auto c = consoleArea.reduced (14, 12);
        auto top = c.removeFromTop (46);
        createBtn.setBounds (top.removeFromRight (120)); top.removeFromRight (12);
        promptBox.setBounds (top.reduced (4));
        c.removeFromTop (10);
        auto chipRow = c.removeFromTop (24);
        for (auto& b : chips)
        {
            const int w = juce::GlyphArrangement::getStringWidthInt (ui::font (12, true), b->getButtonText()) + 22;
            b->setBounds (chipRow.removeFromLeft (w)); chipRow.removeFromLeft (8);
        }
    }
    footerArea = r.removeFromBottom (72);
    replyArea = r.removeFromTop (70).withTrimmedTop (8);
    rackArea = r.reduced (20, 6);
    {
        auto inner = rackArea.reduced (32, 6);
        const int sh = inner.getHeight() / kNumSlots;
        for (int s = 0; s < kNumSlots; ++s) slots[(size_t) s]->setBounds (inner.removeFromTop (sh));
    }
    {
        auto f = footerArea.reduced (20, 10);
        undoBtn.setBounds (f.removeFromLeft (110).withSizeKeepingCentre (110, 30)); f.removeFromLeft (8);
        redoBtn.setBounds (f.removeFromLeft (110).withSizeKeepingCentre (110, 30)); f.removeFromLeft (24);
        libBtn.setBounds (f.removeFromLeft (120).withSizeKeepingCentre (120, 30)); f.removeFromLeft (8);
        saveBtn.setBounds (f.removeFromLeft (90).withSizeKeepingCentre (90, 30));
        bypassBtn.setBounds (f.removeFromRight (90).withSizeKeepingCentre (90, 30)); f.removeFromRight (16);
        outKnob.setBounds (f.removeFromRight (70)); f.removeFromRight (4);
        inKnob.setBounds (f.removeFromRight (70));
    }
    if (settings) settings->setBounds (getLocalBounds());
}

// ==================================================================== biblioteca
void WyattRackEditor::showLibraryMenu()
{
    juce::PopupMenu m, fab, mine;
    const auto& f = factoryTreatments();
    for (int i = 0; i < (int) f.size(); ++i) fab.addItem (1000 + i, u8 (f[(size_t) i].name));
    const auto files = proc.libraryFiles();
    for (int i = 0; i < files.size(); ++i) mine.addItem (2000 + i, files[i].getFileNameWithoutExtension());
    if (files.isEmpty()) mine.addItem (-1, u8 ("(nenhum salvo ainda)"), false);
    m.addSectionHeader ("TRATAMENTOS");
    m.addSubMenu (u8 ("De fábrica"), fab);
    m.addSubMenu ("Meus tratamentos", mine);
    m.addSeparator();
    m.addItem (3, u8 ("Abrir arquivo…"));
    m.addItem (4, "Mostrar a pasta");
    m.addSeparator();
    m.addItem (5, "Copiar tratamento (texto)");
    m.addItem (6, "Colar tratamento");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&libBtn),
        [this, files, sp = juce::Component::SafePointer<WyattRackEditor> (this)] (int id)
        {
            if (! sp) return;
            if (id >= 1000 && id < 2000) { proc.setCurrentProgram (id - 1000); refreshAll(); }
            else if (id >= 2000 && id - 2000 < files.size()) proc.loadFromFile (files[id - 2000]);
            else if (id == 3)
            {
                chooser = std::make_unique<juce::FileChooser> ("Abrir tratamento", WyattRackProcessor::libraryFolder(), "*.wyrack;*.json");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadFromFile (fc.getResult()); });
            }
            else if (id == 4) { WyattRackProcessor::libraryFolder().createDirectory(); WyattRackProcessor::libraryFolder().revealToUser(); }
            else if (id == 5) juce::SystemClipboard::copyTextToClipboard (juce::JSON::toString (treatmentToVar (proc.currentTreatment())));
            else if (id == 6)
            {
                Treatment t; juce::String err;
                if (treatmentFromVar (juce::JSON::parse (juce::SystemClipboard::getTextFromClipboard()), t, err)) proc.applyTreatment (t);
            }
        });
}

void WyattRackEditor::askSaveName()
{
    auto* w = new juce::AlertWindow (u8 ("Salvar tratamento"), u8 ("Dê um nome para guardar na sua biblioteca:"), juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("nome", proc.lastPrompt().substring (0, 40));
    w->addButton ("SALVAR", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCELAR", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, sp = juce::Component::SafePointer<WyattRackEditor> (this)] (int r)
    {
        if (sp && r == 1) proc.saveToLibrary (w->getTextEditorContents ("nome"));
    }), true);
}
} // namespace wr
