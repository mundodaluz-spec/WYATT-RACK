// WMusic Rack — tema visual: metal escovado, painéis com parafusos, knobs cromados, teclas, fontes
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace wr::ui
{
// tamanho de projeto da tela (tudo é desenhado nesta grade e escalado)
constexpr int kDesignW = 1280, kDesignH = 820;

namespace col
{
const juce::Colour red    { 0xffd7262b };
const juce::Colour cream  { 0xffefe6d6 };
const juce::Colour dim    { 0xff8d8a85 };
const juce::Colour label  { 0xffb8b1a6 };
const juce::Colour faint  { 0xff6f6b66 };
const juce::Colour grid   { 0xff1c1c1f };
const juce::Colour grid2  { 0xff2a2a2e };
const juce::Colour blue   { 0xff6fb6ff };
const juce::Colour violet { 0xffb98cff };
const juce::Colour amber  { 0xffffb238 };
const juce::Colour green  { 0xff3fd17a };
const juce::Colour online { 0xff7cc4ff };
const juce::Colour display{ 0xff050506 };
}

juce::Colour familyColour (int blockType);

// Barlow Condensed (500 / 600 / 700)
juce::Font bc (float height, int weight = 600);

struct Assets
{
    juce::Image metal, knob, btnRed, btnCream, btnDark, logo;
    static const Assets& get();
};

void fillMetal (juce::Graphics&, juce::Rectangle<float>, float darken = 0.0f, float corner = 0.0f);
void drawPanel (juce::Graphics&, juce::Rectangle<float>, bool screws = true);
void drawScrew (juce::Graphics&, float x, float y);
void drawTitle (juce::Graphics&, const juce::String&, float x, float y, float size = 21.0f);
void drawDisplay (juce::Graphics&, juce::Rectangle<float>);
void drawStripBase (juce::Graphics&, juce::Rectangle<float>, bool active);
enum class KeyKind { Dark, Red, Cream };
void drawKey (juce::Graphics&, juce::Rectangle<float>, KeyKind, const juce::String& text, float fontSize, bool enabled = true, bool down = false);
void drawKnob (juce::Graphics&, juce::Rectangle<float> circle, float pos, bool valueArc = true);
void drawLed (juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool on);
void drawSegments (juce::Graphics&, float x, float y, int n, float w, float h, float level01, int redFrom);

// ---------------------------------------------------------------- componentes
// Tecla com a arte das teclas WMusic. Liga/desliga muda para a cor "acesa".
class KeyButton : public juce::Button
{
public:
    KeyButton (const juce::String& text = {}, KeyKind off = KeyKind::Dark, KeyKind on = KeyKind::Red, float fontSize = 14.0f);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    KeyKind offKind, onKind;
    float fontSize;
};

// Knob: arte cromada + arco + ponteiro; nome e valor desenhados ao lado (inline) ou em cima/embaixo (stacked)
class Knob : public juce::Component
{
public:
    enum class Layout { Inline, Stacked, Bare };
    Knob();
    juce::Slider slider;
    juce::String name;
    std::function<juce::String (double)> format;
    Layout layout = Layout::Stacked;
    float knobSize = 70, nameSize = 15, valueSize = 17;
    bool bipolar = false;
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Rectangle<float> knobArea() const;
};

class KnobLook : public juce::LookAndFeel_V4
{
public:
    KnobLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return bc (16, 700); }
    juce::Font getPopupMenuFont() override { return bc (17, 500); }
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void fillTextEditorBackground (juce::Graphics&, int, int, juce::TextEditor&) override {}
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
};
} // namespace wr::ui
