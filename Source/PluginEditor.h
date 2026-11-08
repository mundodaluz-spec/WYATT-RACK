// WYATT RACK — tela
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace wr
{
namespace ui
{
// Paleta
const juce::Colour bg       { 0xff121316 };
const juce::Colour metal    { 0xff1d1f24 };
const juce::Colour metalHi  { 0xff2a2d34 };
const juce::Colour line     { 0xff33363e };
const juce::Colour text     { 0xffe9e4d8 };
const juce::Colour dim      { 0xff8a8a90 };
const juce::Colour amber    { 0xffffb238 };
const juce::Colour vfdBg    { 0xff0b0b09 };
const juce::Colour red      { 0xffe0322b };
const juce::Colour online   { 0xff7cc4ff };
juce::Colour familyColour (int blockType);
juce::Font font (float h, bool bold = false, bool black = false);
}

class RackLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (13, true); }
    juce::Font getPopupMenuFont() override { return ui::font (14); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return ui::font (12, true); }
    juce::Font getLabelFont (juce::Label&) override { return ui::font (12); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
};

// Knob com nome em cima e valor embaixo
class Knob final : public juce::Component
{
public:
    Knob();
    void attach (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void setLabel (const juce::String& s, const juce::String& shortName = {}) { name = s; shortLabel = shortName.isEmpty() ? s : shortName; repaint(); }
    void setColour (juce::Colour c) { col = c; slider.repaint(); }
    void setFormatter (std::function<juce::String (double)> f) { fmt = std::move (f); repaint(); }
    void setInline (bool b) { inlineLayout = b; resized(); }
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String name, shortLabel;
    juce::Colour col { ui::amber };
    bool inlineLayout = false;
    juce::Rectangle<int> textArea;
    std::function<juce::String (double)> fmt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class SlotStrip final : public juce::Component
{
public:
    SlotStrip (WyattRackProcessor&, int index);
    void refresh();                 // atualiza nomes/visibilidade se o tipo mudou
    void paint (juce::Graphics&) override;
    void resized() override;
    int index;
    float meter = 0;
private:
    WyattRackProcessor& proc;
    int shownType = -1;
    juce::ComboBox typeBox;
    juce::TextButton onBtn { "" }, upBtn { juce::String::fromUTF8 ("\xe2\x96\xb2") }, downBtn { juce::String::fromUTF8 ("\xe2\x96\xbc") }, delBtn { juce::String::fromUTF8 ("\xe2\x9c\x95") };
    Knob knobs[kNumParams];
    Knob mix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    int blockIndex() const;         // posição deste slot entre os blocos ocupados
};

class WyattRackEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer,
                              private juce::ChangeListener
{
public:
    explicit WyattRackEditor (WyattRackProcessor&);
    ~WyattRackEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // usados pelos testes de integração
    juce::TextEditor promptBox;
    juce::TextButton createBtn { "CRIAR" }, undoBtn, redoBtn, libBtn { "BIBLIOTECA" }, saveBtn { "SALVAR" }, gearBtn;
    juce::TextButton modeBtns[3];
    void submitPrompt();
    void openSettings();
    void closeSettings();
    bool settingsOpen() const { return settings != nullptr; }
    SlotStrip& slot (int i) { return *slots[(size_t) i]; }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshAll();
    void showLibraryMenu();
    void askSaveName();

    WyattRackProcessor& proc;
    RackLookAndFeel lnf;
    std::vector<std::unique_ptr<SlotStrip>> slots;
    std::vector<std::unique_ptr<juce::TextButton>> chips;
    Knob inKnob, outKnob;
    juce::TextButton bypassBtn { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    std::unique_ptr<juce::Component> settings;
    juce::Rectangle<int> headerArea, consoleArea, replyArea, rackArea, footerArea;
    float inMeter = 0, outMeter = 0;
    int thinkingPhase = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WyattRackEditor)
};
} // namespace wr
