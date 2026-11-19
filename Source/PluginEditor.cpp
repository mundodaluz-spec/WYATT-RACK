#include "PluginEditor.h"
#include "UI/Labels.h"
#include "BinaryData.h"

namespace wr
{
using namespace ui;
static juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
static const char* kChips[] = { "voz gospel de igreja", "mais brilho", "menos reverb", "telefone", "anos 80 sonhador", "me surpreenda", "zera tudo" };
static const char* kExamples[][2] = { { "VOCAL", "voz gospel com reverb de igreja" }, { "VOCAL", "voz trap estilo Travis Scott" }, { "DRUMS", "bateria suja e larga" },
                                      { "BASS", "baixo gordo" }, { "GUITAR", "violão aberto e espacial" }, { "MASTER", "master gospel com pressão" },
                                      { "FX", "efeito telefone" }, { "FX", "anos 80 sonhador" }, { "?", "me surpreenda" } };
static const char* kPKeys[] = { "p1", "p2", "p3", "p4", "p5", "p6" };

static int slotType (WyattRackProcessor& p, int s) { return (int) p.apvts.getRawParameterValue (WyattRackProcessor::slotId (s, "tipo"))->load(); }

static void styleEditor (juce::TextEditor& te, float fontSize)
{
    te.setFont (bc (fontSize, 600));
    te.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    te.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    te.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    te.setColour (juce::TextEditor::textColourId, col::cream);
}

// ==================================================================== SlotStrip
SlotStrip::SlotStrip (WyattRackProcessor& p, WyattRackEditor& e, int i) : index (i), proc (p), editor (e)
{
    typeBox.setTextWhenNothingSelected ("+ ADD BLOCK");
    for (int t = 1; t < kNumTypes; ++t) typeBox.addItem (blockName (t), t);
    typeBox.addSeparator();
    typeBox.addItem ("REMOVE BLOCK", 100);
    typeBox.onChange = [this]
    {
        const int id = typeBox.getSelectedId();
        if (id > 0) proc.setSlotType (index, id == 100 ? 0 : id);
    };
    addAndMakeVisible (typeBox);

    onBtn.setTooltip ("Block on / off");
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, WyattRackProcessor::slotId (i, "on"), onBtn);
    addChildComponent (onBtn);

    for (int k = 0; k < kNumParams; ++k)
    {
        auto& kn = knobs[k];
        kn.layout = Knob::Layout::Inline; kn.knobSize = 32; kn.nameSize = 12.5f; kn.valueSize = 14.5f;
        att[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, WyattRackProcessor::slotId (i, kPKeys[k]), kn.slider);
        addChildComponent (kn);
    }
    mix.layout = Knob::Layout::Inline; mix.knobSize = 32; mix.nameSize = 12.5f; mix.valueSize = 14.5f; mix.name = "MIX";
    mix.format = [] (double v) { return juce::String (juce::roundToInt (v)) + " %"; };
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, WyattRackProcessor::slotId (i, "mix"), mix.slider);
    addChildComponent (mix);

    openBtn.setTooltip ("Open the full panel of this block");
    openBtn.onClick = [this] { editor.openModule (index); };
    upBtn.setTooltip ("Move up"); downBtn.setTooltip ("Move down"); delBtn.setTooltip ("Remove");
    upBtn.onClick = [this] { proc.moveSlot (blockIndex(), blockIndex() - 1); };
    downBtn.onClick = [this] { proc.moveSlot (blockIndex(), blockIndex() + 1); };
    delBtn.onClick = [this] { proc.clearSlot (index); };
    for (auto* b : { &openBtn, &upBtn, &downBtn, &delBtn }) addChildComponent (b);
    refresh();
}

int SlotStrip::blockIndex() const
{
    int n = 0;
    for (int s = 0; s < index; ++s) if (slotType (proc, s) > 0) ++n;
    return n;
}

void SlotStrip::refresh()
{
    const int t = slotType (proc, index);
    if (t == shownType) return;
    shownType = t;
    typeBox.setSelectedId (t > 0 ? t : 0, juce::dontSendNotification);
    const auto& bi = info (t);
    for (int k = 0; k < kNumParams; ++k)
    {
        const auto& ps = bi.params[(size_t) k];
        knobs[k].setVisible (t > 0 && ps.used());
        if (! ps.used()) continue;
        knobs[k].name = paramShort (t, k);
        knobs[k].slider.setTooltip (paramName (t, k));
        knobs[k].format = [t, k] (double v) { return juce::String (formatValue (t, k, info (t).params[(size_t) k].toReal ((float) v))); };
        knobs[k].repaint();
    }
    mix.setVisible (t > 0);
    for (juce::Component* c : { (juce::Component*) &onBtn, (juce::Component*) &openBtn, (juce::Component*) &upBtn, (juce::Component*) &downBtn, (juce::Component*) &delBtn })
        c->setVisible (t > 0);
    onBtn.colour = familyColour (t);
    typeBox.setTooltip (blockTagline (t));
    repaint();
}

void SlotStrip::paint (juce::Graphics& g)
{
    const int t = shownType;
    auto r = getLocalBounds().toFloat();
    drawStripBase (g, r, t > 0);
    g.setFont (bc (22, 700)); g.setColour (t > 0 ? juce::Colour (0xffd9d1c2) : juce::Colour (0xff555555));
    g.drawText (juce::String (index + 1), juce::Rectangle<float> (6, 0, 26, r.getHeight()), juce::Justification::centred);
    if (t <= 0)
    {
        g.setFont (bc (15, 500)); g.setColour (juce::Colour (0xff6e6a64));
        g.drawText (u8 ("free slot \xe2\x80\x94 pick a block or ask above"), juce::Rectangle<float> (240, 0, 500, r.getHeight()), juce::Justification::centredLeft);
        return;
    }
    const auto c = familyColour (t);
    g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (34, 8, 8, r.getHeight() - 16, 3);
    g.setColour (c); g.fillRoundedRectangle (36, 10, 4, r.getHeight() - 20, 2);
    g.setColour (col::grid2); g.fillRect (mix.getX() - 6.0f, 12.0f, 1.0f, r.getHeight() - 24);
    auto m = juce::Rectangle<float> (r.getRight() - 10, 8, 4, r.getHeight() - 16);
    g.setColour (juce::Colour (0xff0a0a0b)); g.fillRoundedRectangle (m, 2);
    const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (meter, -60.0f) + 60.0f) / 60.0f);
    g.setColour (meter > 0.98f ? col::red : c); g.fillRoundedRectangle (m.withTop (m.getBottom() - m.getHeight() * lv), 2);
}

void SlotStrip::resized()
{
    typeBox.setBounds (46, 15, 170, 30);
    onBtn.setBounds (222, 16, 28, 28);
    for (int k = 0; k < kNumParams; ++k) knobs[k].setBounds (258 + k * 82, 6, 82, 48);
    mix.setBounds (258 + 6 * 82, 6, 80, 48);
    const int bx = getWidth() - 110;
    openBtn.setBounds (bx, 14, 50, 32);
    upBtn.setBounds (bx + 54, 14, 22, 15);
    downBtn.setBounds (bx + 54, 31, 22, 15);
    delBtn.setBounds (bx + 80, 14, 20, 32);
}

// ==================================================================== ModulePage
ModulePage::ModulePage (WyattRackProcessor& p, WyattRackEditor& e) : proc (p), editor (e)
{
    addAndMakeVisible (viz);
    for (auto* b : { &prevBtn, &nextBtn, &onBtn, &backBtn }) addAndMakeVisible (b);
    onBtn.setClickingTogglesState (true);
    prevBtn.onClick = [this] { step (-1); };
    nextBtn.onClick = [this] { step (1); };
    backBtn.onClick = [this] { editor.showPage (WyattRackEditor::Page::Rack); };
    for (auto& k : knobs) { k.layout = Knob::Layout::Stacked; k.knobSize = 70; addChildComponent (k); }
    mix.layout = Knob::Layout::Stacked; mix.knobSize = 70; mix.name = "MIX";
    mix.format = [] (double v) { return juce::String (juce::roundToInt (v)) + " %"; };
    addAndMakeVisible (mix);

    viz.setParam = [this] (int k, float v)
    {
        if (current < 0) return;
        const int t = slotType (proc, current);
        const auto& ps = info (t).params[(size_t) k];
        if (auto* prm = proc.slotParam (current, kPKeys[k])) prm->setValueNotifyingHost (ps.toNorm (ps.clamp (v)));
    };
    viz.beginDrag = [this] (int k) { if (current >= 0) if (auto* prm = proc.slotParam (current, kPKeys[k])) prm->beginChangeGesture(); };
    viz.endDrag = [this] (int k) { if (current >= 0) if (auto* prm = proc.slotParam (current, kPKeys[k])) prm->endChangeGesture(); };

    fftBuf.assign (4096, 0.0f);
    window.resize (2048);
    for (int i = 0; i < 2048; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / 2047.0f);
    specSmooth.assign (VizState::kSpecBins, -100.0f);
}

ModulePage::~ModulePage() { hideModule(); }

void ModulePage::show (int s)
{
    if (current >= 0 && current != s) proc.rack.slots[(size_t) current].scopeOn.store (false);
    current = s; shownType = -1;
    proc.rack.slots[(size_t) s].scopeOn.store (true);
    viz.state.hIn.assign (VizState::kHist, -100.0f);
    viz.state.hOut.assign (VizState::kHist, -100.0f);
    viz.state.hLive.assign (VizState::kHist, 0.0f);
    std::fill (specSmooth.begin(), specSmooth.end(), -100.0f);
    attach();
    refresh();
    startTimerHz (30);
}

void ModulePage::hideModule()
{
    stopTimer();
    if (current >= 0) proc.rack.slots[(size_t) current].scopeOn.store (false);
    for (auto& a : att) a.reset();
    mixAtt.reset(); onAtt.reset();
    current = -1;
}

void ModulePage::attach()
{
    for (auto& a : att) a.reset();
    mixAtt.reset(); onAtt.reset();
    if (current < 0) return;
    for (int k = 0; k < kNumParams; ++k)
        att[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, WyattRackProcessor::slotId (current, kPKeys[k]), knobs[k].slider);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, WyattRackProcessor::slotId (current, "mix"), mix.slider);
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, WyattRackProcessor::slotId (current, "on"), onBtn);
}

void ModulePage::step (int dir)
{
    if (current < 0) return;
    for (int s = current + dir; s >= 0 && s < kNumSlots; s += dir)
        if (slotType (proc, s) > 0) { show (s); return; }
}

void ModulePage::refresh()
{
    if (current < 0) return;
    const int t = slotType (proc, current);
    if (t <= 0) { editor.showPage (WyattRackEditor::Page::Rack); return; }
    bool hasPrev = false, hasNext = false;
    for (int s = 0; s < kNumSlots; ++s) if (slotType (proc, s) > 0) { if (s < current) hasPrev = true; if (s > current) hasNext = true; }
    prevBtn.setEnabled (hasPrev); nextBtn.setEnabled (hasNext);
    if (t == shownType) return;
    shownType = t;
    const auto& bi = info (t);
    for (int k = 0; k < kNumParams; ++k)
    {
        const auto& ps = bi.params[(size_t) k];
        knobs[k].setVisible (ps.used());
        if (! ps.used()) continue;
        knobs[k].name = paramName (t, k);
        knobs[k].format = [t, k] (double v) { return juce::String (formatValue (t, k, info (t).params[(size_t) k].toReal ((float) v))); };
    }
    resized();
    repaint();
}

void ModulePage::resized()
{
    viz.setBounds (24, 78, 928, 300);
    prevBtn.setBounds (528, 18, 86, 34);
    nextBtn.setBounds (618, 18, 86, 34);
    onBtn.setBounds (716, 18, 64, 34);
    backBtn.setBounds (792, 18, 168, 34);
    const int t = current >= 0 ? slotType (proc, current) : 0;
    int n = 1;
    for (int k = 0; k < kNumParams; ++k) if (info (t).params[(size_t) k].used()) ++n;
    const float w = 936.0f / (float) n;
    int i = 0;
    for (int k = 0; k < kNumParams; ++k)
        if (info (t).params[(size_t) k].used())
            knobs[k].setBounds (juce::Rectangle<float> (20 + w * (float) i++, 388, w, 140).toNearestInt());
    mix.setBounds (juce::Rectangle<float> (20 + w * (float) i, 388, w, 140).toNearestInt());
}

void ModulePage::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    if (current < 0) return;
    const int t = slotType (proc, current);
    const auto c = familyColour (t);
    auto box = juce::Rectangle<float> (20, 12, 42, 42);
    g.setColour (juce::Colour (0xff0a0a0b)); g.fillRoundedRectangle (box, 8);
    g.setColour (col::grid2); g.drawRoundedRectangle (box, 8, 1);
    g.setFont (bc (24, 700)); g.setColour (col::cream); g.drawText (juce::String (current + 1), box, juce::Justification::centred);
    g.setColour (c.withAlpha (0.3f)); g.fillRoundedRectangle (66, 16, 8, 34, 3);
    g.setColour (c); g.fillRoundedRectangle (68, 18, 4, 30, 2);
    drawTitle (g, blockName (t), 82, 10, 28);
    g.setFont (bc (14, 500)); g.setColour (juce::Colour (0xffa9a49c));
    g.drawText (blockTagline (t), 84, 46, 420, 18, juce::Justification::left);
    drawDisplay (g, juce::Rectangle<float> (20, 74, 936, 308));
    // separador antes do MIX
    g.setColour (col::grid2); g.fillRect ((float) mix.getX(), 400.0f, 1.0f, 118.0f);
}

void ModulePage::timerCallback()
{
    if (current < 0) return;
    refresh();
    if (current < 0) return;
    auto& st = viz.state;
    auto& sl = proc.rack.slots[(size_t) current];
    st.type = slotType (proc, current);
    const auto& bi = info (st.type);
    for (int k = 0; k < kNumParams; ++k)
        st.p[k] = bi.params[(size_t) k].used() ? bi.params[(size_t) k].toReal (proc.apvts.getRawParameterValue (WyattRackProcessor::slotId (current, kPKeys[k]))->load()) : 0.0f;
    st.mix = proc.apvts.getRawParameterValue (WyattRackProcessor::slotId (current, "mix"))->load();
    st.on = proc.apvts.getRawParameterValue (WyattRackProcessor::slotId (current, "on"))->load() >= 0.5f;
    st.live = sl.live.load();
    st.inDb = juce::Decibels::gainToDecibels (sl.inLevel.load(), -100.0f);
    st.outDb = juce::Decibels::gainToDecibels (sl.activity.load(), -100.0f);
    st.bpm = proc.hostBpm.load();
    auto push = [] (std::vector<float>& h, float v) { if (! h.empty()) { std::rotate (h.begin(), h.begin() + 1, h.end()); h.back() = v; } };
    push (st.hIn, st.inDb); push (st.hOut, st.outDb);
    const bool dyn = st.type == (int) BlockType::Compressor || st.type == (int) BlockType::Gate || st.type == (int) BlockType::DeEsser || st.type == (int) BlockType::Limitador;
    push (st.hLive, dyn ? st.live : 0.0f);

    st.scopeL.resize (2048); st.scopeR.resize (2048);
    sl.scope.read (st.scopeL.data(), st.scopeR.data(), 2048);
    // espectro: FFT de 2048 pontos com janela de Hann, agrupado em faixas logarítmicas
    std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);
    for (int i = 0; i < 2048; ++i) fftBuf[(size_t) i] = 0.5f * (st.scopeL[(size_t) i] + st.scopeR[(size_t) i]) * window[(size_t) i];
    fft.performFrequencyOnlyForwardTransform (fftBuf.data());
    const double sr = juce::jmax (8000.0, proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
    st.specDb.resize (VizState::kSpecBins);
    for (int b = 0; b < VizState::kSpecBins; ++b)
    {
        const double f0 = 20.0 * std::pow (1000.0, (double) b / VizState::kSpecBins), f1 = 20.0 * std::pow (1000.0, (double) (b + 1) / VizState::kSpecBins);
        int i0 = (int) (f0 / sr * 2048), i1 = juce::jmax (i0 + 1, (int) (f1 / sr * 2048));
        i0 = juce::jlimit (1, 1023, i0); i1 = juce::jlimit (i0 + 1, 1024, i1);
        float m = 0; for (int i = i0; i < i1; ++i) m = juce::jmax (m, fftBuf[(size_t) i]);
        // inclinação de +3 dB/oitava: música soa "plana" no gráfico
        const float db = juce::Decibels::gainToDecibels (m / 512.0f, -100.0f) + 3.0f * (float) std::log2 (juce::jmax (20.0, f0) / 1000.0);
        auto& sm = specSmooth[(size_t) b];
        sm = db > sm ? db : sm - 1.5f;
        st.specDb[(size_t) b] = sm;
    }
    viz.repaint();
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
        keyBox.setTextToShowWhenEmpty ("sk-ant-...", col::dim);
        styleEditor (keyBox, 18); keyBox.setIndents (12, 9);
        addAndMakeVisible (keyBox);
        model.addItem ("Fast and cheap (Claude Haiku 5.5)", 1);
        model.addItem ("Best quality (Claude Sonnet 5.5)", 2);
        model.setSelectedId (proc.aiModel().contains ("sonnet") ? 2 : 1, juce::dontSendNotification);
        addAndMakeVisible (model);
        adapt.setButtonText ("Match thresholds to the track level (automatic analysis)");
        adapt.setToggleState (proc.adaptToTrack, juce::dontSendNotification);
        addAndMakeVisible (adapt);
        link.setButtonText ("Get an API key at console.anthropic.com");
        link.setURL (juce::URL ("https://console.anthropic.com/settings/keys"));
        link.setFont (bc (16, 500), false, juce::Justification::centredLeft);
        addAndMakeVisible (link);
        save.onClick = [this]
        {
            proc.setApiKey (keyBox.getText(), model.getSelectedId() == 2 ? "claude-sonnet-5-5" : "claude-haiku-5-5");
            proc.adaptToTrack = adapt.getToggleState();
            close();
        };
        cancel.onClick = [this] { close(); };
        addAndMakeVisible (save); addAndMakeVisible (cancel);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.72f));
        drawPanel (g, panel().toFloat());
        auto r = panel().reduced (30, 24);
        drawTitle (g, "ADVANCED AI", (float) r.getX(), (float) r.getY(), 26);
        g.setFont (bc (17, 500)); g.setColour (juce::Colour (0xffcfc8bb));
        g.drawFittedText ("The offline brain answers common requests instantly, without internet. For free-form, creative requests "
                          "WMusic Rack can ask an online AI. The sound is always made inside the plugin: the AI only picks blocks and values.\n\n"
                          "Paste your Anthropic API key. It stays on this computer only and usage is billed to your account.",
                          r.withTrimmedTop (48).withHeight (120), juce::Justification::topLeft, 7);
        g.setFont (bc (13, 700)); g.setColour (col::red);
        g.drawText ("API KEY", keyBox.getBounds().translated (0, -18).withHeight (16), juce::Justification::left);
        g.drawText ("MODEL", model.getBounds().translated (0, -18).withHeight (16), juce::Justification::left);
        drawDisplay (g, keyBox.getBounds().toFloat());
    }
    void resized() override
    {
        auto r = panel().reduced (30, 24);
        r.removeFromTop (48 + 120 + 22);
        keyBox.setBounds (r.removeFromTop (40)); r.removeFromTop (28);
        model.setBounds (r.removeFromTop (34).withWidth (340)); r.removeFromTop (12);
        adapt.setBounds (r.removeFromTop (28)); r.removeFromTop (2);
        link.setBounds (r.removeFromTop (24));
        auto b = r.removeFromBottom (38);
        save.setBounds (b.removeFromRight (130)); b.removeFromRight (10);
        cancel.setBounds (b.removeFromRight (110));
    }
    void mouseDown (const juce::MouseEvent& e) override { if (! panel().contains (e.getPosition())) close(); }
    juce::TextEditor keyBox;
private:
    juce::Rectangle<int> panel() const { return getLocalBounds().withSizeKeepingCentre (640, 500); }
    WyattRackProcessor& proc;
    std::function<void()> close;
    juce::ComboBox model;
    juce::ToggleButton adapt;
    juce::HyperlinkButton link;
    KeyButton save { "SAVE", KeyKind::Red, KeyKind::Red, 16 }, cancel { "CLOSE", KeyKind::Dark, KeyKind::Dark, 16 };
};

class ExampleTile final : public juce::Button
{
public:
    ExampleTile (juce::String t, juce::String x) : juce::Button (x), tag (std::move (t)) {}
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        drawStripBase (g, r, true);
        if (over) { g.setColour (col::red.withAlpha (down ? 0.18f : 0.09f)); g.fillRoundedRectangle (r, 7); }
        auto tg = juce::Rectangle<float> (12, r.getCentreY() - 9, 58, 18);
        g.setColour (col::red); g.drawRoundedRectangle (tg, 3, 1);
        g.setFont (bc (12, 700)); g.drawText (tag, tg, juce::Justification::centred);
        g.setFont (bc (18, 500)); g.setColour (col::cream);
        g.drawFittedText (getButtonText(), juce::Rectangle<int> (80, 0, getWidth() - 88, getHeight()), juce::Justification::centredLeft, 1, 0.85f);
    }
    juce::String tag;
};
}

// ==================================================================== editor
WyattRackEditor::WyattRackEditor (WyattRackProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setSize (kDesignW, kDesignH);

    // ---- coluna da esquerda
    const char* modes[] = { "OFFLINE", "HYBRID", "ONLINE" };
    const char* tips[] = { "Offline brain only: instant, no internet", "Offline first; requests it does not understand go to the advanced AI", "Every request goes to the advanced AI (needs internet)" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeBtns[i];
        b.setButtonText (modes[i]); b.fontSize = 14; b.offKind = KeyKind::Dark; b.onKind = KeyKind::Red;
        b.setTooltip (tips[i]); b.setRadioGroupId (77); b.setClickingTogglesState (true);
        b.onClick = [this, i] { if (modeBtns[i].getToggleState()) proc.setAiMode ((AiMode) i); };
        content.addAndMakeVisible (b);
    }
    gearBtn.setTooltip ("Set up the advanced AI");
    gearBtn.onClick = [this] { openSettings(); };
    saveBtn.onClick = [this] { askSaveName(); };
    libBtn.onClick = [this] { showLibraryMenu(); };
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    for (auto* b : { &gearBtn, &saveBtn, &libBtn, &undoBtn, &redoBtn }) content.addAndMakeVisible (b);

    // ---- console do pedido (páginas Rack e Module)
    promptBox.setTextToShowWhenEmpty (u8 ("Descreva o som…  ex.: voz quente, na cara, com reverb de igreja"), col::dim);
    styleEditor (promptBox, 22);
    promptBox.setIndents (4, 9);
    promptBox.onReturnKey = [this] { submitPrompt (false); };
    promptBox.setText (proc.lastPrompt(), false);
    content.addAndMakeVisible (promptBox);
    createBtn.setTooltip ("Build the chain (Enter)");
    createBtn.onClick = [this] { submitPrompt (false); };
    chatBtn.setTooltip ("Open the AI conversation");
    chatBtn.onClick = [this] { showPage (Page::Start); };
    content.addAndMakeVisible (createBtn); content.addAndMakeVisible (chatBtn);
    for (auto* c : kChips)
    {
        auto b = std::make_unique<KeyButton> (u8 (c), KeyKind::Dark, KeyKind::Dark, 13.5f);
        const juce::String t = u8 (c);
        b->onClick = [this, t] { promptBox.setText (t, false); submitPrompt (false); };
        content.addAndMakeVisible (*b);
        chips.push_back (std::move (b));
    }

    // ---- página inicial (conversa)
    bigPrompt.setMultiLine (true, true);
    bigPrompt.setReturnKeyStartsNewLine (false);
    bigPrompt.setTextToShowWhenEmpty (u8 ("ex.: voz de adoração bem espacial, com delay 1/8 pontuado e um brilho em cima"), col::dim);
    styleEditor (bigPrompt, 27);
    bigPrompt.onReturnKey = [this] { submitPrompt (true); };
    content.addAndMakeVisible (bigPrompt);
    bigCreateBtn.onClick = [this] { submitPrompt (true); };
    rackBtn.onClick = [this] { showPage (Page::Rack); };
    content.addAndMakeVisible (bigCreateBtn); content.addAndMakeVisible (rackBtn);
    for (auto& ex : kExamples)
    {
        Example e { ex[0], u8 (ex[1]), std::make_unique<ExampleTile> (ex[0], u8 (ex[1])) };
        const juce::String t = e.text;
        e.btn->onClick = [this, t] { bigPrompt.setText (t, false); submitPrompt (true); };
        content.addAndMakeVisible (*e.btn);
        examples.push_back (std::move (e));
    }

    // ---- rack
    for (int s = 0; s < kNumSlots; ++s)
    {
        slots.push_back (std::make_unique<SlotStrip> (proc, *this, s));
        content.addAndMakeVisible (*slots.back());
    }
    modulePage = std::make_unique<ModulePage> (proc, *this);
    content.addChildComponent (*modulePage);

    // ---- rodapé
    for (auto* k : { &inKnob, &outKnob })
    {
        k->layout = Knob::Layout::Inline; k->knobSize = 40; k->nameSize = 14; k->valueSize = 15;
        k->format = [] (double v) { return (v > 0.05 ? "+" : "") + juce::String (v, 1) + " dB"; };
        k->slider.setDoubleClickReturnValue (true, 0.0);
        content.addAndMakeVisible (*k);
    }
    inKnob.name = "INPUT"; outKnob.name = "OUTPUT";
    inAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "entrada", inKnob.slider);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "saida", outKnob.slider);
    bypassBtn.setClickingTogglesState (true);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassBtn);
    content.addAndMakeVisible (bypassBtn);

    // ---- macros (cabeçalho do SIGNAL CHAIN)
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto& k = macroKnobs[m];
        k.layout = Knob::Layout::Inline; k.knobSize = 30; k.nameSize = 12.5f; k.valueSize = 14.5f;
        k.format = [] (double v)
        {
            const int d = juce::roundToInt (v) - 50;
            return d == 0 ? juce::String ("center") : ((d > 0 ? "+" : juce::String::fromUTF8 ("\xe2\x88\x92")) + juce::String (std::abs (d)));
        };
        k.slider.setDoubleClickReturnValue (true, 50.0);
        macroAtt[m] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, WyattRackProcessor::macroId (m), k.slider);
        content.addAndMakeVisible (k);
    }
    bakeBtn.setTooltip ("Write what the macros are doing into the rack and return them to center (the sound does not change)");
    bakeBtn.onClick = [this] { proc.bakeMacros(); };
    content.addAndMakeVisible (bakeBtn);

    layout();
    proc.addChangeListener (this);
    currentPage = proc.currentTreatment().empty() ? Page::Start : Page::Rack;
    showPage (currentPage);
    refreshAll();

    setResizable (true, true);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kDesignW / kDesignH);
    setResizeLimits (768, 492, 1920, 1230);
    setSize (1152, 738);
    startTimerHz (30);
}

WyattRackEditor::~WyattRackEditor()
{
    proc.removeChangeListener (this);
    modulePage->hideModule();
    setLookAndFeel (nullptr);
}

void WyattRackEditor::resized()
{
    scale = (float) getWidth() / (float) kDesignW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, kDesignW, kDesignH);
}

void WyattRackEditor::layout()
{
    for (int i = 0; i < 3; ++i) modeBtns[i].setBounds (34 + i * 76, 482, 72, 34);
    gearBtn.setBounds (178, 525, 80, 26);
    saveBtn.setBounds (34, 660, 108, 32); libBtn.setBounds (150, 660, 108, 32);
    undoBtn.setBounds (34, 702, 108, 32); redoBtn.setBounds (150, 702, 108, 32);

    promptBox.setBounds (320, 64, 776, 44);
    createBtn.setBounds (1120, 62, 130, 48);
    chatBtn.setBounds (1150, 24, 100, 26);
    int x = 308;
    for (auto& b : chips)
    {
        const int w = 22 + juce::GlyphArrangement::getStringWidthInt (bc (13.5f, 600), b->getButtonText());
        b->setBounds (x, 118, w, 26); x += w + 7;
    }

    bigPrompt.setBounds (330, 122, 750, 96);
    bigCreateBtn.setBounds (1100, 186, 130, 46);
    rackBtn.setBounds (1078, 30, 172, 36);
    for (int i = 0; i < (int) examples.size(); ++i)
        examples[(size_t) i].btn->setBounds (312 + (i % 3) * 312, 300 + (i / 3) * 54, 304, 46);

    for (int s = 0; s < kNumSlots; ++s) slots[(size_t) s]->setBounds (306, juce::roundToInt (240 + (float) s * 61.5f), 944, 60);
    modulePage->setBounds (290, 194, 976, 538);

    for (int m = 0; m < kNumMacros; ++m) macroKnobs[m].setBounds (540 + m * 140, 199, 136, 38);
    bakeBtn.setBounds (1156, 203, 80, 28);
    inKnob.setBounds (810, 752, 120, 44);
    outKnob.setBounds (940, 752, 120, 44);
    bypassBtn.setBounds (1150, 757, 100, 34);
}

void WyattRackEditor::showPage (Page pg)
{
    currentPage = pg;
    const bool start = pg == Page::Start, rack = pg == Page::Rack, mod = pg == Page::Module;
    for (juce::Component* c : { (juce::Component*) &promptBox, (juce::Component*) &createBtn, (juce::Component*) &chatBtn }) c->setVisible (! start);
    for (auto& b : chips) b->setVisible (! start);
    for (juce::Component* c : { (juce::Component*) &bigPrompt, (juce::Component*) &bigCreateBtn, (juce::Component*) &rackBtn }) c->setVisible (start);
    for (auto& e : examples) e.btn->setVisible (start);
    for (auto& s : slots) s->setVisible (rack);
    for (auto& k : macroKnobs) k.setVisible (rack);
    bakeBtn.setVisible (rack);
    modulePage->setVisible (mod);
    if (! mod) modulePage->hideModule();
    if (start) bigPrompt.grabKeyboardFocus();
    content.repaint();
}

void WyattRackEditor::openModule (int s)
{
    if (slotType (proc, s) <= 0) return;
    showPage (Page::Module);
    modulePage->show (s);
}

void WyattRackEditor::submitPrompt (bool fromStart)
{
    auto& box = fromStart ? bigPrompt : promptBox;
    const auto t = box.getText().trim();
    if (t.isEmpty() || proc.isThinking()) return;
    if (fromStart) promptBox.setText (t, false);
    goRackAfterAnswer = fromStart;
    logSizeAtSubmit = proc.log().size();
    proc.ask (t);
    refreshAll();
}

void WyattRackEditor::openSettings()
{
    juce::Component::SafePointer<WyattRackEditor> safe (this);
    settings = std::make_unique<SettingsPanel> (proc, [safe] { juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->closeSettings(); }); });
    content.addAndMakeVisible (*settings);
    settings->setBounds (0, 0, kDesignW, kDesignH);
}

void WyattRackEditor::closeSettings() { settings.reset(); refreshAll(); }

void WyattRackEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refreshAll(); }

void WyattRackEditor::refreshMacros()
{
    bool any = false, moved = false;
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto& k = macroKnobs[m];
        const bool used = proc.macroUsed (m);
        any |= used;
        if (used && std::abs (k.slider.getValue() - 50.0) > 0.05) moved = true;
        k.name = used ? proc.macroName (m) : "MACRO " + juce::String (m + 1);
        k.setEnabled (used);
        k.setAlpha (used ? 1.0f : 0.35f);
        k.slider.setTooltip (used ? proc.macroTip (m) : juce::String ("Free macro: ask for a sound and the rack names it and gives it a job"));
        k.repaint();
    }
    bakeBtn.setEnabled (any && moved);
}

void WyattRackEditor::refreshAll()
{
    for (auto& s : slots) s->refresh();
    refreshMacros();
    for (int i = 0; i < 3; ++i) modeBtns[i].setToggleState ((int) proc.aiMode() == i, juce::dontSendNotification);
    undoBtn.setEnabled (proc.canUndo()); redoBtn.setEnabled (proc.canRedo());
    const bool busy = proc.isThinking();
    createBtn.setEnabled (! busy); bigCreateBtn.setEnabled (! busy);
    createBtn.setButtonText (busy ? "..." : "CREATE"); bigCreateBtn.setButtonText (busy ? "..." : "CREATE");
    if (goRackAfterAnswer && ! busy && proc.log().size() > logSizeAtSubmit)
    {
        goRackAfterAnswer = false;
        if (! proc.currentTreatment().empty() && proc.log().back().ok) { bigPrompt.clear(); showPage (Page::Rack); }
    }
    if (currentPage == Page::Module) modulePage->refresh();
    content.repaint();
}

void WyattRackEditor::timerCallback()
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& st = *slots[(size_t) s];
        st.meter = std::max (proc.slotActivity (s), st.meter * 0.85f);
        st.refresh();
        if (st.isVisible()) st.repaint (st.getWidth() - 14, 0, 14, st.getHeight());
    }
    inMeter = std::max (proc.rack.inPeak.load(), inMeter * 0.85f);
    outMeter = std::max (proc.rack.outPeak.load(), outMeter * 0.85f);
    content.repaint (290, 742, 520, 64);
    {   // BAKE só acende com alguma macro fora do meio
        bool moved = false;
        for (int m = 0; m < kNumMacros; ++m) if (proc.macroUsed (m) && std::abs (macroKnobs[m].slider.getValue() - 50.0) > 0.05) moved = true;
        if (bakeBtn.isEnabled() != moved) bakeBtn.setEnabled (moved);
    }
    if (proc.isThinking()) { ++thinkingPhase; content.repaint (290, 140, 976, 40); content.repaint (312, 510, 932, 206); }
    undoBtn.setEnabled (proc.canUndo()); redoBtn.setEnabled (proc.canRedo());
}

void WyattRackEditor::contentClicked (juce::Point<int> p)
{
    if (juce::Rectangle<int> (34, 610, 224, 40).contains (p)) showPresetMenu();
    else if (currentPage != Page::Start && juce::Rectangle<int> (300, 146, 960, 26).contains (p)) showPage (Page::Start);
}

// ==================================================================== pintura
void WyattRackEditor::paintContent (juce::Graphics& g)
{
    const auto& a = Assets::get();
    fillMetal (g, { 0, 0, (float) kDesignW, (float) kDesignH });

    // coluna da esquerda
    drawPanel (g, { 14, 14, 262, 792 });
    if (a.logo.isValid())
    {
        const float lw = 186, lh = lw * (float) a.logo.getHeight() / (float) a.logo.getWidth();
        g.drawImage (a.logo, { 52, 34, lw, lh }, juce::RectanglePlacement::stretchToFit);
    }
    g.setFont (bc (46, 700)); g.setColour (col::cream);
    g.drawText ("RACK", 30, 350, 230, 54, juce::Justification::centred);
    g.setFont (bc (12.5f, 600)); g.setColour (juce::Colour (0xffa9a49c));
    g.drawText ("A I   E F F E C T S   R A C K", 30, 404, 230, 16, juce::Justification::centred);
    drawTitle (g, "AI ENGINE", 36, 444, 18);
    const bool key = proc.hasApiKey();
    drawLed (g, { 36, 531, 11, 11 }, key ? col::green : col::red, key || proc.aiMode() != AiMode::Offline);
    g.setFont (bc (14.5f, 500)); g.setColour (juce::Colour (0xffb9b3a8));
    g.drawText (key ? "Advanced AI ready" : (proc.aiMode() == AiMode::Offline ? "Offline brain" : "No AI key"), 54, 527, 120, 20, juce::Justification::left);
    drawTitle (g, "PRESET", 36, 574, 18);
    drawDisplay (g, { 34, 610, 224, 40 });
    g.setFont (bc (17, 500)); g.setColour (col::cream);
    g.drawFittedText (proc.presetModified ? juce::String ("Custom chain") : proc.getProgramName (proc.getCurrentProgram()), juce::Rectangle<int> (46, 610, 178, 40), juce::Justification::centredLeft, 1, 0.8f);
    { juce::Path t; t.addTriangle (232, 626, 244, 626, 238, 634); g.setColour (col::red); g.fillPath (t); }
    g.setFont (bc (12.5f, 600)); g.setColour (juce::Colour (0xffa9a49c));
    g.drawText ("W Y A T T   S O U N D S", 34, 766, 160, 16, juce::Justification::left);
    g.drawText ("v" + juce::String (JucePlugin_VersionString), 200, 766, 60, 16, juce::Justification::left);

    const auto& lg = proc.log();
    if (currentPage == Page::Start)
    {
        drawPanel (g, { 290, 14, 976, 718 });
        drawTitle (g, "WHAT SOUND DO YOU WANT?", 316, 26, 30);
        g.setFont (bc (16, 500)); g.setColour (juce::Colour (0xffa9a49c));
        g.drawText (u8 ("Describe it the way you would tell your engineer \xe2\x80\x94 style, instrument, an artist reference, or a quick fix."), 318, 72, 900, 22, juce::Justification::left);
        drawDisplay (g, { 312, 112, 932, 132 });
        g.setFont (bc (12.5f, 600)); g.setColour (juce::Colour (0xff8d877e));
        g.drawText (u8 ("W R I T E   I N   P O R T U G U E S E   \xc2\xb7   E N T E R   T O   C R E A T E"), 334, 222, 500, 16, juce::Justification::left);
        drawTitle (g, "TRY ONE", 316, 258, 18);
        drawTitle (g, "CONVERSATION", 316, 470, 18);
        drawDisplay (g, { 312, 510, 932, 206 });
        // conversa: últimas mensagens que cabem
        float y = 704;
        auto drawReply = [&] (const LogEntry& e, float yy)
        {
            g.setFont (bc (12, 700)); g.setColour (e.online ? col::online : col::red);
            g.drawText (e.online ? "ADVANCED AI" : "OFFLINE BRAIN", 328, (int) yy, 108, 20, juce::Justification::centredLeft);
            g.setFont (bc (16, 500)); g.setColour (juce::Colour (0xffd8d1c4));
            g.drawFittedText (e.reply, juce::Rectangle<int> (440, (int) yy, 790, 20), juce::Justification::centredLeft, 1, 0.85f);
        };
        auto drawYou = [&] (const juce::String& s, float yy)
        {
            g.setFont (bc (16, 500));
            const float w = juce::jmin (780.0f, (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) + 28);
            auto r = juce::Rectangle<float> (1230 - w, yy - 2, w, 26);
            g.setColour (juce::Colour (0xff2a1213)); g.fillRoundedRectangle (r, 13);
            g.setColour (juce::Colour (0xff5a1a1c)); g.drawRoundedRectangle (r, 13, 1);
            g.setColour (col::cream); g.drawFittedText (s, r.toNearestInt().reduced (12, 0), juce::Justification::centred, 1, 0.85f);
        };
        if (proc.isThinking())
        {
            y -= 30;
            juce::String dots; for (int i = 0; i < (thinkingPhase / 8) % 4; ++i) dots << ".";
            g.setFont (bc (12, 700)); g.setColour (col::online); g.drawText ("ADVANCED AI", 328, (int) y, 108, 20, juce::Justification::centredLeft);
            g.setFont (bc (16, 500)); g.setColour (col::online); g.drawText ("Thinking about your sound" + dots, 440, (int) y, 600, 20, juce::Justification::centredLeft);
            y -= 32; drawYou (proc.lastPrompt(), y);
        }
        for (int i = (int) lg.size() - 1; i >= 0 && y > 560; --i)
        {
            y -= 30; drawReply (lg[(size_t) i], y);
            y -= 32; if (y > 516) drawYou (lg[(size_t) i].prompt, y);
        }
        if (lg.empty() && ! proc.isThinking())
        {
            g.setFont (bc (17, 500)); g.setColour (juce::Colour (0xff6e6a64));
            g.drawText ("Your requests and the rack's answers show up here.", 312, 590, 932, 30, juce::Justification::centred);
        }
    }
    else
    {
        drawPanel (g, { 290, 14, 976, 168 });
        drawTitle (g, "DESCRIBE YOUR SOUND", 312, 22);
        drawDisplay (g, { 308, 62, 800, 48 });
        // resposta
        if (proc.isThinking())
        {
            juce::String dots; for (int i = 0; i < (thinkingPhase / 8) % 4; ++i) dots << ".";
            g.setFont (bc (12.5f, 700)); g.setColour (col::online); g.drawText ("ADVANCED AI", 310, 150, 110, 18, juce::Justification::left);
            g.setFont (bc (16, 500)); g.drawText ("Thinking about your sound" + dots, 420, 150, 600, 18, juce::Justification::left);
        }
        else if (! lg.empty())
        {
            const auto& e = lg.back();
            g.setFont (bc (12.5f, 700)); g.setColour (e.online ? col::online : col::red);
            g.drawText (e.online ? "ADVANCED AI" : "OFFLINE BRAIN", 310, 150, 110, 18, juce::Justification::left);
            g.setFont (bc (16, 500)); g.setColour (e.ok ? juce::Colour (0xffcfc8bb) : col::dim);
            g.drawFittedText (e.reply, juce::Rectangle<int> (420, 149, 836, 20), juce::Justification::centredLeft, 1, 0.8f);
        }
        else
        {
            g.setFont (bc (16, 500)); g.setColour (col::dim);
            g.drawText (u8 ("Write in Portuguese the sound you want and press CREATE. Click here to open the conversation."), 310, 149, 900, 20, juce::Justification::left);
        }
        if (currentPage == Page::Rack)
        {
            drawPanel (g, { 290, 194, 976, 538 });
            drawTitle (g, "SIGNAL CHAIN", 312, 200);
            g.setFont (bc (12.5f, 600)); g.setColour (juce::Colour (0xffa9a49c));
            g.drawText ("M A C R O S", 462, 210, 70, 16, juce::Justification::left);
        }
    }

    // rodapé
    drawPanel (g, { 290, 742, 976, 64 });
    g.setFont (bc (14, 600)); g.setColour (col::cream);
    g.drawText ("IN", 306, 752, 30, 18, juce::Justification::left);
    g.drawText ("OUT", 306, 775, 30, 18, juce::Justification::left);
    auto lvl = [] (float v) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -54.0f) + 54.0f) / 60.0f); };
    drawSegments (g, 340, 755, 36, 9, 12, lvl (inMeter), 28);
    drawSegments (g, 340, 778, 36, 9, 12, lvl (outMeter), 28);
}

// ==================================================================== presets e biblioteca
void WyattRackEditor::showPresetMenu()
{
    juce::PopupMenu m, mine;
    m.addSectionHeader ("FACTORY");
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (1000 + i, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    const auto files = proc.libraryFiles();
    for (int i = 0; i < files.size(); ++i) mine.addItem (2000 + i, files[i].getFileNameWithoutExtension());
    if (files.isEmpty()) mine.addItem (-1, "(nothing saved yet)", false);
    m.addSeparator();
    m.addSubMenu ("MY PRESETS", mine);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (content.localAreaToGlobal (juce::Rectangle<int> (34, 610, 224, 40))),
        [this, files, sp = juce::Component::SafePointer<WyattRackEditor> (this)] (int id)
        {
            if (! sp) return;
            if (id >= 1000 && id < 2000) { proc.setCurrentProgram (id - 1000); if (currentPage == Page::Start && ! proc.currentTreatment().empty()) showPage (Page::Rack); refreshAll(); }
            else if (id >= 2000 && id - 2000 < files.size()) { proc.loadFromFile (files[id - 2000]); if (currentPage == Page::Start) showPage (Page::Rack); }
        });
}

void WyattRackEditor::showLibraryMenu()
{
    juce::PopupMenu m;
    m.addItem (1, u8 ("Open preset file\xe2\x80\xa6"));
    m.addItem (2, "Show presets folder");
    m.addSeparator();
    m.addItem (3, "Copy chain as text");
    m.addItem (4, "Paste chain");
    m.addSeparator();
    m.addItem (5, "Clear rack");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&libBtn),
        [this, sp = juce::Component::SafePointer<WyattRackEditor> (this)] (int id)
        {
            if (! sp) return;
            if (id == 1)
            {
                chooser = std::make_unique<juce::FileChooser> ("Open preset", WyattRackProcessor::libraryFolder(), "*.wyrack;*.json");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile() && proc.loadFromFile (fc.getResult())) showPage (Page::Rack); });
            }
            else if (id == 2) { WyattRackProcessor::libraryFolder().createDirectory(); WyattRackProcessor::libraryFolder().revealToUser(); }
            else if (id == 3) juce::SystemClipboard::copyTextToClipboard (juce::JSON::toString (treatmentToVar (proc.currentTreatment())));
            else if (id == 4)
            {
                Treatment t; juce::String err;
                if (treatmentFromVar (juce::JSON::parse (juce::SystemClipboard::getTextFromClipboard()), t, err)) { proc.applyTreatment (t); showPage (Page::Rack); }
            }
            else if (id == 5) proc.applyTreatment ({});
        });
}

void WyattRackEditor::askSaveName()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name it to keep it in your library:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.lastPrompt().substring (0, 40));
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, sp = juce::Component::SafePointer<WyattRackEditor> (this)] (int r)
    {
        if (sp && r == 1) proc.saveToLibrary (w->getTextEditorContents ("name"));
    }), true);
}
} // namespace wr
