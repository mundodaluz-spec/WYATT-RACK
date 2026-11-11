// WYATT RACK — testes de integração do plugin (processador, estado, IA híbrida com servidor falso, tela)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

using namespace wr;
static int passed = 0, failed = 0;
#define CHECK(cond, ...) do { if (cond) { ++passed; } else { ++failed; std::printf ("  FALHOU: " __VA_ARGS__); std::printf ("   [linha %d]\n", __LINE__); } } while (0)

static juce::File outDir;

// ------------------------------------------------------------------ servidor HTTP falso (imita a API)
class FakeApi final : public juce::Thread
{
public:
    FakeApi() : juce::Thread ("api falsa") { sock.createListener (0, "127.0.0.1"); port = sock.getBoundPort(); startThread(); }
    ~FakeApi() override { signalThreadShouldExit(); sock.close(); stopThread (2000); }
    void run() override
    {
        while (! threadShouldExit())
        {
            std::unique_ptr<juce::StreamingSocket> c (sock.waitForNextConnection());
            if (! c) continue;
            juce::MemoryBlock data; char buf[8192];
            int contentLength = -1, headerEnd = -1;
            for (int tries = 0; tries < 400; ++tries)
            {
                if (c->waitUntilReady (true, 50) != 1) { if (headerEnd >= 0 && contentLength >= 0 && (int) data.getSize() >= headerEnd + contentLength) break; continue; }
                const int n = c->read (buf, sizeof buf, false);
                if (n <= 0) break;
                data.append (buf, (size_t) n);
                const auto s = data.toString();
                headerEnd = s.indexOf ("\r\n\r\n");
                if (headerEnd >= 0)
                {
                    headerEnd += 4;
                    const auto cl = s.fromFirstOccurrenceOf ("content-length:", false, true).upToFirstOccurrenceOf ("\r\n", false, false).trim();
                    contentLength = cl.getIntValue();
                    if ((int) data.getSize() >= headerEnd + contentLength) break;
                }
            }
            {
                const juce::ScopedLock sl (lock);
                lastRequest = juce::String::fromUTF8 ((const char*) data.getData(), (int) data.getSize());
                ++requests;
            }
            juce::String body, statusLine;
            { const juce::ScopedLock sl (lock); body = replyBody; statusLine = replyStatus; }
            juce::String resp;
            resp << "HTTP/1.1 " << statusLine << "\r\ncontent-type: application/json\r\ncontent-length: " << (int) body.getNumBytesAsUTF8() << "\r\nconnection: close\r\n\r\n" << body;
            c->write (resp.toRawUTF8(), (int) resp.getNumBytesAsUTF8());
            c->close();
        }
    }
    void reply (const juce::String& status, const juce::String& body) { const juce::ScopedLock sl (lock); replyStatus = status; replyBody = body; }
    juce::String request() { const juce::ScopedLock sl (lock); return lastRequest; }
    int count() { const juce::ScopedLock sl (lock); return requests; }
    int port = 0;
private:
    juce::StreamingSocket sock;
    juce::CriticalSection lock;
    juce::String lastRequest, replyBody, replyStatus { "200 OK" };
    int requests = 0;
};

static juce::String apiMessage (const juce::String& jsonText)
{
    auto* tb = new juce::DynamicObject(); tb->setProperty ("type", "text"); tb->setProperty ("text", jsonText);
    auto* m = new juce::DynamicObject();
    m->setProperty ("id", "msg_teste"); m->setProperty ("type", "message"); m->setProperty ("role", "assistant");
    m->setProperty ("content", juce::Array<juce::var> { juce::var (tb) }); m->setProperty ("stop_reason", "end_turn");
    return juce::JSON::toString (juce::var (m));
}

// ------------------------------------------------------------------ utilidades
static void processSeconds (WyattRackProcessor& p, double secs, float amp = 0.25f, juce::AudioBuffer<float>* keep = nullptr)
{
    const int block = 512; const double sr = 48000;
    juce::MidiBuffer midi; juce::Random rnd (7);
    juce::AudioBuffer<float> b (2, block);
    int done = 0; const int total = (int) (secs * sr);
    if (keep) keep->setSize (2, total);
    while (done < total)
    {
        const int n = std::min (block, total - done);
        b.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const double t = (done + i) / sr;
            const float v = amp * (float) (std::sin (2 * juce::MathConstants<double>::pi * 220 * t) * 0.7 + (rnd.nextFloat() * 2 - 1) * 0.3 * std::exp (-6 * std::fmod (t, 0.5)));
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        p.processBlock (b, midi);
        if (keep) for (int c = 0; c < 2; ++c) keep->copyFrom (c, done, b, c, 0, n);
        done += n;
    }
}

static bool finiteBuf (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

static void snap (juce::Component& c, const juce::String& name)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png; juce::FileOutputStream os (outDir.getChildFile (name + ".png"));
    if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
}

// ------------------------------------------------------------------ testes
static void testProcessor()
{
    std::printf ("== Processador\n");
    WyattRackProcessor p; p.setAiMode (AiMode::Offline);
    p.prepareToPlay (48000, 512);
    {   // vazio = idêntico
        juce::AudioBuffer<float> b (2, 512), ref; juce::MidiBuffer m;
        for (int i = 0; i < 512; ++i) { b.setSample (0, i, std::sin (i * 0.05f) * 0.3f); b.setSample (1, i, std::cos (i * 0.03f) * 0.3f); }
        ref.makeCopyOf (b);
        p.processBlock (b, m);
        bool same = true; for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) if (b.getSample (c, i) != ref.getSample (c, i)) same = false;
        CHECK (same, "rack vazio deveria ser transparente\n");
    }
    p.ask ("voz gospel com reverb de igreja");
    auto t = p.currentTreatment();
    CHECK (t.blocks.size() >= 4 && t.find (BlockType::Reverb), "pedido offline montou %d blocos\n", (int) t.blocks.size());
    CHECK (! p.log().empty() && ! p.log().back().online && p.log().back().ok, "registro da resposta offline\n");
    CHECK ((int) p.apvts.getRawParameterValue ("s1_tipo")->load() == t.blocks[0].type, "slot 1 recebeu o primeiro bloco\n");

    {   // os valores na tela/DAW batem com o tratamento do cérebro
        LocalBrain br; auto r = br.interpret ("voz gospel com reverb de igreja", {});
        bool eq = r.treatment.blocks.size() == t.blocks.size();
        for (size_t i = 0; eq && i < t.blocks.size(); ++i)
            for (int k = 0; k < kNumParams; ++k)
            {
                const auto& ps = info (t.blocks[i].type).params[(size_t) k];
                if (! ps.used()) continue;
                const float a = r.treatment.blocks[i].p[(size_t) k], b = t.blocks[i].p[(size_t) k];
                if (std::abs (a - b) > 0.01f * std::max (1.0f, std::abs (a))) { eq = false; std::printf ("  %s.%s: %f vs %f\n", info (t.blocks[i].type).key, ps.key, a, b); }
            }
        CHECK (eq, "tratamento aplicado difere do cérebro\n");
    }
    // texto do parâmetro na DAW usa a unidade do bloco
    {
        auto* prm = p.apvts.getParameter ("s1_p1");
        const auto txt = prm->getCurrentValueAsText();
        CHECK (txt.containsAnyOf ("0123456789"), "texto do parâmetro na DAW: '%s'\n", txt.toRawUTF8());
    }
    juce::AudioBuffer<float> out;
    processSeconds (p, 3, 0.25f, &out);
    CHECK (finiteBuf (out) && out.getMagnitude (0, out.getNumSamples()) < 4, "áudio estável com tratamento\n");

    // refinamento + desfazer/refazer
    const auto before = p.currentTreatment();
    p.ask ("sem reverb");
    CHECK (! p.currentTreatment().find (BlockType::Reverb), "sem reverb\n");
    p.undo();
    CHECK (p.currentTreatment() == before, "desfazer voltou ao anterior\n");
    p.redo();
    CHECK (! p.currentTreatment().find (BlockType::Reverb), "refazer\n");

    // mover e tirar slots
    {
        auto t0 = p.currentTreatment();
        p.moveSlot (0, 1);
        auto t1 = p.currentTreatment();
        CHECK (t1.blocks[0].type == t0.blocks[1].type && t1.blocks[1].type == t0.blocks[0].type, "mover bloco\n");
        p.clearSlot (0);
        CHECK (p.currentTreatment().blocks.size() == t0.blocks.size() - 1, "tirar bloco\n");
        p.setSlotType (7, (int) BlockType::Tremolo);
        CHECK (p.currentTreatment().blocks.back().type == (int) BlockType::Tremolo, "adicionar bloco no fim\n");
    }

    // estado salvo/reaberto
    {
        p.ask ("voz trap");
        juce::MemoryBlock mb; p.getStateInformation (mb);
        WyattRackProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (q.currentTreatment() == p.currentTreatment(), "projeto reaberto com o mesmo tratamento\n");
        CHECK (q.log().size() == p.log().size() && q.lastPrompt() == "voz trap", "histórico reaberto\n");
    }

    // programas de fábrica
    {
        int ok = 0;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            juce::AudioBuffer<float> o; processSeconds (p, 0.5, 0.25f, &o);
            const bool good = finiteBuf (o) && (i == 0 ? p.currentTreatment().empty() : ! p.currentTreatment().empty());
            if (good) ++ok; else std::printf ("  programa ruim: %s\n", p.getProgramName (i).toRawUTF8());
        }
        CHECK (ok == p.getNumPrograms(), "%d de %d programas de fábrica OK\n", ok, p.getNumPrograms());
    }

    // biblioteca
    {
        p.ask ("anos 80 sonhador");
        const auto t0 = p.currentTreatment();
        CHECK (p.saveToLibrary ("Teste Automatico"), "salvar na biblioteca\n");
        const auto f = WyattRackProcessor::libraryFolder().getChildFile ("Teste Automatico.wyrack");
        p.applyTreatment ({});
        CHECK (p.loadFromFile (f) && p.currentTreatment() == t0, "abrir da biblioteca\n");
        f.deleteFile();
    }

    // mono → mono
    {
        WyattRackProcessor m;
        juce::AudioProcessor::BusesLayout l; l.inputBuses.add (juce::AudioChannelSet::mono()); l.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (m.setBusesLayout (l), "layout mono aceito\n");
        m.prepareToPlay (48000, 256); m.setCurrentProgram (1);
        juce::AudioBuffer<float> b (1, 256); juce::MidiBuffer mid;
        for (int i = 0; i < 256; ++i) b.setSample (0, i, 0.2f * std::sin (i * 0.1f));
        for (int k = 0; k < 50; ++k) m.processBlock (b, mid);
        CHECK (finiteBuf (b), "mono estável\n");
    }

    // CPU com o tratamento mais pesado
    {
        p.ask ("efeito fantasma bem extremo com chorus e phaser");
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        processSeconds (p, 10);
        const double pct = (juce::Time::getMillisecondCounterHiRes() - t0) / 10000.0 * 100;
        std::printf ("  CPU (plugin, %d blocos): %.1f%% de um núcleo\n", (int) p.currentTreatment().blocks.size(), pct);
        CHECK (pct < 25, "CPU alta\n");
    }
}

static void testAnalysis()
{
    std::printf ("== Análise da faixa\n");
    TrackAnalyzer a; a.prepare (48000);
    std::vector<float> s (48000 * 3);
    for (size_t i = 0; i < s.size(); ++i) s[i] = 0.0447f * std::sin ((float) i * 2 * juce::MathConstants<float>::pi * 1000 / 48000);  // −27 dBFS pico, −30 RMS
    for (size_t off = 0; off < s.size(); off += 512) a.process (s.data() + off, s.data() + off, (int) std::min<size_t> (512, s.size() - off));
    auto pr = a.snapshot();
    CHECK (pr.valid && std::abs (pr.rmsDb - (-30)) < 1.5f, "nível médio medido %.1f dB (esperado −30)\n", pr.rmsDb);
    CHECK (std::abs (pr.peakDb - (-27)) < 1.0f, "pico medido %.1f dB (esperado −27)\n", pr.peakDb);
    CHECK (pr.bandDb[2] > pr.bandDb[0] + 15 && pr.bandDb[2] > pr.bandDb[4] + 15, "1 kHz cai nos médios (%.0f / %.0f / %.0f)\n", pr.bandDb[0], pr.bandDb[2], pr.bandDb[4]);

    LocalBrain br; auto t = br.interpret ("voz pop", {}).treatment;
    const float thr0 = t.find (BlockType::Compressor)->p[0];
    const auto note = adaptToProfile (t, pr);
    CHECK (std::abs (t.find (BlockType::Compressor)->p[0] - (thr0 - 12)) < 0.6f, "limiar do compressor acompanhou a faixa (−12 dB): %s\n", note.c_str());

    TrackProfile none;
    auto t2 = br.interpret ("voz pop", {}).treatment;
    CHECK (adaptToProfile (t2, none).empty() && t2 == br.interpret ("voz pop", {}).treatment, "sem análise, nada muda\n");
}

static void testCloudParsing()
{
    std::printf ("== Leitura das respostas da IA online\n");
    {
        auto r = CloudBrain::parseResponse (200, apiMessage (R"({"explicacao":"Reverb de caverna.","zerar":false,"blocos":[
            {"tipo":"reverb","ligado":true,"mix":40,"params":[{"nome":"tipo","valor":3},{"nome":"decaimento","valor":7.5}]},
            {"tipo":"filtro","ligado":true,"mix":100,"params":[{"nome":"frequencia","valor":900}]}]})"));
        CHECK (r.ok && r.treatment.blocks.size() == 2 && (int) r.treatment.blocks[0].p[0] == 3 && std::abs (r.treatment.blocks[0].p[2] - 7.5f) < 0.01f
               && r.explanation == "Reverb de caverna.", "resposta estruturada\n");
    }
    {   // JSON no meio de texto, nomes com acento, escolha por nome, valores fora da faixa
        auto r = CloudBrain::parseResponse (200, apiMessage (juce::String::fromUTF8 (u8"Claro! Aqui está:\n{\"explicacao\":\"ok\",\"zerar\":false,\"blocos\":[{\"tipo\":\"Saturação\",\"ligado\":true,\"mix\":250,\"params\":[{\"nome\":\"TIPO\",\"valor\":\"Fita\"},{\"nome\":\"drive\",\"valor\":999}]},{\"tipo\":\"bloco-que-nao-existe\",\"ligado\":true,\"mix\":10,\"params\":[]}]}\nBom som!")));
        CHECK (r.ok && r.treatment.blocks.size() == 1 && (int) r.treatment.blocks[0].p[1] == 1 && r.treatment.blocks[0].p[0] == 36 && r.treatment.blocks[0].mix == 100,
               "resposta bagunçada (ok=%d, blocos=%d)\n", r.ok, (int) r.treatment.blocks.size());
    }
    {
        auto r = CloudBrain::parseResponse (200, apiMessage (R"({"explicacao":"Zerei.","zerar":true,"blocos":[]})"));
        CHECK (r.ok && r.reset, "zerar pela IA online\n");
    }
    CHECK (! CloudBrain::parseResponse (401, R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})").ok, "401\n");
    CHECK (CloudBrain::parseResponse (401, "{}").error.contains ("inv"), "mensagem de chave inválida\n");
    CHECK (! CloudBrain::parseResponse (200, R"({"content":[],"stop_reason":"refusal"})").ok, "recusa\n");
    CHECK (! CloudBrain::parseResponse (200, apiMessage ("nada de json aqui")).ok, "sem JSON\n");
    {   // pedido montado corretamente
        CloudConfig cfg; cfg.apiKey = "x";
        const auto body = juce::JSON::parse (CloudBrain::requestBody (cfg, "teste", {}, {}));
        CHECK (body.getProperty ("model", "").toString() == "claude-haiku-5-5"
               && body.getProperty ("output_config", {}).getProperty ("format", {}).getProperty ("type", "").toString() == "json_schema"
               && body.getProperty ("system", "").toString().contains ("compressor"), "corpo do pedido\n");
    }
    {   // ida e volta do JSON do tratamento
        LocalBrain br; auto t = br.interpret ("voz adoração com delay 1/8 pontuado ping pong", {}).treatment;
        Treatment back; juce::String err;
        CHECK (treatmentFromVar (juce::JSON::parse (juce::JSON::toString (treatmentToVar (t))), back, err) && back == t, "JSON ida e volta\n");
    }
}

static void testHybrid()
{
    std::printf ("== IA híbrida (servidor falso)\n");
    FakeApi api;
    WyattRackProcessor p;
    p.prepareToPlay (48000, 512);
    p.useCloudOverride = true;
    p.cloudOverride.apiKey = "sk-ant-teste";
    p.cloudOverride.endpoint = "http://127.0.0.1:" + juce::String (api.port) + "/v1/messages";
    p.cloudOverride.timeoutMs = 4000;
    p.setAiMode (AiMode::Hibrido);
    api.reply ("200 OK", apiMessage (R"({"explicacao":"Montei um eco de caverna escura.","zerar":false,"blocos":[
        {"tipo":"eq","ligado":true,"mix":100,"params":[{"nome":"corte_agudo","valor":5000}]},
        {"tipo":"delay","ligado":true,"mix":30,"params":[{"nome":"sinc","valor":2},{"nome":"repeticoes","valor":55}]},
        {"tipo":"reverb","ligado":true,"mix":45,"params":[{"nome":"tipo","valor":3},{"nome":"decaimento","valor":8}]}]})"));

    // 1) pedido que o offline entende bem: não usa a internet
    p.ask ("voz pop");
    CHECK (! p.isThinking() && api.count() == 0 && ! p.log().back().online, "pedido comum ficou no offline\n");

    // 2) pedido livre: vai para a IA online
    p.ask ("quero que soe como se eu estivesse cantando sozinho numa caverna escura");
    CHECK (p.isThinking(), "deveria estar pensando\n");
    p.waitForAssistant (6000);
    auto t = p.currentTreatment();
    CHECK (api.count() == 1 && p.log().back().online && t.blocks.size() == 3 && (int) t.blocks[2].p[0] == 3, "resposta online aplicada (%d blocos)\n", (int) t.blocks.size());
    const auto req = api.request();
    CHECK (req.contains ("x-api-key: sk-ant-teste") && req.contains ("anthropic-version: 2026-10-21") && req.contains ("caverna"), "cabeçalhos e pedido enviados\n");
    CHECK (req.contains ("\"blocos\"") || req.contains ("pop") || req.contains ("eq"), "tratamento atual enviado junto\n");

    // 3) desfazer volta ao que era antes da IA online
    p.undo();
    CHECK (p.currentTreatment().find (BlockType::Compressor) != nullptr, "desfazer depois da IA online\n");

    // 4) chave inválida: cai no offline com aviso
    api.reply ("401 Unauthorized", R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})");
    p.ask (juce::String::fromUTF8 ("voz com reverb de igreja mas com aquela textura quebradiça granular estranha borbulhante"));
    p.waitForAssistant (6000);
    CHECK (api.count() >= 2 && ! p.log().back().online && p.log().back().reply.contains (juce::String::fromUTF8 ("inválida")) && p.currentTreatment().find (BlockType::Reverb), "401 → offline (%s)\n", p.log().back().reply.toRawUTF8());

    // 5) sem servidor (sem internet)
    p.cloudOverride.endpoint = "http://127.0.0.1:1/v1/messages";
    p.setAiMode (AiMode::Online);
    p.ask ("telefone");
    p.waitForAssistant (8000);
    CHECK (! p.isThinking() && ! p.log().back().online && p.currentTreatment().find (BlockType::LoFi), "sem conexão → offline (%s)\n", p.log().back().reply.toRawUTF8());

    // 6) modo offline nunca chama a internet
    p.cloudOverride.endpoint = "http://127.0.0.1:" + juce::String (api.port) + "/v1/messages";
    p.setAiMode (AiMode::Offline);
    const int before = api.count();
    p.ask (juce::String::fromUTF8 ("algo totalmente indescritível e abstrato"));
    CHECK (api.count() == before && ! p.isThinking(), "offline não usa a rede\n");
    p.setAiMode (AiMode::Hibrido);
}

static float macroNorm (WyattRackProcessor& p, int m) { return p.apvts.getParameter (WyattRackProcessor::macroId (m))->getValue(); }
static void setMacro (WyattRackProcessor& p, int m, float n) { p.apvts.getParameter (WyattRackProcessor::macroId (m))->setValueNotifyingHost (n); }

static void testMacros()
{
    std::printf ("== Macros\n");
    WyattRackProcessor p; p.setAiMode (AiMode::Offline);
    p.prepareToPlay (48000, 512);
    for (int m = 0; m < kNumMacros; ++m) CHECK (! p.macroUsed (m), "rack vazio sem macros\n");

    p.ask (juce::String::fromUTF8 ("voz quente, na cara, com reverb de igreja"));
    int used = 0; for (int m = 0; m < kNumMacros; ++m) if (p.macroUsed (m)) ++used;
    CHECK (used >= 3, "pedido offline criou %d macros\n", used);
    CHECK (std::abs (macroNorm (p, 0) - 0.5f) < 1e-4f, "macro nasce no meio\n");
    CHECK (p.macroTip (0).contains ("move:"), "dica da macro (%s)\n", p.macroTip (0).toRawUTF8());
    {
        auto* prm = p.apvts.getParameter ("macro1");
        CHECK (prm->getCurrentValueAsText().startsWith (p.macroName (0)), "DAW mostra o nome da macro (%s)\n", prm->getCurrentValueAsText().toRawUTF8());
    }

    // girar e fixar
    const auto base = p.currentTreatment();
    setMacro (p, 0, 1.0f); setMacro (p, 1, 0.2f);
    const auto turned = p.currentTreatment();
    CHECK (! (turned == base) && turned.blocks.size() == base.blocks.size(), "girar a macro muda o tratamento salvo (valor da macro)\n");
    const auto expectBaked = turned.baked();
    p.bakeMacros();
    auto after = p.currentTreatment();
    Treatment blocksOnly = after; blocksOnly.macros = {};
    CHECK (blocksOnly == expectBaked, "FIXAR grava nos blocos o que as macros faziam\n");
    CHECK (std::abs (macroNorm (p, 0) - 0.5f) < 1e-4f && std::abs (macroNorm (p, 1) - 0.5f) < 1e-4f && p.macroUsed (0), "FIXAR volta os knobs ao meio e mantém as macros\n");
    p.undo();
    CHECK (std::abs (macroNorm (p, 0) - 1.0f) < 1e-4f && p.currentTreatment() == turned, "DESFAZER do FIXAR volta a macro girada\n");

    // ajuste com macro girada: o cérebro parte do som que está tocando
    {
        const auto playing = p.currentTreatment().baked();
        const auto* rv0 = playing.find (BlockType::Reverb);
        p.ask ("mais reverb");
        const auto* rv1 = p.currentTreatment().find (BlockType::Reverb);
        CHECK (rv0 && rv1 && rv1->mix > rv0->mix, "\"mais reverb\" parte do som com a macro (%.1f → %.1f)\n", rv0 ? rv0->mix : -1.f, rv1 ? rv1->mix : -1.f);
        CHECK (std::abs (macroNorm (p, 0) - 0.5f) < 1e-4f, "depois do ajuste as macros recomeçam no meio\n");
        bool keptCalor = false; for (int m = 0; m < kNumMacros; ++m) if (p.macroName (m) == "CALOR") keptCalor = true;
        CHECK (keptCalor, "ajuste não troca as macros do pedido original\n");
    }

    // estado salvo e reaberto
    setMacro (p, 1, 0.8f);
    {
        juce::MemoryBlock mb; p.getStateInformation (mb);
        WyattRackProcessor q; q.prepareToPlay (48000, 512);
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (q.currentTreatment() == p.currentTreatment(), "macros (nomes, alvos e valores) voltam com o projeto\n");
        CHECK (q.macroName (1) == p.macroName (1), "nome da macro 2 salvo\n");
        juce::AudioBuffer<float> a, b;
        processSeconds (p, 0.5, 0.25f, &a); processSeconds (q, 0.5, 0.25f, &b);
        float d = 0; for (int i = 0; i < a.getNumSamples(); ++i) d = std::max (d, std::abs (a.getSample (0, i) - b.getSample (0, i)));
        CHECK (d < 1e-4f, "projeto reaberto soa igual (dif. %.6f)\n", d);

        // sessão antiga (sem macros salvas) ganha macros automáticas
        auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
        if (auto* w = xml->getChildByName ("WYATT")) { w->removeAttribute ("macros"); }
        for (auto* c : xml->getChildIterator()) if (c->getStringAttribute ("id").startsWith ("macro")) c->setAttribute ("value", 50.0);
        juce::MemoryBlock old; juce::AudioProcessor::copyXmlToBinary (*xml, old);
        WyattRackProcessor r; r.prepareToPlay (48000, 512);
        r.setStateInformation (old.getData(), (int) old.getSize());
        CHECK (r.macroUsed (0) && r.currentTreatment().blocks.size() == p.currentTreatment().blocks.size(), "projeto da versão 1.0 ganha macros\n");
    }

    // mover e trocar bloco
    {
        auto t = p.currentTreatment();
        int m0 = -1; for (int m = 0; m < kNumMacros; ++m) if (p.macroUsed (m)) { m0 = m; break; }
        const int blk = t.macros[(size_t) m0].targets[0].block;
        const int typ = t.blocks[(size_t) blk].type;
        p.moveSlot (blk, blk == 0 ? 1 : 0);
        auto t2 = p.currentTreatment();
        bool follows = false;
        for (auto& tg : t2.macros[(size_t) m0].targets) if (t2.blocks[(size_t) tg.block].type == typ) follows = true;
        CHECK (follows, "macro continua no mesmo bloco depois de mover\n");
        const int nb = (blk == 0 ? 1 : 0);
        p.setSlotType (nb, (int) BlockType::Tremolo);
        auto t3 = p.currentTreatment();
        bool none = true; for (auto& mm : t3.macros) for (auto& tg : mm.targets) if (tg.block == nb) none = false;
        CHECK (none, "bloco trocado solta das macros\n");
    }

    // biblioteca
    {
        p.ask (juce::String::fromUTF8 ("violão aberto e espacial"));
        setMacro (p, 0, 0.7f);
        const auto ref = p.currentTreatment();
        CHECK (p.saveToLibrary ("__teste macros__"), "salvar na biblioteca\n");
        auto f = WyattRackProcessor::libraryFolder().getChildFile ("__teste macros__.wyrack");
        CHECK (f.loadFileAsString().contains ("\"macros\""), "arquivo .wyrack tem as macros\n");
        p.ask ("telefone");
        CHECK (p.loadFromFile (f) && p.currentTreatment() == ref, "abrir da biblioteca volta as macros e a posição delas\n");
        f.deleteFile();
        // .wyrack antigo, sem macros
        juce::var v = treatmentToVar (ref.baked());
        CHECK (! juce::JSON::toString (v).contains ("macros"), "tratamento sem macros não grava a lista\n");
        auto old = WyattRackProcessor::libraryFolder().getChildFile ("__antigo__.wyrack");
        old.replaceWithText (juce::JSON::toString (v));
        CHECK (p.loadFromFile (old) && p.macroUsed (0), ".wyrack antigo ganha macros ao abrir\n");
        old.deleteFile();
    }

    // preset de fábrica
    p.setCurrentProgram (12);   // Baixo Gordo
    bool peso = false; for (int m = 0; m < kNumMacros; ++m) if (p.macroName (m) == "PESO") peso = true;
    CHECK (peso, "preset Baixo Gordo tem PESO\n");

    // zerar
    p.ask ("zera tudo");
    bool any = false; for (int m = 0; m < kNumMacros; ++m) any |= p.macroUsed (m);
    CHECK (! any, "zerar o rack apaga as macros\n");
}

static void testMacrosCloud()
{
    std::printf ("== Macros da IA avançada (servidor falso)\n");
    FakeApi api;
    WyattRackProcessor p;
    p.prepareToPlay (48000, 512);
    p.useCloudOverride = true;
    p.cloudOverride.apiKey = "sk-ant-teste";
    p.cloudOverride.endpoint = "http://127.0.0.1:" + juce::String (api.port) + "/v1/messages";
    p.cloudOverride.timeoutMs = 4000;
    p.setAiMode (AiMode::Online);
    api.reply ("200 OK", apiMessage (juce::String::fromUTF8 (R"({"explicacao":"Bateria funk com mojo vintage.","zerar":false,"blocos":[
        {"tipo":"eq","ligado":true,"mix":100,"params":[{"nome":"grave","valor":3}]},
        {"tipo":"compressor","ligado":true,"mix":100,"params":[{"nome":"limiar","valor":-24}]},
        {"tipo":"saturacao","ligado":true,"mix":100,"params":[{"nome":"drive","valor":12}]}],
        "macros":[
          {"nome":"mojo","alvos":[{"bloco":2,"param":"drive","quanto":0.3},{"bloco":2,"param":"tipo","quanto":0.5}]},
          {"nome":"THUMP","alvos":[{"bloco":0,"param":"grave","quanto":0.25},{"bloco":1,"param":"ataque","quanto":0.2}]},
          {"nome":"BITE","alvos":[{"bloco":1,"param":"limiar","quanto":-0.2},{"bloco":9,"param":"mix","quanto":0.2}]},
          {"nome":"FANTASMA","alvos":[{"bloco":5,"param":"mix","quanto":0.3}]}]})")));
    p.ask (juce::String::fromUTF8 ("dá um mojo vintage funk nessa bateria"));
    p.waitForAssistant (6000);
    CHECK (p.log().back().online, "resposta online\n");
    CHECK (p.macroName (0) == "MOJO" && p.macroName (1) == "THUMP" && p.macroName (2) == "BITE", "nomes da IA (%s, %s, %s)\n", p.macroName (0).toRawUTF8(), p.macroName (1).toRawUTF8(), p.macroName (2).toRawUTF8());
    auto t = p.currentTreatment();
    CHECK (t.macros[0].targets.size() == 1 && t.macros[2].targets.size() == 1, "alvos inválidos da IA descartados (escolha, bloco 9)\n");
    CHECK (! p.macroUsed (3), "macro sem nenhum alvo válido some\n");
    const auto req = api.request();
    CHECK (req.contains ("\"macros\"") && req.contains ("alvos"), "pedido ensina a IA a criar macros\n");

    // IA sem macros → automáticas
    api.reply ("200 OK", apiMessage (R"({"explicacao":"Só um reverb.","zerar":false,"blocos":[{"tipo":"reverb","ligado":true,"mix":30,"params":[]}],"macros":[]})"));
    p.ask (juce::String::fromUTF8 ("reverb de catedral bem distante"));
    p.waitForAssistant (6000);
    bool esp = false; for (int m = 0; m < kNumMacros; ++m) if (p.macroName (m) == juce::String::fromUTF8 ("ESPAÇO")) esp = true;
    CHECK (p.log().back().online && esp, "IA sem macros → macros automáticas com ESPAÇO\n");
}

static void testEditor()
{
    std::printf ("== Tela\n");
    WyattRackProcessor p; p.setAiMode (AiMode::Hibrido);
    p.prepareToPlay (48000, 512);
    std::unique_ptr<WyattRackEditor> ed (dynamic_cast<WyattRackEditor*> (p.createEditor()));
    CHECK (ed != nullptr && ed->getWidth() == 1100, "editor criado\n");
    ed->setVisible (true);
    snap (*ed, "01-vazio");

    ed->promptBox.setText (juce::String::fromUTF8 ("voz quente, na cara, com reverb de igreja"), false);
    ed->createBtn.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    CHECK (p.currentTreatment().blocks.size() >= 4, "CRIAR pela tela montou o rack\n");
    processSeconds (p, 1.0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    snap (*ed, "02-voz-igreja");

    ed->promptBox.setText ("adlib com eco", false);
    ed->submitPrompt();
    processSeconds (p, 1.0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    snap (*ed, "03-adlibs");

    CHECK (ed->macroKnobs[0].isEnabled() && ! ed->bakeBtn.isEnabled(), "knobs de macro ativos, FIXAR apagado no meio\n");
    ed->macroKnobs[0].slider.setValue (80.0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK (std::abs (p.currentTreatment().macros[0].value - 0.8f) < 1e-3f && ed->bakeBtn.isEnabled(), "girar a macro na tela\n");
    snap (*ed, "03b-macro-girada");
    ed->bakeBtn.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK (std::abs (ed->macroKnobs[0].slider.getValue() - 50.0) < 0.01, "FIXAR pela tela\n");

    p.setCurrentProgram (11); // Bateria Suja
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    processSeconds (p, 1.0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    snap (*ed, "04-bateria");

    ed->undoBtn.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK (p.currentTreatment().find (BlockType::Delay) != nullptr, "DESFAZER pela tela\n");

    ed->modeBtns[0].triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK (p.aiMode() == AiMode::Offline, "botão OFFLINE\n");
    ed->modeBtns[1].triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK (p.aiMode() == AiMode::Hibrido, "botão HÍBRIDO\n");

    ed->openSettings();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK (ed->settingsOpen(), "configurações abertas\n");
    snap (*ed, "05-config-ia");
    ed->closeSettings();

    ed->setSize (1500, 1000);
    snap (*ed, "06-grande");
    ed->setSize (980, 720);
    snap (*ed, "07-pequeno");
    ed.reset();
    CHECK (true, "editor fechado sem travar\n");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    outDir = juce::File (argc > 1 ? argv[1] : "capturas");
    outDir.createDirectory();
    testProcessor();
    testAnalysis();
    testCloudParsing();
    testHybrid();
    testMacros();
    testMacrosCloud();
    testEditor();
    std::printf ("\n%d verificações OK, %d falharam\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
