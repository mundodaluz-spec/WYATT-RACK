#include "CloudBrain.h"

namespace wr
{
static juce::String T (const char* s) { return juce::String::fromUTF8 (s); }
// ==================================================================== JSON
juce::var treatmentToVar (const Treatment& t)
{
    juce::Array<juce::var> arr;
    for (auto& b : t.blocks)
    {
        const auto& bi = info (b.type);
        auto* o = new juce::DynamicObject();
        o->setProperty ("tipo", juce::String (bi.key));
        o->setProperty ("ligado", b.on);
        o->setProperty ("mix", (double) std::round (b.mix * 10) / 10);
        auto* ps = new juce::DynamicObject();
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto& sp = bi.params[(size_t) i];
            if (! sp.used()) continue;
            const float v = b.p[(size_t) i];
            if (sp.scale == Scale::Choice) ps->setProperty (sp.key, (int) v);
            else if (sp.scale == Scale::Toggle) ps->setProperty (sp.key, v >= 0.5f);
            else ps->setProperty (sp.key, (double) std::round (v * 1000) / 1000);
        }
        o->setProperty ("params", juce::var (ps));
        arr.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("blocos", arr);
    if (t.hasMacros())
    {
        juce::Array<juce::var> ms;
        for (auto& m : t.macros)
        {
            auto* mo = new juce::DynamicObject();
            mo->setProperty ("nome", juce::String::fromUTF8 (m.name.c_str()));
            mo->setProperty ("valor", (double) std::round (m.value * 1000) / 1000);
            juce::Array<juce::var> alvos;
            for (auto& tg : m.targets)
            {
                if (tg.block < 0 || tg.block >= (int) t.blocks.size()) continue;
                auto* a = new juce::DynamicObject();
                a->setProperty ("bloco", tg.block);
                a->setProperty ("param", tg.param == kMacroMix ? juce::String ("mix")
                                                               : juce::String (info (t.blocks[(size_t) tg.block].type).params[(size_t) tg.param].key));
                a->setProperty ("quanto", (double) std::round (tg.depth * 1000) / 1000);
                alvos.add (juce::var (a));
            }
            mo->setProperty ("alvos", alvos);
            ms.add (juce::var (mo));
        }
        root->setProperty ("macros", ms);
    }
    return juce::var (root);
}

int macrosFromVar (const juce::var& list, Treatment& out)
{
    out.macros = {};
    if (! list.isArray()) return 0;
    int mi = 0;
    for (auto& item : *list.getArray())
    {
        if (mi >= kNumMacros) break;
        Macro m;
        m.name = item.getProperty ("nome", item.getProperty ("name", "")).toString().trim().toUpperCase().substring (0, 12).toStdString();
        const auto v = item.getProperty ("valor", item.getProperty ("value", 0.5));
        m.value = juce::jlimit (0.0f, 1.0f, (float) (double) v);
        if (auto* alvos = item.getProperty ("alvos", item.getProperty ("targets", juce::var())).getArray())
            for (auto& a : *alvos)
            {
                const int b = (int) a.getProperty ("bloco", a.getProperty ("block", -1));
                if (b < 0 || b >= (int) out.blocks.size()) continue;
                const auto pn = a.getProperty ("param", "").toString();
                const int type = out.blocks[(size_t) b].type;
                const int pi = pn.trim().equalsIgnoreCase ("mix") ? kMacroMix : paramFromName ((BlockType) type, pn.toStdString());
                if (pi != kMacroMix && pi < 0) continue;
                m.targets.push_back ({ b, pi, juce::jlimit (-1.0f, 1.0f, (float) (double) a.getProperty ("quanto", a.getProperty ("depth", 0.0))) });
            }
        if (m.name.empty()) m.name = "MACRO " + std::to_string (mi + 1);
        out.macros[(size_t) mi] = m;
        ++mi;
    }
    out.sanitizeMacros();
    // compacta: macros vazias (todos os alvos inválidos) saem e as outras sobem
    std::array<Macro, kNumMacros> packed; int n = 0;
    for (auto& m : out.macros) if (m.used()) packed[(size_t) n++] = m;
    out.macros = packed;
    return n;
}

static float numberFrom (const juce::var& v, const ParamSpec& sp, bool& ok)
{
    ok = true;
    if (v.isBool()) return (bool) v ? 1.0f : 0.0f;
    if (v.isInt() || v.isInt64() || v.isDouble()) return (float) (double) v;
    if (v.isString())
    {
        const auto s = v.toString().trim();
        if (sp.scale == Scale::Choice)
        {
            const auto k = semAcento (s.toStdString());
            for (int c = 0; c < sp.numChoices(); ++c)
                if (semAcento (sp.choices[(size_t) c]) == k) return (float) c;
        }
        if (sp.scale == Scale::Toggle)
        {
            const auto k = semAcento (s.toStdString());
            if (k == "sim" || k == "ligado" || k == "true" || k == "on") return 1;
            if (k == "nao" || k == "desligado" || k == "false" || k == "off") return 0;
        }
        if (s.containsAnyOf ("0123456789")) return s.retainCharacters ("-0123456789.,").replaceCharacter (',', '.').getFloatValue();
    }
    ok = false;
    return 0;
}

bool treatmentFromVar (const juce::var& v, Treatment& out, juce::String& error)
{
    out = {};
    const juce::var* list = &v;
    juce::var tmp;
    if (! v.isArray())
    {
        tmp = v.getProperty ("blocos", juce::var());
        if (! tmp.isArray()) tmp = v.getProperty ("blocks", juce::var());
        if (! tmp.isArray()) { error = "JSON sem a lista \"blocos\""; return false; }
        list = &tmp;
    }
    for (auto& item : *list->getArray())
    {
        if ((int) out.blocks.size() >= kNumSlots) break;
        const int t = typeFromName (item.getProperty ("tipo", item.getProperty ("type", "")).toString().toStdString());
        if (t <= 0) { error = "bloco desconhecido: " + item.getProperty ("tipo", "").toString(); continue; }
        auto b = BlockState::defaults ((BlockType) t);
        const auto on = item.getProperty ("ligado", item.getProperty ("on", true));
        b.on = on.isString() ? ! on.toString().equalsIgnoreCase ("false") : (bool) on;
        const auto mx = item.getProperty ("mix", juce::var());
        if (! mx.isVoid()) { bool ok; const float m = numberFrom (mx, info (t).params[0], ok); if (ok) b.mix = m; }
        const auto ps = item.getProperty ("params", juce::var());
        auto setParam = [&] (const juce::String& name, const juce::var& val)
        {
            const int pi = paramFromName ((BlockType) t, name.toStdString());
            if (pi < 0) return;
            bool ok; const float f = numberFrom (val, info (t).params[(size_t) pi], ok);
            if (ok) b.p[(size_t) pi] = f;
        };
        if (auto* obj = ps.getDynamicObject())
            for (auto& nv : obj->getProperties()) setParam (nv.name.toString(), nv.value);
        else if (ps.isArray())
            for (auto& e : *ps.getArray()) setParam (e.getProperty ("nome", e.getProperty ("name", "")).toString(), e.getProperty ("valor", e.getProperty ("value", juce::var())));
        b.clampAll();
        out.blocks.push_back (b);
    }
    if (out.blocks.empty() && list->size() > 0) return false;
    if (! v.isArray()) macrosFromVar (v.getProperty ("macros", juce::var()), out);
    error.clear();
    return true;
}

// ==================================================================== prompt
juce::String CloudBrain::systemPrompt()
{
    juce::String s;
    s << T ("Você é o motor de ideias do WYATT RACK, um plugin de áudio da WYATT Sounds (Brasil). "
         "O usuário descreve em português (às vezes com gírias de estúdio, referências de artistas ou erros de digitação) o som que quer, "
         "e você monta uma cadeia de efeitos usando SOMENTE os blocos do catálogo abaixo. Você não gera áudio: o DSP do plugin faz o som.\n\n"
         "Regras:\n"
         "- No máximo 8 blocos, na ordem do sinal (normalmente: gate, eq, deesser, compressor, saturacao, lofi, filtro, modulacao, chorus, tremolo, delay, reverb, estereo, limitador).\n"
         "- \"mix\" vai de 0 a 100 (% do efeito misturado ao seco). Reverb e delay em faixa individual costumam ficar entre 10 e 35.\n"
         "- Ajuste limiares (compressor, de-esser, gate) e o ganho do limitador ao nível real informado na análise da faixa; sem análise, assuma nível médio de -18 dBFS.\n"
         "- Use os nomes de parâmetros exatamente como no catálogo. Escolhas (TIPO, SINC, FORMA...) são o número da opção, começando em 0. Liga/desliga é 0 ou 1.\n"
         "- Se houver um tratamento atual e o pedido for um ajuste (\"mais\", \"menos\", \"tira\", \"sem\"), parta dele e mude só o necessário. "
         "Se for um pedido novo, monte do zero.\n"
         "- Se o pedido for para zerar/limpar tudo, devolva \"zerar\": true e blocos vazios.\n"
         "- Pense em como um engenheiro de mixagem experiente chegaria nesse som (inclusive em produções gospel, sertanejo, trap, funk e adoração).\n"
         "- \"macros\": crie de 2 a 4 knobs de macro com nomes curtos em MAIÚSCULAS, do jeito que um músico falaria (ex.: PESO, BRILHO, PRESSÃO, CALOR, ESPAÇO, MOJO, PEGADA, FANTASMA). "
         "Cada macro move vários parâmetros juntos: \"alvos\" lista {bloco: posição do bloco na sua lista, começando em 0; param: nome do parâmetro ou \"mix\"; quanto: de -0.5 a 0.5}. "
         "\"quanto\" é a fração da faixa do parâmetro que ele anda quando o knob vai do meio até o fim (negativo = anda ao contrário; ex.: mais PRESSÃO = limiar do compressor mais baixo). "
         "O knob começa no meio (o som que você montou) e o usuário gira para os dois lados. Escolha macros que façam sentido para ESTE pedido. Não use parâmetros de escolha (TIPO, SINC, FORMA, MODO, VOZES) nem liga/desliga.\n"
         "- \"explicacao\": 1 ou 2 frases curtas em português do Brasil, no tom de um técnico de estúdio, dizendo o que você montou e por quê.\n\n"
         "Catálogo de blocos (tipo: descrição; parâmetros com unidade, faixa e padrão):\n");
    for (int t = 1; t < kNumTypes; ++t)
    {
        const auto& bi = info (t);
        s << "- " << bi.key << ": " << juce::String::fromUTF8 (bi.description) << " (mix padrão " << (int) bi.defaultMix << ")\n";
        for (auto& p : bi.params)
        {
            if (! p.used()) continue;
            s << "    " << p.key << " ";
            if (p.scale == Scale::Choice)
            {
                s << "[escolha: ";
                for (int c = 0; c < p.numChoices(); ++c) s << (c ? ", " : "") << c << "=" << juce::String::fromUTF8 (p.choices[(size_t) c]);
                s << T ("] padrão ") << (int) p.def;
            }
            else if (p.scale == Scale::Toggle) s << T ("[0 ou 1] padrão ") << (int) p.def;
            else s << "[" << juce::String (p.min, 2) << " a " << juce::String (p.max, 2) << " " << juce::String::fromUTF8 (p.unit) << T ("] padrão ") << juce::String (p.def, 2);
            s << "\n";
        }
    }
    return s;
}

static juce::var outputSchema()
{
    const char* json = R"({
      "type": "object",
      "properties": {
        "explicacao": { "type": "string" },
        "zerar": { "type": "boolean" },
        "blocos": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "tipo": { "type": "string" },
              "ligado": { "type": "boolean" },
              "mix": { "type": "number" },
              "params": {
                "type": "array",
                "items": {
                  "type": "object",
                  "properties": { "nome": { "type": "string" }, "valor": { "type": "number" } },
                  "required": ["nome", "valor"],
                  "additionalProperties": false
                }
              }
            },
            "required": ["tipo", "ligado", "mix", "params"],
            "additionalProperties": false
          }
        },
        "macros": {
          "type": "array",
          "items": {
            "type": "object",
            "properties": {
              "nome": { "type": "string" },
              "alvos": {
                "type": "array",
                "items": {
                  "type": "object",
                  "properties": { "bloco": { "type": "integer" }, "param": { "type": "string" }, "quanto": { "type": "number" } },
                  "required": ["bloco", "param", "quanto"],
                  "additionalProperties": false
                }
              }
            },
            "required": ["nome", "alvos"],
            "additionalProperties": false
          }
        }
      },
      "required": ["explicacao", "zerar", "blocos", "macros"],
      "additionalProperties": false
    })";
    return juce::JSON::parse (json);
}

juce::String CloudBrain::requestBody (const CloudConfig& cfg, const juce::String& prompt, const Treatment& current, const TrackProfile& profile)
{
    juce::String user;
    user << "Tratamento atual: " << (current.empty() ? juce::String ("(rack vazio)") : juce::JSON::toString (treatmentToVar (current), true)) << "\n";
    user << "Análise da faixa: " << juce::String::fromUTF8 (profile.describe().c_str()) << "\n";
    user << "Pedido: " << prompt;

    auto* msg = new juce::DynamicObject();
    msg->setProperty ("role", "user");
    msg->setProperty ("content", user);

    auto* fmt = new juce::DynamicObject();
    fmt->setProperty ("type", "json_schema");
    fmt->setProperty ("schema", outputSchema());
    auto* oc = new juce::DynamicObject();
    oc->setProperty ("format", juce::var (fmt));

    auto* body = new juce::DynamicObject();
    body->setProperty ("model", cfg.model);
    body->setProperty ("max_tokens", 2000);
    body->setProperty ("system", systemPrompt());
    body->setProperty ("messages", juce::Array<juce::var> { juce::var (msg) });
    body->setProperty ("output_config", juce::var (oc));
    return juce::JSON::toString (juce::var (body), true);
}

CloudResult CloudBrain::parseResponse (int status, const juce::String& body)
{
    CloudResult r;
    const auto j = juce::JSON::parse (body);
    if (status != 200)
    {
        const auto apiMsg = j.getProperty ("error", juce::var()).getProperty ("message", "").toString();
        if (status == 401 || status == 403) r.error = T ("Chave da IA avançada inválida. Confira nas configurações (⚙).");
        else if (status == 429) r.error = T ("A IA avançada está ocupada ou o limite da sua conta acabou. Tente de novo em instantes.");
        else if (status == 400) r.error = T ("A IA avançada recusou o pedido (") + apiMsg.substring (0, 120) + ")";
        else if (status >= 500) r.error = T ("A IA avançada está fora do ar agora. Usei o cérebro offline.");
        else if (status == 0) r.error = T ("Sem conexão com a internet. Usei o cérebro offline.");
        else r.error = T ("Erro ") + juce::String (status) + T (" na IA avançada.");
        return r;
    }
    if (j.getProperty ("stop_reason", "").toString() == "refusal") { r.error = T ("A IA avançada não quis responder a esse pedido."); return r; }

    juce::String text;
    if (auto* arr = j.getProperty ("content", juce::var()).getArray())
        for (auto& c : *arr) if (c.getProperty ("type", "").toString() == "text") text += c.getProperty ("text", "").toString();
    if (text.isEmpty()) { r.error = T ("A IA avançada respondeu vazio."); return r; }

    // aceita JSON puro ou JSON no meio de texto
    auto out = juce::JSON::parse (text);
    if (! out.isObject())
    {
        const int a = text.indexOfChar ('{'), b = text.lastIndexOfChar ('}');
        if (a >= 0 && b > a) out = juce::JSON::parse (text.substring (a, b + 1));
    }
    if (! out.isObject()) { r.error = T ("Não consegui ler a resposta da IA avançada."); return r; }

    r.explanation = out.getProperty ("explicacao", "").toString().trim();
    r.reset = (bool) out.getProperty ("zerar", false);
    juce::String err;
    if (! r.reset && ! treatmentFromVar (out, r.treatment, err)) { r.error = T ("Resposta da IA sem blocos válidos (") + err + ")."; return r; }
    r.ok = true;
    return r;
}

CloudResult CloudBrain::ask (const CloudConfig& cfg, const juce::String& prompt, const Treatment& current, const TrackProfile& profile)
{
    if (cfg.apiKey.trim().isEmpty()) { CloudResult r; r.error = T ("IA avançada sem chave. Configure no ⚙."); return r; }

    const auto body = requestBody (cfg, prompt, current, profile);
    juce::String headers;
    headers << "content-type: application/json\r\n"
            << "anthropic-version: 2026-10-21\r\n"
            << "x-api-key: " << cfg.apiKey.trim() << "\r\n";

    int status = 0;
    juce::StringPairArray respHeaders;
    auto url = juce::URL (cfg.endpoint).withPOSTData (body);
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders (headers)
                    .withConnectionTimeoutMs (cfg.timeoutMs)
                    .withStatusCode (&status)
                    .withResponseHeaders (&respHeaders)
                    .withHttpRequestCmd ("POST");
    std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
    if (in == nullptr) return parseResponse (0, {});
    const auto resp = in->readEntireStreamAsString();
    return parseResponse (status, resp);
}
} // namespace wr
