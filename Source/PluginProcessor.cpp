#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace wr
{
const std::vector<FactoryTreatment>& factoryTreatments()
{
    static const std::vector<FactoryTreatment> f {
        { "Rack vazio", "" },
        { "Voz Gospel de Igreja", "voz gospel com reverb de igreja" },
        { "Voz de Adoração", "voz de adoração bem espacial" },
        { "Voz Trap", "voz trap" },
        { "Voz Sertanejo", "voz sertanejo" },
        { "Voz Pop na Cara", "voz pop na cara e brilhante" },
        { "Voz R&B Aveludada", "voz r&b aveludada" },
        { "Pregação / Podcast", "pregação limpa" },
        { "Adlibs", "adlibs" },
        { "Backs Largos", "backs de adoração largos" },
        { "Violão Aberto", "violão aberto e espacial" },
        { "Bateria Suja", "bateria suja e larga" },
        { "Baixo Gordo", "baixo gordo" },
        { "Guitarra Rock", "guitarra rock" },
        { "Teclado Gospel", "teclado gospel com chorus" },
        { "Master Gospel", "master gospel com pressão" },
        { "Telefone", "telefone" },
        { "Rádio Antigo", "rádio antigo" },
        { "Debaixo d'Água", "debaixo d'água" },
        { "Fantasma", "efeito fantasma" },
        { "Anos 80", "anos 80 sonhador" },
        { "Lo-fi", "lo-fi" },
        { "Megafone", "megafone" },
    };
    return f;
}

static const char* kSettingsName = "WYATT RACK";

// ==================================================================== parâmetros
juce::String WyattRackProcessor::slotId (int slot, const char* what)
{
    return "s" + juce::String (slot + 1) + "_" + what;
}

juce::RangedAudioParameter* WyattRackProcessor::slotParam (int slot, const char* what) const
{
    return apvts.getParameter (slotId (slot, what));
}

juce::AudioProcessorValueTreeState::ParameterLayout WyattRackProcessor::createLayout (WyattRackProcessor* self)
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray typeNames;
    for (int t = 0; t < kNumTypes; ++t) typeNames.add (juce::String::fromUTF8 (info (t).name));

    for (int s = 0; s < kNumSlots; ++s)
    {
        const juce::String pre = "Slot " + juce::String (s + 1) + " ";
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("slot" + juce::String (s + 1), "Slot " + juce::String (s + 1), " | ");
        g->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId (s, "tipo"), 1 }, pre + "Bloco", typeNames, 0));
        g->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { slotId (s, "on"), 1 }, pre + "Liga", true));
        g->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotId (s, "mix"), 1 }, pre + "Mix",
                         juce::NormalisableRange<float> (0, 100, 0.1f), 100.0f,
                         juce::AudioParameterFloatAttributes().withLabel ("%")));
        for (int k = 0; k < kNumParams; ++k)
        {
            const char* key[] = { "p1", "p2", "p3", "p4", "p5", "p6" };
            auto attr = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [self, s, k] (float v, int) -> juce::String
                {
                    if (self == nullptr) return juce::String (v, 3);
                    auto* tp = self->apvts.getRawParameterValue (slotId (s, "tipo"));
                    const int t = tp ? (int) tp->load() : 0;
                    const auto& ps = info (t).params[(size_t) k];
                    if (! ps.used()) return juce::String::fromUTF8 ("—");
                    return juce::String::fromUTF8 (ps.format (ps.toReal (v)).c_str());
                });
            g->addChild (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotId (s, key[k]), 1 }, pre + "P" + juce::String (k + 1),
                             juce::NormalisableRange<float> (0, 1), 0.0f, attr));
        }
        l.add (std::move (g));
    }
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "entrada", 1 }, "Entrada", juce::NormalisableRange<float> (-24, 24, 0.1f), 0.0f,
                                                        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "saida", 1 }, juce::String::fromUTF8 ("Saída"), juce::NormalisableRange<float> (-24, 24, 0.1f), 0.0f,
                                                        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto attr = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [self, m] (float v, int) -> juce::String
            {
                const auto nm = self != nullptr ? self->macroName (m) : juce::String();
                return (nm.isNotEmpty() ? nm + " " : juce::String()) + juce::String (juce::roundToInt (v)) + "%";
            });
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { macroId (m), 1 }, "Macro " + juce::String (m + 1),
                                                            juce::NormalisableRange<float> (0, 100, 0.1f), 50.0f, attr));
    }
    return l;
}

WyattRackProcessor::WyattRackProcessor()
    : AudioProcessor (BusesProperties().withInput ("Entrada", juce::AudioChannelSet::stereo(), true)
                                       .withOutput (juce::String::fromUTF8 ("Saída"), juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WYATT_RACK", createLayout (this))
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& sp = rack.slots[(size_t) s].params;
        sp.type = apvts.getRawParameterValue (slotId (s, "tipo"));
        sp.on = apvts.getRawParameterValue (slotId (s, "on"));
        sp.mix = apvts.getRawParameterValue (slotId (s, "mix"));
        const char* key[] = { "p1", "p2", "p3", "p4", "p5", "p6" };
        for (int k = 0; k < kNumParams; ++k) sp.p[k] = apvts.getRawParameterValue (slotId (s, key[k]));
    }
    rack.inGainDb = inDb = apvts.getRawParameterValue ("entrada");
    rack.outGainDb = outDb = apvts.getRawParameterValue ("saida");
    rack.bypass = bypassP = apvts.getRawParameterValue ("bypass");
    for (int m = 0; m < kNumMacros; ++m) rack.macros.value[m] = macroP[m] = apvts.getRawParameterValue (macroId (m));

    if (auto s = openSettings()) mode = (AiMode) juce::jlimit (0, 2, s->getIntValue ("modoIA", (int) AiMode::Hibrido));
}

WyattRackProcessor::~WyattRackProcessor()
{
    alive->store (false);
    const juce::ScopedLock sl (workerLock);
    if (worker) worker->stopThread (30000);
}

// ==================================================================== áudio
bool WyattRackProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void WyattRackProcessor::prepareToPlay (double sr, int block)
{
    rack.prepare (sr, block);
    analyzer.prepare (sr);
    dbuf.setSize (2, juce::jmax (block, 64));
}

void WyattRackProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    for (int c = inCh; c < outCh; ++c) buf.copyFrom (c, 0, buf, 0, 0, n);   // mono → estéreo
    if (n == 0) return;

    float* L = buf.getWritePointer (0);
    float* R = outCh > 1 ? buf.getWritePointer (1) : nullptr;
    Ctx ctx;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) ctx.bpm = *bpm;

    analyzer.process (L, R, n);
    if (R != nullptr) rack.process (L, R, n, ctx);
    else
    {   // saída mono: processa em estéreo e soma
        if (dbuf.getNumSamples() < n) dbuf.setSize (2, n, false, false, true);
        dbuf.copyFrom (0, 0, L, n); dbuf.copyFrom (1, 0, L, n);
        rack.process (dbuf.getWritePointer (0), dbuf.getWritePointer (1), n, ctx);
        for (int i = 0; i < n; ++i) L[i] = 0.5f * (dbuf.getSample (0, i) + dbuf.getSample (1, i));
    }
}

void WyattRackProcessor::processBlock (juce::AudioBuffer<double>& buf, juce::MidiBuffer& m)
{
    juce::AudioBuffer<float> f (buf.getNumChannels(), buf.getNumSamples());
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int i = 0; i < buf.getNumSamples(); ++i) f.setSample (c, i, (float) buf.getSample (c, i));
    processBlock (f, m);
    for (int c = 0; c < buf.getNumChannels(); ++c)
        for (int i = 0; i < buf.getNumSamples(); ++i) buf.setSample (c, i, f.getSample (c, i));
}

// ==================================================================== tratamento
Treatment WyattRackProcessor::currentTreatment() const
{
    Treatment t;
    const char* key[] = { "p1", "p2", "p3", "p4", "p5", "p6" };
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int type = (int) apvts.getRawParameterValue (slotId (s, "tipo"))->load();
        if (type <= 0) continue;
        BlockState b; b.type = type;
        b.on = apvts.getRawParameterValue (slotId (s, "on"))->load() >= 0.5f;
        b.mix = apvts.getRawParameterValue (slotId (s, "mix"))->load();
        const auto& bi = info (type);
        for (int k = 0; k < kNumParams; ++k)
            b.p[(size_t) k] = bi.params[(size_t) k].used() ? bi.params[(size_t) k].toReal (apvts.getRawParameterValue (slotId (s, key[k]))->load()) : 0.0f;
        t.blocks.push_back (b);
    }
    // macros: alvos guardados por slot → posição na cadeia
    const auto occ = occupiedSlots();
    for (int m = 0; m < kNumMacros; ++m)
    {
        const auto& d = macroDefs[(size_t) m];
        auto& out = t.macros[(size_t) m];
        out.name = d.name;
        out.value = macroP[m] ? macroP[m]->load() / 100.0f : 0.5f;
        for (auto tg : d.targets)
        {
            const auto it = std::find (occ.begin(), occ.end(), tg.block);
            if (it == occ.end()) continue;
            tg.block = (int) (it - occ.begin());
            out.targets.push_back (tg);
        }
    }
    t.sanitizeMacros();
    return t;
}

std::vector<int> WyattRackProcessor::occupiedSlots() const
{
    std::vector<int> v;
    for (int s = 0; s < kNumSlots; ++s) if ((int) apvts.getRawParameterValue (slotId (s, "tipo"))->load() > 0) v.push_back (s);
    return v;
}

void WyattRackProcessor::syncMacroBus()
{
    rack.macros.clear();
    for (int m = 0; m < kNumMacros; ++m)
        for (auto& tg : macroDefs[(size_t) m].targets)
        {
            if (tg.block < 0 || tg.block >= kNumSlots) continue;
            const int k = tg.param == kMacroMix ? kNumParams : tg.param;
            if (k < 0 || k > kNumParams) continue;
            auto& a = rack.macros.depth[m][tg.block][k];
            a.store (a.load() + tg.depth);
        }
}

juce::String WyattRackProcessor::macroName (int m) const
{
    if (m < 0 || m >= kNumMacros) return {};
    return juce::String::fromUTF8 (macroDefs[(size_t) m].name.c_str());
}

juce::String WyattRackProcessor::macroTip (int m) const
{
    if (m < 0 || m >= kNumMacros || ! macroUsed (m)) return {};
    return macroName (m) + juce::String::fromUTF8 (" move: ") + juce::String::fromUTF8 (macroDescription (currentTreatment(), m).c_str())
         + juce::String::fromUTF8 (". No meio (50%) é o som como foi montado.");
}

void WyattRackProcessor::bakeMacros()
{
    auto t = currentTreatment();
    if (! t.hasMacros()) return;
    auto b = t.baked();
    b.macros = t.macros;                       // mantém os nomes e alvos...
    for (auto& m : b.macros) m.value = 0.5f;   // ...com os knobs de volta ao meio
    applyTreatment (b);
}

void WyattRackProcessor::applyTreatmentRemapped (Treatment t, const std::vector<int>& newIndexOf)
{
    t.remapMacros (newIndexOf);
    applyTreatment (t);
}

void WyattRackProcessor::setParamReal (juce::RangedAudioParameter* p, float normalized)
{
    if (p == nullptr) return;
    normalized = juce::jlimit (0.0f, 1.0f, normalized);
    if (std::abs (p->getValue() - normalized) < 1e-7f) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (normalized);
    p->endChangeGesture();
}

void WyattRackProcessor::pushUndo()
{
    undoStack.push_back (currentTreatment());
    if (undoStack.size() > 40) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void WyattRackProcessor::applyTreatment (const Treatment& tIn, bool undoable)
{
    if (undoable) pushUndo();
    Treatment t = tIn;
    if ((int) t.blocks.size() > kNumSlots) t.blocks.resize ((size_t) kNumSlots);
    t.sanitizeMacros();
    // Zera as macros antes de mexer nos blocos: nenhum bloco novo herda deslocamento de macro antiga.
    macroDefs = {};
    syncMacroBus();
    const char* key[] = { "p1", "p2", "p3", "p4", "p5", "p6" };
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto* tp = slotParam (s, "tipo");
        if (s >= (int) t.blocks.size())
        {
            setParamReal (tp, 0.0f);
            continue;
        }
        auto b = t.blocks[(size_t) s]; b.clampAll();
        const auto& bi = info (b.type);
        // valores primeiro, depois o tipo: o bloco novo já nasce com os valores certos
        for (int k = 0; k < kNumParams; ++k)
            setParamReal (slotParam (s, key[k]), bi.params[(size_t) k].used() ? bi.params[(size_t) k].toNorm (b.p[(size_t) k]) : 0.0f);
        setParamReal (slotParam (s, "mix"), b.mix / 100.0f);
        setParamReal (slotParam (s, "on"), b.on ? 1.0f : 0.0f);
        setParamReal (tp, tp->convertTo0to1 ((float) b.type));
    }
    // aplicado em ordem: bloco i = slot i
    macroDefs = t.macros;
    for (int m = 0; m < kNumMacros; ++m)
        if (auto* mp = apvts.getParameter (macroId (m))) setParamReal (mp, macroDefs[(size_t) m].used() ? t.macros[(size_t) m].value : 0.5f);
    syncMacroBus();
    sendChangeMessage();
}

void WyattRackProcessor::setSlotType (int slot, int type)
{
    auto t = currentTreatment();
    // posição real: slots vazios antes deste não existem no tratamento
    std::vector<int> slotOf;
    for (int s = 0; s < kNumSlots; ++s) if ((int) apvts.getRawParameterValue (slotId (s, "tipo"))->load() > 0) slotOf.push_back (s);
    int idx = 0; while (idx < (int) slotOf.size() && slotOf[(size_t) idx] < slot) ++idx;
    const bool exists = idx < (int) slotOf.size() && slotOf[(size_t) idx] == slot;
    std::vector<int> newIndexOf ((size_t) t.blocks.size());
    for (int i = 0; i < (int) newIndexOf.size(); ++i) newIndexOf[(size_t) i] = i;
    if (type <= 0)
    {
        if (! exists) return;
        t.blocks.erase (t.blocks.begin() + idx);
        for (int i = 0; i < (int) newIndexOf.size(); ++i) newIndexOf[(size_t) i] = i < idx ? i : (i == idx ? -1 : i - 1);
    }
    else
    {
        auto nb = BlockState::defaults ((BlockType) type);
        if (exists) { t.blocks[(size_t) idx] = nb; newIndexOf[(size_t) idx] = -1; }   // bloco trocado: as macros soltam dele
        else
        {
            if ((int) t.blocks.size() >= kNumSlots) return;
            t.blocks.insert (t.blocks.begin() + idx, nb);
            for (int i = 0; i < (int) newIndexOf.size(); ++i) newIndexOf[(size_t) i] = i < idx ? i : i + 1;
        }
    }
    applyTreatmentRemapped (t, newIndexOf);
}

void WyattRackProcessor::moveSlot (int from, int to)
{
    auto t = currentTreatment();
    if (from < 0 || to < 0 || from >= (int) t.blocks.size() || to >= (int) t.blocks.size() || from == to) return;
    std::vector<int> order ((size_t) t.blocks.size());
    for (int i = 0; i < (int) order.size(); ++i) order[(size_t) i] = i;
    order.erase (order.begin() + from);
    order.insert (order.begin() + to, from);
    std::vector<int> newIndexOf (order.size());
    for (int i = 0; i < (int) order.size(); ++i) newIndexOf[(size_t) order[(size_t) i]] = i;
    auto b = t.blocks[(size_t) from];
    t.blocks.erase (t.blocks.begin() + from);
    t.blocks.insert (t.blocks.begin() + to, b);
    applyTreatmentRemapped (t, newIndexOf);
}

void WyattRackProcessor::clearSlot (int slot) { setSlotType (slot, 0); }

void WyattRackProcessor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (currentTreatment());
    auto t = undoStack.back(); undoStack.pop_back();
    applyTreatment (t, false);
}

void WyattRackProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (currentTreatment());
    auto t = redoStack.back(); redoStack.pop_back();
    applyTreatment (t, false);
}

// ==================================================================== programas (presets de fábrica)
int WyattRackProcessor::getNumPrograms() { return (int) factoryTreatments().size(); }
const juce::String WyattRackProcessor::getProgramName (int i)
{
    const auto& f = factoryTreatments();
    return i >= 0 && i < (int) f.size() ? juce::String::fromUTF8 (f[(size_t) i].name) : juce::String();
}
void WyattRackProcessor::setCurrentProgram (int i)
{
    const auto& f = factoryTreatments();
    if (i < 0 || i >= (int) f.size()) return;
    currentProgram = i;
    const std::string prompt = f[(size_t) i].prompt;
    Treatment t;
    if (! prompt.empty()) { t = brain.interpret (prompt, {}).treatment; autoMacros (t, prompt); }
    macroContext = juce::String::fromUTF8 (prompt.c_str());
    applyTreatment (t);
    LogEntry e;
    e.prompt = juce::String::fromUTF8 (f[(size_t) i].name);
    e.reply = juce::String::fromUTF8 ("Preset de fábrica \"") + e.prompt + "\": "
            + (t.empty() ? juce::String ("rack vazio.") : juce::String::fromUTF8 (t.chainText().c_str()) + ".");
    addLog (e);
    sendChangeMessage();
}

// ==================================================================== assistente
std::unique_ptr<juce::PropertiesFile> WyattRackProcessor::openSettings() const
{
    juce::PropertiesFile::Options o;
    o.applicationName = kSettingsName;
    o.filenameSuffix = ".settings";
    o.folderName = "WYATT Sounds";
    o.osxLibrarySubFolder = "Application Support";
    o.commonToAllUsers = false;
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    return std::make_unique<juce::PropertiesFile> (o);
}

// A chave fica só neste computador, misturada (não é criptografia forte, mas não aparece em texto puro).
static juce::String scramble (const juce::String& s, bool enc)
{
    const juce::String salt = "WYATT-RACK-SOLI-DEO-GLORIA";
    if (enc)
    {
        juce::MemoryBlock mb (s.toRawUTF8(), s.getNumBytesAsUTF8());
        for (size_t i = 0; i < mb.getSize(); ++i) mb[i] = (char) (mb[i] ^ salt[(int) (i % (size_t) salt.length())]);
        return mb.toBase64Encoding();
    }
    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (s)) return {};
    for (size_t i = 0; i < mb.getSize(); ++i) mb[i] = (char) (mb[i] ^ salt[(int) (i % (size_t) salt.length())]);
    return juce::String::fromUTF8 ((const char*) mb.getData(), (int) mb.getSize());
}

juce::String WyattRackProcessor::apiKey() const
{
    if (useCloudOverride) return cloudOverride.apiKey;
    auto s = openSettings();
    return s ? scramble (s->getValue ("chaveIA"), false) : juce::String();
}
juce::String WyattRackProcessor::aiModel() const
{
    if (useCloudOverride) return cloudOverride.model;
    auto s = openSettings();
    return s ? s->getValue ("modeloIA", "claude-haiku-5-5") : juce::String ("claude-haiku-5-5");
}
bool WyattRackProcessor::hasApiKey() const { return apiKey().trim().isNotEmpty(); }

void WyattRackProcessor::setApiKey (const juce::String& key, const juce::String& model)
{
    if (auto s = openSettings())
    {
        s->setValue ("chaveIA", scramble (key.trim(), true));
        s->setValue ("modeloIA", model.isEmpty() ? juce::String ("claude-haiku-5-5") : model);
        s->saveIfNeeded();
    }
    sendChangeMessage();
}

void WyattRackProcessor::setAiMode (AiMode m)
{
    mode = m;
    if (auto s = openSettings()) { s->setValue ("modoIA", (int) m); s->saveIfNeeded(); }
    sendChangeMessage();
}

CloudConfig WyattRackProcessor::cloudConfig() const
{
    if (useCloudOverride) return cloudOverride;
    CloudConfig c; c.apiKey = apiKey(); c.model = aiModel();
    return c;
}

void WyattRackProcessor::addLog (LogEntry e)
{
    chatLog.push_back (std::move (e));
    if (chatLog.size() > 30) chatLog.erase (chatLog.begin());
}

namespace
{
class AskThread final : public juce::Thread
{
public:
    explicit AskThread (std::function<void()> f) : juce::Thread ("WYATT RACK IA"), fn (std::move (f)) {}
    void run() override { fn(); }
private:
    std::function<void()> fn;
};
}

void WyattRackProcessor::ask (const juce::String& promptIn, bool forceOnline)
{
    const auto prompt = promptIn.trim();
    if (prompt.isEmpty() || thinking.load()) return;
    lastPromptText = prompt;
    // O cérebro trabalha em cima do som que está tocando: o efeito das macros vira valor fixo.
    const Treatment before = currentTreatment().baked();
    const BrainResult local = brain.interpret (prompt.toStdString(), before);

    const bool keyOk = hasApiKey();
    const bool localStrong = local.recognized() && local.confidence >= 0.75f;
    bool goOnline = false;
    if (keyOk)
    {
        if (forceOnline || mode == AiMode::Online) goOnline = true;
        else if (mode == AiMode::Hibrido && ! localStrong) goOnline = true;
    }

    if (! goOnline)
    {
        finishAnswer (prompt, local, nullptr, before);
        return;
    }

    thinking.store (true);
    sendChangeMessage();
    const auto cfg = cloudConfig();
    const auto profile = analyzer.snapshot();
    auto token = alive;
    auto job = [this, token, cfg, prompt, before, profile, local]
    {
        auto cloud = CloudBrain::ask (cfg, prompt, before, profile);
        juce::MessageManager::callAsync ([this, token, cloud, prompt, before, local]
        {
            if (! token->load()) return;
            thinking.store (false);
            finishAnswer (prompt, local, &cloud, before);
        });
    };
    const juce::ScopedLock sl (workerLock);
    if (worker) worker->stopThread (100);
    worker = std::make_unique<AskThread> (job);
    worker->startThread();
}

void WyattRackProcessor::finishAnswer (const juce::String& prompt, const BrainResult& local, const CloudResult* cloud, const Treatment& before)
{
    LogEntry e; e.prompt = prompt;
    if (cloud != nullptr && cloud->ok)
    {
        e.online = true;
        if (cloud->reset) { applyTreatment ({}); e.reply = cloud->explanation.isNotEmpty() ? cloud->explanation : "Zerei o rack."; }
        else
        {
            Treatment t = cloud->treatment;
            t.sanitizeMacros();
            macroContext = prompt;
            if (! t.hasMacros()) autoMacros (t, prompt.toStdString());
            applyTreatment (t);
            e.reply = cloud->explanation;
            if (e.reply.isEmpty()) e.reply = "Montei: " + juce::String::fromUTF8 (cloud->treatment.chainText().c_str()) + ".";
        }
        addLog (e); sendChangeMessage();
        return;
    }

    // offline (direto ou porque a IA online falhou)
    juce::String prefix;
    if (cloud != nullptr) prefix = cloud->error + " ";
    if (local.reset)
    {
        applyTreatment ({});
        e.reply = prefix + juce::String::fromUTF8 (local.explanation.c_str());
    }
    else if (! local.recognized() || (local.treatment.blocks.empty() && ! local.refinement))
    {
        e.ok = false;
        e.reply = prefix + juce::String::fromUTF8 (local.explanation.c_str());
        if (cloud == nullptr && ! hasApiKey() && mode != AiMode::Offline)
            e.reply += juce::String::fromUTF8 (" Para pedidos livres assim, ligue a IA avançada no ⚙.");
    }
    else
    {
        Treatment t = local.treatment;
        juce::String adapted;
        const auto profile = analyzer.snapshot();
        if (adaptToTrack && ! local.refinement)
            adapted = juce::String::fromUTF8 (adaptToProfile (t, profile).c_str());
        // ajuste ("mais reverb") soma as palavras ao pedido original, para as macros não trocarem de nome à toa
        macroContext = local.refinement ? (macroContext + " " + prompt).trim() : prompt;
        autoMacros (t, macroContext.toStdString());
        applyTreatment (t);
        e.reply = prefix + juce::String::fromUTF8 (local.explanation.c_str());
        if (adapted.isNotEmpty()) e.reply += " " + adapted;
        juce::ignoreUnused (before);
    }
    addLog (e);
    sendChangeMessage();
}

void WyattRackProcessor::waitForAssistant (int ms)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (thinking.load() && juce::Time::getMillisecondCounter() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
}

// ==================================================================== biblioteca
juce::File WyattRackProcessor::libraryFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("WYATT Sounds").getChildFile ("WYATT RACK").getChildFile ("Tratamentos");
}

bool WyattRackProcessor::saveToLibrary (const juce::String& nameIn)
{
    const auto name = juce::File::createLegalFileName (nameIn.trim());
    if (name.isEmpty()) return false;
    auto v = treatmentToVar (currentTreatment());
    if (auto* o = v.getDynamicObject())
    {
        o->setProperty ("nome", nameIn.trim());
        o->setProperty ("pedido", lastPromptText);
        o->setProperty ("app", "WYATT RACK");
        o->setProperty ("versao", 1);
    }
    auto dir = libraryFolder();
    dir.createDirectory();
    return dir.getChildFile (name + ".wyrack").replaceWithText (juce::JSON::toString (v));
}

bool WyattRackProcessor::loadFromFile (const juce::File& f)
{
    Treatment t; juce::String err;
    const auto json = juce::JSON::parse (f.loadFileAsString());
    if (! treatmentFromVar (json, t, err)) return false;
    if (! t.hasMacros()) autoMacros (t, (json.getProperty ("pedido", "").toString() + " " + f.getFileNameWithoutExtension()).toStdString());
    macroContext = json.getProperty ("pedido", "").toString();
    applyTreatment (t);
    LogEntry e; e.prompt = "Abrir: " + f.getFileNameWithoutExtension();
    e.reply = "Carreguei: " + juce::String::fromUTF8 (t.chainText().c_str()) + ".";
    addLog (e); sendChangeMessage();
    return true;
}

juce::Array<juce::File> WyattRackProcessor::libraryFiles() const
{
    auto files = libraryFolder().findChildFiles (juce::File::findFiles, false, "*.wyrack");
    files.sort();
    return files;
}

// ==================================================================== estado
void WyattRackProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto extra = state.getOrCreateChildWithName ("WYATT", nullptr);
    extra.setProperty ("ultimoPedido", lastPromptText, nullptr);
    extra.setProperty ("contextoMacros", macroContext, nullptr);
    extra.setProperty ("macros", juce::JSON::toString (treatmentToVar (currentTreatment()).getProperty ("macros", juce::var()), true), nullptr);
    extra.removeAllChildren (nullptr);
    for (auto& e : chatLog)
    {
        juce::ValueTree c ("MSG");
        c.setProperty ("p", e.prompt, nullptr); c.setProperty ("r", e.reply, nullptr);
        c.setProperty ("online", e.online, nullptr); c.setProperty ("ok", e.ok, nullptr);
        extra.appendChild (c, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void WyattRackProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    auto extra = state.getChildWithName ("WYATT");
    chatLog.clear();
    juce::var savedMacros; bool hasSavedMacros = false;
    if (extra.isValid())
    {
        lastPromptText = extra.getProperty ("ultimoPedido").toString();
        macroContext = extra.getProperty ("contextoMacros", lastPromptText).toString();
        if (extra.hasProperty ("macros")) { savedMacros = juce::JSON::parse (extra.getProperty ("macros").toString()); hasSavedMacros = true; }
        for (auto c : extra)
            chatLog.push_back ({ c.getProperty ("p").toString(), c.getProperty ("r").toString(), (bool) c.getProperty ("online"), (bool) c.getProperty ("ok", true) });
        state.removeChild (extra, nullptr);
    }
    apvts.replaceState (state);
    // O JUCE pula parâmetros cujo valor guardado em cache já parece igual; um liga/desliga que o host deixou em 0,58
    // ficaria assim. Aqui cada parâmetro recebe explicitamente o valor salvo.
    for (auto child : state)
    {
        if (! child.hasType ("PARAM")) continue;
        if (auto* prm = apvts.getParameter (child.getProperty ("id").toString()))
        {
            const float n = prm->convertTo0to1 ((float) (double) child.getProperty ("value"));
            if (std::abs (prm->getValue() - n) > 1e-6f) prm->setValueNotifyingHost (n);
        }
    }
    // macros: alvos salvos (posição na cadeia) → slots; sessões antigas ganham macros automáticas
    {
        macroDefs = {};
        syncMacroBus();
        Treatment t = currentTreatment();
        if (hasSavedMacros) macrosFromVar (savedMacros, t);
        else if (! t.empty()) autoMacros (t, macroContext.toStdString());
        t.sanitizeMacros();
        const auto occ = occupiedSlots();
        for (int m = 0; m < kNumMacros; ++m)
        {
            auto d = t.macros[(size_t) m];
            for (auto& tg : d.targets) tg.block = occ[(size_t) tg.block];
            macroDefs[(size_t) m] = d;
            // o valor do knob é o do parâmetro salvo; sessão antiga sem o parâmetro fica no meio
            if (! hasSavedMacros)
                if (auto* mp = apvts.getParameter (macroId (m))) mp->setValueNotifyingHost (0.5f);
        }
        syncMacroBus();
    }
    undoStack.clear(); redoStack.clear();
    sendChangeMessage();
}

juce::AudioProcessorEditor* WyattRackProcessor::createEditor() { return new WyattRackEditor (*this); }
} // namespace wr

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new wr::WyattRackProcessor(); }
