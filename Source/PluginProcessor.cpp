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
    return t;
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

void WyattRackProcessor::applyTreatment (const Treatment& t, bool undoable)
{
    if (undoable) pushUndo();
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
    if (type <= 0) { if (exists) t.blocks.erase (t.blocks.begin() + idx); }
    else
    {
        auto nb = BlockState::defaults ((BlockType) type);
        if (exists) t.blocks[(size_t) idx] = nb;
        else t.blocks.insert (t.blocks.begin() + idx, nb);
    }
    applyTreatment (t);
}

void WyattRackProcessor::moveSlot (int from, int to)
{
    auto t = currentTreatment();
    if (from < 0 || to < 0 || from >= (int) t.blocks.size() || to >= (int) t.blocks.size() || from == to) return;
    auto b = t.blocks[(size_t) from];
    t.blocks.erase (t.blocks.begin() + from);
    t.blocks.insert (t.blocks.begin() + to, b);
    applyTreatment (t);
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
    if (! prompt.empty()) t = brain.interpret (prompt, {}).treatment;
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
    const Treatment before = currentTreatment();
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
            applyTreatment (cloud->treatment);
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
    if (! treatmentFromVar (juce::JSON::parse (f.loadFileAsString()), t, err)) return false;
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
    if (extra.isValid())
    {
        lastPromptText = extra.getProperty ("ultimoPedido").toString();
        for (auto c : extra)
            chatLog.push_back ({ c.getProperty ("p").toString(), c.getProperty ("r").toString(), (bool) c.getProperty ("online"), (bool) c.getProperty ("ok", true) });
        state.removeChild (extra, nullptr);
    }
    apvts.replaceState (state);
    undoStack.clear(); redoStack.clear();
    sendChangeMessage();
}

juce::AudioProcessorEditor* WyattRackProcessor::createEditor() { return new WyattRackEditor (*this); }
} // namespace wr

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new wr::WyattRackProcessor(); }
