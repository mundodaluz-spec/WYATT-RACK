// WMusic Rack — tela (metal escovado WMusic). Tudo é desenhado numa grade de 1280×820 e escalado.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/Visualizers.h"

namespace wr
{
class WyattRackEditor;

// LED clicável (liga/desliga do bloco)
class LedButton : public juce::Button
{
public:
    LedButton() : juce::Button ("on") { setClickingTogglesState (true); }
    juce::Colour colour = ui::col::red;
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        ui::drawLed (g, getLocalBounds().toFloat().reduced (5.5f), colour, getToggleState());
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.15f)); g.drawEllipse (getLocalBounds().toFloat().reduced (1), 1); }
    }
};

// Uma linha do rack
class SlotStrip : public juce::Component
{
public:
    SlotStrip (WyattRackProcessor&, WyattRackEditor&, int index);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    const int index;
    float meter = 0;
    ui::KeyButton openBtn { "OPEN", ui::KeyKind::Dark, ui::KeyKind::Dark, 13 };
    juce::ComboBox typeBox;
private:
    WyattRackProcessor& proc;
    WyattRackEditor& editor;
    int shownType = -1;
    LedButton onBtn;
    ui::KeyButton upBtn { juce::String::fromUTF8 ("\xe2\x96\xb2"), ui::KeyKind::Dark, ui::KeyKind::Dark, 9 },
                  downBtn { juce::String::fromUTF8 ("\xe2\x96\xbc"), ui::KeyKind::Dark, ui::KeyKind::Dark, 9 },
                  delBtn { juce::String::fromUTF8 ("\xe2\x9c\x95"), ui::KeyKind::Dark, ui::KeyKind::Dark, 11 };
    ui::Knob knobs[kNumParams], mix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att[kNumParams], mixAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    int blockIndex() const;
};

// Painel expandido de um bloco
class ModulePage : public juce::Component, private juce::Timer
{
public:
    ModulePage (WyattRackProcessor&, WyattRackEditor&);
    ~ModulePage() override;
    void show (int slot);
    void hideModule();
    int slot() const { return current; }
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    ui::Visualizer viz;
    void timerCallback() override;    // público para os testes
private:
    WyattRackProcessor& proc;
    WyattRackEditor& editor;
    int current = -1, shownType = -1;
    ui::KeyButton prevBtn { juce::String::fromUTF8 ("\xe2\x97\x80  PREV"), ui::KeyKind::Dark, ui::KeyKind::Dark, 15 },
                  nextBtn { juce::String::fromUTF8 ("NEXT  \xe2\x96\xb6"), ui::KeyKind::Dark, ui::KeyKind::Dark, 15 },
                  onBtn { "ON", ui::KeyKind::Dark, ui::KeyKind::Cream, 15 },
                  backBtn { "BACK TO RACK", ui::KeyKind::Red, ui::KeyKind::Red, 15 };
    ui::Knob knobs[kNumParams], mix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att[kNumParams], mixAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    // análise
    juce::dsp::FFT fft { 11 };
    std::vector<float> fftBuf, window, specSmooth;
    void step (int dir);
    void attach();
};

class WyattRackEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer,
                              private juce::ChangeListener
{
public:
    enum class Page { Start, Rack, Module };
    explicit WyattRackEditor (WyattRackProcessor&);
    ~WyattRackEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override {}

    // usados também pelos testes de integração
    juce::TextEditor promptBox, bigPrompt;
    ui::KeyButton createBtn { "CREATE", ui::KeyKind::Red, ui::KeyKind::Red, 18 },
                  bigCreateBtn { "CREATE", ui::KeyKind::Red, ui::KeyKind::Red, 19 },
                  rackBtn { juce::String::fromUTF8 ("SIGNAL CHAIN  \xe2\x96\xb6"), ui::KeyKind::Dark, ui::KeyKind::Dark, 15 },
                  chatBtn { "AI CHAT", ui::KeyKind::Dark, ui::KeyKind::Dark, 13 },
                  undoBtn { juce::String::fromUTF8 ("\xe2\x86\xb6  UNDO") }, redoBtn { juce::String::fromUTF8 ("REDO  \xe2\x86\xb7") },
                  libBtn { "LIBRARY" }, saveBtn { "SAVE" }, gearBtn { "AI SETUP", ui::KeyKind::Dark, ui::KeyKind::Dark, 12 },
                  bypassBtn { "BYPASS", ui::KeyKind::Dark, ui::KeyKind::Red, 15 };
    ui::KeyButton modeBtns[3];
    void submitPrompt (bool fromStart = false);
    void openSettings();
    void closeSettings();
    bool settingsOpen() const { return settings != nullptr; }
    SlotStrip& slot (int i) { return *slots[(size_t) i]; }
    ModulePage& module() { return *modulePage; }
    void showPage (Page);
    void openModule (int slot);
    Page page() const { return currentPage; }
    float contentScale() const { return scale; }
    ui::Knob macroKnobs[kNumMacros];
    ui::KeyButton bakeBtn { "BAKE", ui::KeyKind::Dark, ui::KeyKind::Dark, 13 };

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (WyattRackEditor& e) : ed (e) {}
        void paint (juce::Graphics& g) override { ed.paintContent (g); }
        void mouseUp (const juce::MouseEvent& e) override { ed.contentClicked (e.getPosition()); }
        WyattRackEditor& ed;
    };
    void paintContent (juce::Graphics&);
    void contentClicked (juce::Point<int>);
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshAll();
    void showPresetMenu();
    void showLibraryMenu();
    void askSaveName();
    void layout();

    WyattRackProcessor& proc;
    ui::KnobLook look;
    Content content { *this };
    juce::TooltipWindow tooltips { this, 600 };
    std::vector<std::unique_ptr<SlotStrip>> slots;
    std::unique_ptr<ModulePage> modulePage;
    std::vector<std::unique_ptr<ui::KeyButton>> chips;
    struct Example { juce::String tag, text; std::unique_ptr<juce::Button> btn; };
    std::vector<Example> examples;
    ui::Knob inKnob, outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt, outAtt, macroAtt[kNumMacros];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    std::unique_ptr<juce::Component> settings;
    void refreshMacros();
    std::unique_ptr<juce::FileChooser> chooser;
    Page currentPage = Page::Rack;
    bool goRackAfterAnswer = false;
    size_t logSizeAtSubmit = 0;
    float inMeter = 0, outMeter = 0, scale = 0.9f;
    int thinkingPhase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WyattRackEditor)
};
} // namespace wr
