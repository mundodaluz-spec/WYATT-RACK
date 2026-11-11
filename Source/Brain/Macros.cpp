#include "Macros.h"
#include <algorithm>
#include <cctype>

namespace wr
{
namespace
{
struct Tgt { BlockType type; const char* param; float depth; };   // param: chave do catálogo ou "mix"

struct Template
{
    const char* name;
    int order;                       // posição na tela (da esquerda para a direita)
    float base;                      // prioridade sem nenhuma palavra do pedido
    std::vector<const char*> words;  // palavras do pedido que puxam esta macro
    std::vector<Tgt> targets;
};

// Profundidades em unidades normalizadas do parâmetro: 0,25 num ganho de -12..+12 dB = ±6 dB no fim do curso.
const std::vector<Template>& templates()
{
    using B = BlockType;
    static const std::vector<Template> t {
        { "PESO", 0, 2.0f, { "grave", "peso", "pesad", "gord", "encorpad", "corpo", "grosso", "sub", "808", "baixo", "graves" },
          { { B::EQ, "grave", 0.25f }, { B::EQ, "corte_grave", -0.15f }, { B::Saturacao, "drive", 0.12f },
            { B::LoFi, "corte_grave", -0.2f }, { B::Estereo, "graves_mono", 0.1f } } },

        { "BRILHO", 1, 2.0f, { "brilh", "agudo", "cristal", "presenca", "aberto", "aberta", "ar", "claro", "clara", "abafad" },
          { { B::EQ, "agudo", 0.25f }, { B::EQ, "corte_agudo", 0.15f }, { B::Saturacao, "tom", 0.2f },
            { B::Reverb, "agudos", 0.15f }, { B::Delay, "tom", 0.12f }, { B::LoFi, "corte_agudo", 0.2f },
            { B::DeEsser, "limiar", -0.08f } } },

        { "PRESSÃO", 2, 3.0f, { "pressao", "na cara", "punch", "pegada", "compri", "compress", "socad", "forte", "master", "alto", "denso", "densa", "radio" },
          { { B::Compressor, "limiar", -0.2f }, { B::Compressor, "ratio", 0.12f }, { B::Compressor, "ganho", 0.1f },
            { B::Limitador, "ganho", 0.2f } } },

        { "CALOR", 3, 2.5f, { "quent", "calor", "valvul", "fita", "vintage", "analogic", "saturad", "mojo", "drive", "aveludad" },
          { { B::Saturacao, "drive", 0.3f }, { B::Saturacao, "saida", -0.12f }, { B::EQ, "medio", 0.1f } } },

        { "SUJEIRA", 3, 2.5f, { "sujo", "suja", "sujeira", "lo-fi", "lofi", "telefone", "megafone", "radio antigo", "8-bit", "8 bit", "distor", "crush", "podre", "velho", "antigo" },
          { { B::LoFi, "bits", -0.2f }, { B::LoFi, "ruido", 0.2f }, { B::LoFi, "oscilacao", 0.2f },
            { B::LoFi, "mix", 0.2f }, { B::Saturacao, "drive", 0.25f } } },

        { "MOVIMENTO", 4, 2.5f, { "chorus", "phaser", "flanger", "tremolo", "moviment", "sonhador", "anos 80", "modula", "fantasma", "auto-pan", "autopan", "balanc", "psicodel" },
          { { B::Chorus, "profundidade", 0.25f }, { B::Chorus, "mix", 0.15f }, { B::Modulacao, "profundidade", 0.25f },
            { B::Modulacao, "velocidade", 0.12f }, { B::Tremolo, "profundidade", 0.25f }, { B::Tremolo, "auto_pan", 0.2f },
            { B::Filtro, "lfo_prof", 0.25f }, { B::Delay, "modulacao", 0.2f } } },

        { "FILTRO", 5, 2.0f, { "filtro", "wah", "debaixo", "agua", "varredura", "abafad" },
          { { B::Filtro, "frequencia", 0.3f }, { B::Filtro, "ressonancia", 0.12f } } },

        { "LARGURA", 6, 1.5f, { "larg", "estereo", "stereo", "wide", "dobra", "abert", "lados" },
          { { B::Estereo, "largura", 0.25f }, { B::Estereo, "dobra", 0.12f }, { B::Chorus, "largura", 0.2f },
            { B::Modulacao, "estereo", 0.2f }, { B::Reverb, "largura", 0.15f } } },

        { "ESPAÇO", 7, 3.0f, { "reverb", "igreja", "espac", "sala", "catedral", "plate", "delay", "eco", "ambien", "distant", "profund", "longe", "adoracao" },
          { { B::Reverb, "mix", 0.18f }, { B::Reverb, "decaimento", 0.12f }, { B::Reverb, "tamanho", 0.2f },
            { B::Delay, "mix", 0.15f }, { B::Delay, "repeticoes", 0.15f } } },

        { "S", 8, 1.0f, { "chiad", "sibil", "esse", "de-esser", "deesser" },
          { { B::DeEsser, "limiar", -0.2f }, { B::DeEsser, "reducao", 0.2f } } },

        { "GATE", 9, 0.6f, { "gate", "ruido de fundo", "silencio", "chiado de fundo" },
          { { B::Gate, "limiar", 0.2f }, { B::Gate, "faixa", 0.2f } } },
    };
    return t;
}

std::vector<std::string> words (const std::string& s)
{
    std::vector<std::string> w; std::string cur;
    for (char c : s)
    {
        if (std::isalnum ((unsigned char) c) || c == '-') cur += c;
        else { if (! cur.empty()) w.push_back (cur); cur.clear(); }
    }
    if (! cur.empty()) w.push_back (cur);
    return w;
}

bool mentions (const std::string& text, const std::vector<std::string>& ws, const char* key)
{
    const std::string k = key;
    if (k.find (' ') != std::string::npos) return (" " + text + " ").find (" " + k) != std::string::npos;
    for (auto& w : ws)
    {
        if (k.size() <= 3) { if (w == k) return true; }
        else if (w.compare (0, k.size(), k) == 0) return true;
    }
    return false;
}
} // namespace

void autoMacros (Treatment& t, const std::string& prompt)
{
    t.macros = {};
    if (t.blocks.empty()) return;
    const std::string text = semAcento (prompt);
    const auto ws = words (text);
    const bool hasLoFi = t.find (BlockType::LoFi) != nullptr;

    struct Cand { const Template* tp; float score; std::vector<MacroTarget> targets; };
    std::vector<Cand> cands;
    for (auto& tp : templates())
    {
        // CALOR e SUJEIRA disputam a mesma vaga: com LO-FI na cadeia, é SUJEIRA
        if (std::string (tp.name) == "CALOR" && hasLoFi) continue;
        if (std::string (tp.name) == "SUJEIRA" && ! hasLoFi && ! mentions (text, ws, "sujo") && ! mentions (text, ws, "suja")) continue;

        Cand c { &tp, tp.base, {} };
        for (auto& tg : tp.targets)
            for (int b = 0; b < (int) t.blocks.size(); ++b)
            {
                if (t.blocks[(size_t) b].bt() != tg.type) continue;
                const int pi = std::string (tg.param) == "mix" ? kMacroMix : paramFromName (tg.type, tg.param);
                if (pi != kMacroMix && pi < 0) continue;
                if (! macroCanTarget ((int) tg.type, pi)) continue;
                c.targets.push_back ({ b, pi, tg.depth });
            }
        if (c.targets.empty()) continue;
        c.score += 0.4f * (float) std::min<size_t> (c.targets.size(), 4);
        for (auto* k : tp.words) if (mentions (text, ws, k)) { c.score += 10.0f; break; }
        cands.push_back (std::move (c));
    }
    std::stable_sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b) { return a.score > b.score; });
    if (cands.size() > (size_t) kNumMacros) cands.resize ((size_t) kNumMacros);
    std::stable_sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b) { return a.tp->order < b.tp->order; });

    for (size_t i = 0; i < cands.size(); ++i)
    {
        auto& m = t.macros[i];
        m.name = cands[i].tp->name;
        m.value = 0.5f;
        m.targets = cands[i].targets;
    }
    t.sanitizeMacros();
}

std::string macroDescription (const Treatment& t, int mi)
{
    if (mi < 0 || mi >= kNumMacros) return {};
    const auto& m = t.macros[(size_t) mi];
    std::string s;
    for (auto& tg : m.targets)
    {
        if (tg.block < 0 || tg.block >= (int) t.blocks.size()) continue;
        const auto& bi = info (t.blocks[(size_t) tg.block].type);
        std::string pn = tg.param == kMacroMix ? std::string ("mix") : std::string (bi.params[(size_t) tg.param].name);
        for (auto& c : pn) c = (char) std::tolower ((unsigned char) c);
        if (! s.empty()) s += ", ";
        s += std::string (bi.name) + " " + pn + (tg.depth < 0 ? " (inverso)" : "");
    }
    return s;
}
} // namespace wr
