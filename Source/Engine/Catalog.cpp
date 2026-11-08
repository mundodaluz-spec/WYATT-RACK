#include "Catalog.h"
#include <cstdio>
#include <algorithm>
#include <cctype>

namespace wr
{
using S = Scale;
static const ParamSpec none {};

static const std::array<BlockInfo, kNumTypes>& table()
{
    static const std::array<BlockInfo, kNumTypes> t {{
        { BlockType::Vazio, "VAZIO", "vazio", "Slot livre", 100, { none, none, none, none, none, none } },

        { BlockType::EQ, "EQ", "eq", "Equalizador: cortes, grave, médio e agudo", 100, {
            ParamSpec { "CORTE GRAVE", "corte_grave", "Hz", 20, 600, 20, S::Log },
            ParamSpec { "GRAVE", "grave", "dB", -12, 12, 0 },
            ParamSpec { "MÉDIO FREQ", "medio_freq", "Hz", 150, 6000, 800, S::Log },
            ParamSpec { "MÉDIO", "medio", "dB", -12, 12, 0 },
            ParamSpec { "AGUDO", "agudo", "dB", -12, 12, 0 },
            ParamSpec { "CORTE AGUDO", "corte_agudo", "Hz", 1000, 20000, 20000, S::Log } } },

        { BlockType::Compressor, "COMPRESSOR", "compressor", "Controla a dinâmica e dá pressão", 100, {
            ParamSpec { "LIMIAR", "limiar", "dB", -50, 0, -18 },
            ParamSpec { "RATIO", "ratio", ":1", 1, 20, 4, S::Log },
            ParamSpec { "ATAQUE", "ataque", "ms", 0.1f, 100, 10, S::Log },
            ParamSpec { "SOLTURA", "soltura", "ms", 10, 1500, 120, S::Log },
            ParamSpec { "GANHO", "ganho", "dB", 0, 24, 0 },
            ParamSpec { "JOELHO", "joelho", "dB", 0, 12, 6 } } },

        { BlockType::Saturacao, "SATURAÇÃO", "saturacao", "Calor, sujeira e harmônicos", 100, {
            ParamSpec { "DRIVE", "drive", "dB", 0, 36, 6 },
            ParamSpec { "TIPO", "tipo", "", 0, 2, 0, S::Choice, { "Válvula", "Fita", "Transistor" } },
            ParamSpec { "TOM", "tom", "dB", -6, 6, 0 },
            ParamSpec { "SAÍDA", "saida", "dB", -12, 12, 0 },
            none, none } },

        { BlockType::Reverb, "REVERB", "reverb", "Ambiência: sala, igreja, plate ou catedral", 25, {
            ParamSpec { "TIPO", "tipo", "", 0, 3, 0, S::Choice, { "Sala", "Igreja", "Plate", "Catedral" } },
            ParamSpec { "TAMANHO", "tamanho", "%", 0, 100, 50 },
            ParamSpec { "DECAIMENTO", "decaimento", "s", 0.2f, 12, 2, S::Log },
            ParamSpec { "PRÉ-DELAY", "pre_delay", "ms", 0, 200, 20 },
            ParamSpec { "AGUDOS", "agudos", "%", 0, 100, 50 },
            ParamSpec { "LARGURA", "largura", "%", 0, 100, 100 } } },

        { BlockType::Delay, "DELAY", "delay", "Eco com sincronia ao andamento da música", 20, {
            ParamSpec { "TEMPO", "tempo", "ms", 1, 2000, 350, S::Log },
            ParamSpec { "SINC", "sinc", "", 0, 7, 0, S::Choice, { "Livre", "1/4", "1/4 pont.", "1/8", "1/8 pont.", "1/8 terc.", "1/16", "1/2" } },
            ParamSpec { "REPETIÇÕES", "repeticoes", "%", 0, 95, 35 },
            ParamSpec { "TOM", "tom", "Hz", 1000, 20000, 6000, S::Log },
            ParamSpec { "PING-PONG", "ping_pong", "", 0, 1, 0, S::Toggle },
            ParamSpec { "MODULAÇÃO", "modulacao", "%", 0, 100, 0 } } },

        { BlockType::Chorus, "CHORUS", "chorus", "Dobra e alarga o som", 50, {
            ParamSpec { "VELOCIDADE", "velocidade", "Hz", 0.05f, 5, 0.8f, S::Log },
            ParamSpec { "PROFUNDIDADE", "profundidade", "%", 0, 100, 40 },
            ParamSpec { "VOZES", "vozes", "", 0, 2, 1, S::Choice, { "1", "2", "3" } },
            ParamSpec { "LARGURA", "largura", "%", 0, 100, 100 },
            ParamSpec { "ATRASO", "atraso", "ms", 5, 30, 12 },
            ParamSpec { "REALIM.", "realimentacao", "%", 0, 90, 0 } } },

        { BlockType::Modulacao, "PHASER/FLANGER", "modulacao", "Movimento: phaser ou flanger", 100, {
            ParamSpec { "TIPO", "tipo", "", 0, 1, 0, S::Choice, { "Phaser", "Flanger" } },
            ParamSpec { "VELOCIDADE", "velocidade", "Hz", 0.02f, 8, 0.4f, S::Log },
            ParamSpec { "PROFUNDIDADE", "profundidade", "%", 0, 100, 60 },
            ParamSpec { "REALIM.", "realimentacao", "%", -90, 90, 30 },
            ParamSpec { "CENTRO", "centro", "Hz", 200, 4000, 1000, S::Log },
            ParamSpec { "ESTÉREO", "estereo", "%", 0, 100, 50 } } },

        { BlockType::Filtro, "FILTRO", "filtro", "Filtro ressonante com LFO e envelope (auto-wah)", 100, {
            ParamSpec { "TIPO", "tipo", "", 0, 2, 0, S::Choice, { "Passa-baixas", "Passa-altas", "Passa-banda" } },
            ParamSpec { "FREQUÊNCIA", "frequencia", "Hz", 20, 20000, 2000, S::Log },
            ParamSpec { "RESSONÂNCIA", "ressonancia", "%", 0, 100, 20 },
            ParamSpec { "LFO", "lfo", "Hz", 0.02f, 10, 1, S::Log },
            ParamSpec { "LFO PROF.", "lfo_prof", "%", 0, 100, 0 },
            ParamSpec { "ENVELOPE", "envelope", "%", -100, 100, 0 } } },

        { BlockType::DeEsser, "DE-ESSER", "deesser", "Doma os S e o chiado", 100, {
            ParamSpec { "FREQ", "freq", "Hz", 3000, 12000, 6000, S::Log },
            ParamSpec { "LIMIAR", "limiar", "dB", -60, 0, -30 },
            ParamSpec { "REDUÇÃO MÁX", "reducao", "dB", 0, 24, 10 },
            ParamSpec { "MODO", "modo", "", 0, 1, 0, S::Choice, { "Agudos", "Banda inteira" } },
            none, none } },

        { BlockType::Gate, "GATE", "gate", "Fecha o som nos silêncios", 100, {
            ParamSpec { "LIMIAR", "limiar", "dB", -80, 0, -45 },
            ParamSpec { "ATAQUE", "ataque", "ms", 0.1f, 50, 1, S::Log },
            ParamSpec { "SEGURA", "segura", "ms", 0, 500, 50 },
            ParamSpec { "SOLTURA", "soltura", "ms", 5, 2000, 150, S::Log },
            ParamSpec { "FAIXA", "faixa", "dB", 0, 80, 40 },
            none } },

        { BlockType::Estereo, "ESTÉREO", "estereo", "Largura, graves em mono, pan e dobra", 100, {
            ParamSpec { "LARGURA", "largura", "%", 0, 200, 100 },
            ParamSpec { "GRAVES MONO", "graves_mono", "Hz", 20, 400, 20, S::Log },
            ParamSpec { "PAN", "pan", "", -100, 100, 0 },
            ParamSpec { "DOBRA", "dobra", "ms", 0, 30, 0 },
            none, none } },

        { BlockType::Tremolo, "TREMOLO", "tremolo", "Volume pulsando ou auto-pan", 100, {
            ParamSpec { "VELOCIDADE", "velocidade", "Hz", 0.1f, 20, 4, S::Log },
            ParamSpec { "PROFUNDIDADE", "profundidade", "%", 0, 100, 50 },
            ParamSpec { "FORMA", "forma", "", 0, 2, 0, S::Choice, { "Seno", "Triângulo", "Quadrada" } },
            ParamSpec { "AUTO-PAN", "auto_pan", "%", 0, 100, 0 },
            ParamSpec { "SINC", "sinc", "", 0, 5, 0, S::Choice, { "Livre", "1/2", "1/4", "1/8", "1/16", "1/8 terc." } },
            none } },

        { BlockType::LoFi, "LO-FI", "lofi", "Bits, taxa, ruído, banda e oscilação de fita", 100, {
            ParamSpec { "BITS", "bits", "", 2, 24, 12 },
            ParamSpec { "TAXA", "taxa", "Hz", 1000, 48000, 22050, S::Log },
            ParamSpec { "RUÍDO", "ruido", "%", 0, 100, 10 },
            ParamSpec { "CORTE GRAVE", "corte_grave", "Hz", 20, 1000, 150, S::Log },
            ParamSpec { "CORTE AGUDO", "corte_agudo", "Hz", 1000, 20000, 6000, S::Log },
            ParamSpec { "OSCILAÇÃO", "oscilacao", "%", 0, 100, 20 } } },

        { BlockType::Limitador, "LIMITADOR", "limitador", "Segura os picos e aumenta o volume", 100, {
            ParamSpec { "TETO", "teto", "dB", -12, 0, -1 },
            ParamSpec { "GANHO", "ganho", "dB", 0, 24, 3 },
            ParamSpec { "SOLTURA", "soltura", "ms", 1, 1000, 80, S::Log },
            none, none, none } },
    }};
    return t;
}

const BlockInfo& info (BlockType t)
{
    const int i = (int) t;
    return table()[(size_t) (i < 0 || i >= kNumTypes ? 0 : i)];
}

std::string ParamSpec::format (float v) const
{
    char b[64];
    if (scale == Scale::Choice)
    {
        const int i = (int) clamp (v);
        return choices.empty() ? "" : choices[(size_t) i];
    }
    if (scale == Scale::Toggle) return v >= 0.5f ? "LIGADO" : "DESLIG.";

    const std::string u = unit;
    if (u == "Hz")
    {
        if (v >= 1000) std::snprintf (b, sizeof b, "%.1f kHz", v / 1000.0f);
        else           std::snprintf (b, sizeof b, "%.0f Hz", v);
        std::string s = b;
        for (auto& c : s) if (c == '.') c = ',';
        return s;
    }
    if (u == "ms" && v >= 1000) { std::snprintf (b, sizeof b, "%.2f s", v / 1000.0f); }
    else if (u == "ms" && v < 10) { std::snprintf (b, sizeof b, "%.1f ms", v); }
    else if (u == "s")  { std::snprintf (b, sizeof b, "%.1f s", v); }
    else if (u == "dB") { std::snprintf (b, sizeof b, "%+.1f dB", v); }
    else if (u == ":1") { std::snprintf (b, sizeof b, "%.1f:1", v); }
    else if (u.empty()) { std::snprintf (b, sizeof b, "%.0f", v); }
    else                { std::snprintf (b, sizeof b, "%.0f %s", v, unit); }
    std::string s = b;
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}

std::string semAcento (const std::string& in)
{
    std::string out; out.reserve (in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        const unsigned char c = (unsigned char) in[i];
        if (c < 0x80) { out += (char) std::tolower (c); continue; }
        if (c == 0xC3 && i + 1 < in.size())
        {
            const unsigned char d = (unsigned char) in[++i];
            char r = '?';
            switch (d)
            {
                case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: r = 'a'; break;
                case 0xA7: case 0x87: r = 'c'; break;
                case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0x88: case 0x89: case 0x8A: case 0x8B: r = 'e'; break;
                case 0xAC: case 0xAD: case 0xAE: case 0xAF: case 0x8C: case 0x8D: case 0x8E: case 0x8F: r = 'i'; break;
                case 0xB1: case 0x91: r = 'n'; break;
                case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: r = 'o'; break;
                case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0x99: case 0x9A: case 0x9B: case 0x9C: r = 'u'; break;
                default: r = ' ';
            }
            out += r;
            continue;
        }
        // Outros caracteres multibyte (aspas curvas, travessões, emojis): viram espaço.
        int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
        i += (size_t) extra;
        out += ' ';
    }
    return out;
}

static std::string keyOnly (const std::string& s)
{
    std::string k;
    for (char c : semAcento (s)) if (std::isalnum ((unsigned char) c)) k += c;
    return k;
}

int typeFromName (const std::string& name)
{
    const std::string k = keyOnly (name);
    if (k.empty()) return -1;
    for (int t = 1; t < kNumTypes; ++t)
        if (keyOnly (info (t).key) == k || keyOnly (info (t).name) == k) return t;
    struct A { const char* a; BlockType t; };
    static const A aliases[] = {
        { "equalizador", BlockType::EQ }, { "equalizacao", BlockType::EQ }, { "eq", BlockType::EQ },
        { "comp", BlockType::Compressor }, { "compressao", BlockType::Compressor },
        { "saturador", BlockType::Saturacao }, { "distorcao", BlockType::Saturacao }, { "drive", BlockType::Saturacao },
        { "reverberacao", BlockType::Reverb }, { "reverb", BlockType::Reverb }, { "eco", BlockType::Delay },
        { "phaser", BlockType::Modulacao }, { "flanger", BlockType::Modulacao }, { "phaserflanger", BlockType::Modulacao },
        { "filter", BlockType::Filtro }, { "wah", BlockType::Filtro }, { "deesser", BlockType::DeEsser },
        { "noisegate", BlockType::Gate }, { "stereo", BlockType::Estereo }, { "largura", BlockType::Estereo },
        { "autopan", BlockType::Tremolo }, { "lofi", BlockType::LoFi }, { "bitcrusher", BlockType::LoFi },
        { "limiter", BlockType::Limitador }, { "limitador", BlockType::Limitador },
    };
    for (auto& a : aliases) if (k == a.a) return (int) a.t;
    return -1;
}

int paramFromName (BlockType t, const std::string& name)
{
    const std::string k = keyOnly (name);
    const auto& bi = info (t);
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& p = bi.params[(size_t) i];
        if (! p.used()) continue;
        if (keyOnly (p.key) == k || keyOnly (p.name) == k) return i;
    }
    if (k.size() == 2 && k[0] == 'p' && k[1] >= '1' && k[1] <= '6')
    {
        const int i = k[1] - '1';
        return bi.params[(size_t) i].used() ? i : -1;
    }
    return -1;
}
} // namespace wr
