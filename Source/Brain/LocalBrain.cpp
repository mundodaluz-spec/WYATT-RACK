// WYATT RACK — cérebro offline (regras locais com tolerância a erros de digitação)
#include "LocalBrain.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <random>
#include <set>

namespace wr
{
using BT = BlockType;

// Índices dos parâmetros (ver Catalog.cpp)
namespace eq  { enum { HPF, GRAVE, MF, MED, AGU, LPF }; }
namespace cp  { enum { THR, RAT, ATK, REL, MK, KNEE }; }
namespace st  { enum { DRV, TYPE, TOM, OUT }; }
namespace rv  { enum { TYPE, SIZE, DEC, PRE, DAMP, WIDTH }; }
namespace dl  { enum { TIME, SYNC, FB, TONE, PP, MOD }; }
namespace ch  { enum { RATE, DEPTH, VOX, WIDTH, DLY, FB }; }
namespace md  { enum { TYPE, RATE, DEPTH, FB, CENTER, STEREO }; }
namespace fl  { enum { TYPE, FREQ, RES, LFO, LFOD, ENV }; }
namespace ds  { enum { FREQ, THR, RED, MODE }; }
namespace gt  { enum { THR, ATK, HOLD, REL, RANGE }; }
namespace es  { enum { WIDTH, MONOLOW, PAN, HAAS }; }
namespace tr  { enum { RATE, DEPTH, SHAPE, PAN, SYNC }; }
namespace lf  { enum { BITS, RATE, NOISE, HP, LP, WOW }; }
namespace lm  { enum { CEIL, GAIN, REL }; }

int fuzzyDistance (const std::string& a, const std::string& b)
{
    const size_t n = a.size(), m = b.size();
    std::vector<std::vector<int>> d (n + 1, std::vector<int> (m + 1));
    for (size_t i = 0; i <= n; ++i) d[i][0] = (int) i;
    for (size_t j = 0; j <= m; ++j) d[0][j] = (int) j;
    for (size_t i = 1; i <= n; ++i)
        for (size_t j = 1; j <= m; ++j)
        {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            d[i][j] = std::min ({ d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost });
            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) d[i][j] = std::min (d[i][j], d[i - 2][j - 2] + 1);
        }
    return d[n][m];
}

namespace
{
// ------------------------------------------------------------------ tokens
struct Tok
{
    std::string w;
    bool num = false;
    float v = 0;
    std::string unit;      // unidade colada ou seguinte ("ms", "hz", "db", "%", "s", ":1", "frac")
    bool used = false;
};

bool isDigit (char c) { return c >= '0' && c <= '9'; }
bool isAlpha (char c) { return (c >= 'a' && c <= 'z'); }

std::string normalizeUnit (const std::string& u)
{
    if (u == "ms" || u == "milissegundo" || u == "milissegundos" || u == "mili") return "ms";
    if (u == "s" || u == "seg" || u == "segundo" || u == "segundos" || u == "sec") return "s";
    if (u == "hz" || u == "hertz") return "hz";
    if (u == "k" || u == "khz") return "khz";
    if (u == "db" || u == "dbs" || u == "decibeis" || u == "decibel") return "db";
    if (u == "%" || u == "porcento") return "%";
    if (u == "bpm") return "bpm";
    return "";
}

std::vector<Tok> tokenize (const std::string& raw)
{
    std::string s = semAcento (raw);
    // remove apóstrofos e junta hífens entre letras ("lo-fi", "d'agua")
    std::string t;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        if (c == '\'' || c == '`') continue;
        if (c == '-' && i > 0 && i + 1 < s.size() && (isAlpha (s[i - 1]) || isDigit (s[i - 1])) && isAlpha (s[i + 1])) continue;
        t += c;
    }
    std::vector<Tok> out;
    size_t i = 0;
    while (i < t.size())
    {
        const char c = t[i];
        const bool signedNum = (c == '-' || c == '+') && i + 1 < t.size() && isDigit (t[i + 1]) && (i == 0 || ! isAlpha (t[i - 1]));
        if (isDigit (c) || signedNum)
        {
            size_t j = i + (signedNum ? 1 : 0);
            std::string num; if (c == '-') num = "-";
            while (j < t.size() && (isDigit (t[j]) || ((t[j] == ',' || t[j] == '.') && j + 1 < t.size() && isDigit (t[j + 1]))))
            {
                num += (t[j] == ',') ? '.' : t[j]; ++j;
            }
            Tok k; k.num = true; k.v = (float) std::atof (num.c_str());
            k.w = num[0] == '-' ? num : num;
            if (j < t.size() && (t[j] == '/' || t[j] == ':') && j + 1 < t.size() && isDigit (t[j + 1]))
            {
                const char sep = t[j]; size_t q = j + 1; std::string den;
                while (q < t.size() && isDigit (t[q])) den += t[q++];
                k.w = num + std::string (1, sep) + den;
                if (sep == '/') { k.unit = "frac"; k.v = std::atof (num.c_str()) / std::max (1.0, std::atof (den.c_str())); }
                else            { k.unit = ":1"; k.v = (float) std::atof (num.c_str()); }
                j = q;
            }
            else
            {
                // unidade colada ("300ms", "8k", "40%")
                size_t q = j; std::string u;
                while (q < t.size() && (isAlpha (t[q]) || t[q] == '%')) u += t[q++];
                const std::string nu = normalizeUnit (u);
                if (! nu.empty()) { k.unit = nu; j = q; }
                else if (! u.empty())
                {   // "8bit", "8d", "808" → separa número e letras
                    out.push_back (k);
                    Tok w; w.w = u; out.push_back (w);
                    i = q; continue;
                }
            }
            out.push_back (k);
            i = j;
            continue;
        }
        if (isAlpha (c) || c == '%')
        {
            size_t j = i; std::string w;
            while (j < t.size() && (isAlpha (t[j]) || isDigit (t[j]))) w += t[j++];
            if (c == '%') { w = "%"; j = i + 1; }
            Tok k; k.w = w;
            // unidade separada do número ("300 ms")
            const std::string nu = normalizeUnit (w);
            if (! nu.empty() && ! out.empty() && out.back().num && out.back().unit.empty())
            {
                out.back().unit = nu; i = j; continue;
            }
            if (w == "por" && j + 6 <= t.size() && t.compare (j, 6, " cento") == 0 && ! out.empty() && out.back().num)
            {
                out.back().unit = "%"; i = j + 6; continue;
            }
            out.push_back (k);
            i = j;
            continue;
        }
        ++i;
    }
    return out;
}

bool wordEq (const std::string& a, const std::string& key)
{
    if (a == key) return true;
    auto strip = [] (const std::string& x) { return (x.size() > 3 && x.back() == 's') ? x.substr (0, x.size() - 1) : x; };
    const std::string sa = strip (a), sk = strip (key);
    if (sa == sk) return true;
    if (key.size() >= 5 && a.size() >= 4)
    {
        const int d = fuzzyDistance (sa, sk);
        if (d <= 1) return true;
        if (key.size() >= 8 && d <= 2) return true;
    }
    return false;
}

std::vector<std::string> splitWords (const std::string& s)
{
    std::vector<std::string> v; std::string cur;
    for (char c : s) { if (c == ' ') { if (! cur.empty()) v.push_back (cur); cur.clear(); } else cur += c; }
    if (! cur.empty()) v.push_back (cur);
    return v;
}

// ------------------------------------------------------------------ construtor do tratamento
int rank (BT t)
{
    switch (t)
    {
        case BT::Gate: return 0; case BT::EQ: return 1; case BT::DeEsser: return 2; case BT::Compressor: return 3;
        case BT::Saturacao: return 4; case BT::LoFi: return 5; case BT::Filtro: return 6; case BT::Modulacao: return 7;
        case BT::Chorus: return 8; case BT::Tremolo: return 9; case BT::Delay: return 10; case BT::Reverb: return 11;
        case BT::Estereo: return 12; case BT::Limitador: return 13; default: return 99;
    }
}
bool isWet (BT t) { return t == BT::Reverb || t == BT::Delay || t == BT::Chorus; }

struct B
{
    std::vector<BlockState> blocks;
    std::string inst;
    float dryBias = 0;
    std::set<int> forbid;
    std::set<int> touched;

    BlockState* find (BT t) { for (auto& b : blocks) if (b.type == (int) t) return &b; return nullptr; }
    bool has (BT t) { return find (t) != nullptr; }
    BlockState& ensure (BT t, bool* created = nullptr)
    {
        touched.insert ((int) t);
        if (auto* b = find (t)) { if (created) *created = false; return *b; }
        BlockState nb = BlockState::defaults (t);
        if (isWet (t)) nb.mix = 0;
        size_t pos = blocks.size();
        for (size_t i = 0; i < blocks.size(); ++i) if (rank (blocks[i].bt()) > rank (t)) { pos = i; break; }
        blocks.insert (blocks.begin() + (long) pos, nb);
        if (created) *created = true;
        return *find (t);
    }
    void remove (BT t)
    {
        blocks.erase (std::remove_if (blocks.begin(), blocks.end(), [t] (const BlockState& b) { return b.type == (int) t; }), blocks.end());
    }
    float& P (BT t, int i) { return ensure (t).p[(size_t) i]; }
    void add (BT t, int i, float d) { P (t, i) += d; }
    void set (BT t, int i, float v) { P (t, i) = v; }
    void atLeast (BT t, int i, float v) { auto& p = P (t, i); p = std::max (p, v); }
    void atMost (BT t, int i, float v) { auto& p = P (t, i); p = std::min (p, v); }
    void mixAdd (BT t, float d) { ensure (t).mix += d; }
};

// Estilos de reverb e delay (só redefinem o "tipo" quando o bloco é novo ou de outro tipo)
void reverb (B& b, float k, int type, float size, float dec, float pre, float damp, float amount)
{
    bool created = false;
    auto& r = b.ensure (BT::Reverb, &created);
    if (k > 0 && (created || (int) r.p[rv::TYPE] != type || r.mix < 1))
    {
        r.p[rv::TYPE] = (float) type; r.p[rv::SIZE] = size; r.p[rv::DEC] = dec; r.p[rv::PRE] = pre; r.p[rv::DAMP] = damp;
        if (k > 1.2f) { r.p[rv::SIZE] += 15 * (k - 1); r.p[rv::DEC] *= 1 + 0.45f * (k - 1); }
        if (k < 0.8f) { r.p[rv::SIZE] -= 15 * (1 - k); r.p[rv::DEC] *= 1 - 0.4f * (1 - k); }
    }
    r.mix += amount * k;
}

void delay (B& b, float k, int sync, float timeMs, float fb, float tone, bool pp, float mod, float amount)
{
    bool created = false;
    auto& d = b.ensure (BT::Delay, &created);
    if (k > 0 && (created || d.mix < 1))
    {
        d.p[dl::SYNC] = (float) sync; d.p[dl::TIME] = timeMs; d.p[dl::FB] = fb; d.p[dl::TONE] = tone;
        d.p[dl::PP] = pp ? 1.0f : 0.0f; d.p[dl::MOD] = mod;
    }
    else if (k > 0) d.p[dl::FB] += 8 * k;
    d.mix += amount * k;
}

void comp (B& b, float thr, float ratio, float atk, float rel, float mk, float knee = 6)
{
    auto& c = b.ensure (BT::Compressor);
    c.p[cp::THR] = thr; c.p[cp::RAT] = ratio; c.p[cp::ATK] = atk; c.p[cp::REL] = rel; c.p[cp::MK] = mk; c.p[cp::KNEE] = knee;
}
void sat (B& b, int type, float drive) { auto& s = b.ensure (BT::Saturacao); s.p[st::TYPE] = (float) type; s.p[st::DRV] = drive; }
void deess (B& b, float thr = -30, float red = 8) { auto& d = b.ensure (BT::DeEsser); d.p[ds::THR] = thr; d.p[ds::RED] = red; }
void eqv (B& b, float hpf, float grave, float mf, float med, float agu, float lpf = 20000)
{
    auto& e = b.ensure (BT::EQ);
    e.p[eq::HPF] = hpf; e.p[eq::GRAVE] = grave; e.p[eq::MF] = mf; e.p[eq::MED] = med; e.p[eq::AGU] = agu; e.p[eq::LPF] = lpf;
}

// ------------------------------------------------------------------ números
struct Num { float v = 0; std::string unit; std::string extra; };

bool hookTone (B& b, const Num& n, int param)
{
    if (n.unit != "db" && ! (n.unit.empty() && std::abs (n.v) <= 12)) return false;
    b.set (BT::EQ, param, n.v); return true;
}
float toHz (const Num& n) { return n.unit == "khz" || (n.unit.empty() && n.v < 30) ? n.v * 1000.0f : n.v; }
float toMs (const Num& n) { return n.unit == "s" ? n.v * 1000.0f : n.v; }

// ------------------------------------------------------------------ conceitos
enum Cat { Tone, Genre, Instr, Effect };

struct Concept
{
    const char* label;
    Cat cat;
    std::vector<const char*> keys;
    std::vector<BT> removes;
    BT primary;
    std::function<void (B&, float)> apply;
    std::function<bool (B&, const Num&)> numeric;
    std::vector<std::vector<std::string>> split;
};

void vozBase (B& b)
{
    eqv (b, 90, 0, 3000, 1.5f, 2);
    deess (b);
    comp (b, -22, 3.5f, 10, 120, 5);
    reverb (b, 1, 2, 40, 1.4f, 20, 60, 12);
}

void applyInstrumentBase (B& b, const std::string& inst)
{
    if (inst == "voz") vozBase (b);
    else if (inst == "backs") { vozBase (b); b.set (BT::EQ, eq::HPF, 150); b.ensure (BT::Estereo).p[es::WIDTH] = 140; b.ensure (BT::Chorus); b.mixAdd (BT::Chorus, 15); b.ensure (BT::Reverb).mix += 6; }
    else if (inst == "bateria") { eqv (b, 30, 2, 400, -1.5f, 2); comp (b, -20, 4, 15, 100, 4); sat (b, 1, 6); b.ensure (BT::Limitador); }
    else if (inst == "baixo") { eqv (b, 30, 2, 800, 1.5f, 0, 8000); comp (b, -22, 4, 20, 150, 5); sat (b, 0, 8); b.ensure (BT::Estereo).p[es::MONOLOW] = 200; }
    else if (inst == "violao") { eqv (b, 90, 0, 250, -2, 2.5f); comp (b, -22, 3, 15, 150, 4); reverb (b, 1, 2, 45, 1.5f, 15, 60, 15); b.ensure (BT::Estereo).p[es::WIDTH] = 120; }
    else if (inst == "guitarra") { eqv (b, 90, 0, 1500, 1.5f, 0); comp (b, -20, 3, 15, 150, 3); delay (b, 1, 3, 350, 25, 4500, true, 0, 12); reverb (b, 1, 0, 40, 1.2f, 10, 50, 12); }
    else if (inst == "teclado") { eqv (b, 50, 0, 800, 0, 1.5f); comp (b, -22, 2.5f, 20, 200, 3); b.ensure (BT::Chorus); b.mixAdd (BT::Chorus, 15); reverb (b, 1, 2, 55, 2, 20, 55, 15); b.ensure (BT::Estereo).p[es::WIDTH] = 130; }
    else if (inst == "master") { eqv (b, 25, 0.5f, 3000, 0, 1); comp (b, -14, 2, 30, 200, 2, 10); sat (b, 1, 2); auto& e = b.ensure (BT::Estereo); e.p[es::WIDTH] = 110; e.p[es::MONOLOW] = 120; b.ensure (BT::Limitador); }
}

// "Me surpreenda": 3 a 5 blocos criativos com valores sorteados dentro de faixas musicais.
void surpriseChain (B& b, float k)
{
    static thread_local std::mt19937 rng { std::random_device{}() };
    auto r = [&] (float lo, float hi) { return std::uniform_real_distribution<float> (lo, hi) (rng); };
    auto ri = [&] (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (rng); };
    std::vector<BT> pool { BT::Saturacao, BT::LoFi, BT::Filtro, BT::Modulacao, BT::Chorus, BT::Tremolo, BT::Delay, BT::Reverb };
    std::shuffle (pool.begin(), pool.end(), rng);
    const int n = ri (3, 5);
    for (int i = 0; i < n; ++i)
    {
        auto& x = b.ensure (pool[(size_t) i]);
        switch (pool[(size_t) i])
        {
            case BT::Saturacao: x.p[st::TYPE] = (float) ri (0, 2); x.p[st::DRV] = r (6, 20) * k; x.p[st::TOM] = r (-3, 3); break;
            case BT::LoFi:      x.p[lf::BITS] = r (6, 14); x.p[lf::RATE] = r (6000, 22000); x.p[lf::NOISE] = r (0, 30); x.p[lf::LP] = r (3000, 9000); x.p[lf::WOW] = r (0, 60); break;
            case BT::Filtro:    x.p[fl::TYPE] = (float) ri (0, 2); x.p[fl::FREQ] = r (400, 4000); x.p[fl::RES] = r (20, 70); x.p[fl::LFO] = r (0.1f, 4); x.p[fl::LFOD] = r (20, 80); break;
            case BT::Modulacao: x.p[md::TYPE] = (float) ri (0, 1); x.p[md::RATE] = r (0.05f, 2); x.p[md::DEPTH] = r (40, 90); x.p[md::FB] = r (-60, 70); break;
            case BT::Chorus:    x.p[ch::RATE] = r (0.1f, 2); x.p[ch::DEPTH] = r (30, 90); x.p[ch::VOX] = (float) ri (0, 2); x.mix = r (25, 60); break;
            case BT::Tremolo:   x.p[tr::SYNC] = (float) ri (1, 4); x.p[tr::DEPTH] = r (30, 80); x.p[tr::SHAPE] = (float) ri (0, 2); x.p[tr::PAN] = r (0, 100); break;
            case BT::Delay:     x.p[dl::SYNC] = (float) ri (1, 7); x.p[dl::FB] = r (20, 65); x.p[dl::TONE] = r (2000, 8000); x.p[dl::PP] = (float) ri (0, 1); x.p[dl::MOD] = r (0, 50); x.mix = r (15, 35); break;
            case BT::Reverb:    x.p[rv::TYPE] = (float) ri (0, 3); x.p[rv::SIZE] = r (30, 100); x.p[rv::DEC] = r (1, 8); x.p[rv::PRE] = r (0, 80); x.mix = r (15, 40); break;
            default: break;
        }
    }
}

// Artistas → estilo de som (pedidos como "voz estilo Fernandinho")
struct Artist { const char* name; const char* style; };
const std::vector<Artist>& artists()
{
    static const std::vector<Artist> a {
        // gospel
        { "fernandinho", "gospel" }, { "aline barros", "gospel" }, { "anderson freire", "gospel" }, { "thalles roberto", "gospel" },
        { "bruna karla", "gospel" }, { "cassiane", "gospel" }, { "damares", "gospel" }, { "eyshila", "gospel" }, { "davi sacer", "gospel" },
        { "fernanda brum", "gospel" }, { "leonardo goncalves", "gospel" }, { "paulo cesar baruk", "gospel" }, { "sarah farias", "gospel" },
        { "preto no branco", "gospel" }, { "kirk franklin", "gospel" }, { "regis danese", "gospel" }, { "marquinhos gomes", "gospel" },
        { "harpa crista", "gospel" }, { "padre fabio de melo", "gospel" }, { "padre marcelo rossi", "gospel" }, { "ana paula valadao", "adoracao" },
        // adoração
        { "isadora pompeo", "adoracao" }, { "gabriela rocha", "adoracao" }, { "morada", "adoracao" }, { "nivea soares", "adoracao" },
        { "hillsong", "adoracao" }, { "elevation worship", "adoracao" }, { "bethel", "adoracao" }, { "maverick city", "adoracao" },
        { "diante do trono", "adoracao" }, { "julliany souza", "adoracao" }, { "laura souguellis", "adoracao" }, { "gabi sampaio", "adoracao" },
        { "kemuel", "adoracao" }, { "theo rubia", "adoracao" }, { "brandon lake", "adoracao" }, { "aline barros adoracao", "adoracao" },
        // trap / rap
        { "travis scott", "trap" }, { "ryu the runner", "trap" }, { "matue", "trap" }, { "wiu", "trap" },
        { "veigh", "trap" }, { "kayblack", "trap" }, { "orochi", "trap" }, { "playboi carti", "trap" },
        { "post malone", "trap" }, { "filipe ret", "trap" }, { "l7nnon", "rap" }, { "racionais", "rap" }, { "emicida", "rap" },
        { "criolo", "rap" }, { "djonga", "rap" }, { "baco exu do blues", "rap" }, { "kendrick lamar", "rap" }, { "drake", "rap" },
        { "eminem", "rap" }, { "sabotage", "rap" }, { "projota", "rap" }, { "rashid", "rap" }, { "marcelo d2", "rap" },
        // funk
        { "mc ryan sp", "funk" }, { "mc kevin", "funk" }, { "ludmilla", "funk" }, { "mc daniel", "funk" }, { "mc ig", "funk" },
        { "mc don juan", "funk" }, { "kevin o chris", "funk" }, { "mc cabelinho", "funk" },
        // sertanejo
        { "marilia mendonca", "sertanejo" }, { "henrique e juliano", "sertanejo" }, { "jorge e mateus", "sertanejo" },
        { "gusttavo lima", "sertanejo" }, { "ze neto e cristiano", "sertanejo" }, { "ana castela", "sertanejo" },
        { "luan santana", "sertanejo" }, { "maiara e maraisa", "sertanejo" }, { "simone mendes", "sertanejo" }, { "joao gomes", "sertanejo" },
        // pagode
        { "thiaguinho", "pagode" }, { "pericles", "pagode" }, { "ferrugem", "pagode" }, { "sorriso maroto", "pagode" },
        { "menos e mais", "pagode" }, { "dilsinho", "pagode" }, { "turma do pagode", "pagode" }, { "zeca pagodinho", "pagode" },
        // pop
        { "anitta", "pop" }, { "taylor swift", "pop" }, { "dua lipa", "pop" }, { "the weeknd", "pop" }, { "ariana grande", "pop" },
        { "billie eilish", "pop" }, { "pabllo vittar", "pop" }, { "luisa sonza", "pop" }, { "iza", "pop" }, { "bruno mars", "pop" },
        { "vitor kley", "pop" }, { "melim", "pop" },
        // rock
        { "foo fighters", "rock" }, { "legiao urbana", "rock" }, { "charlie brown jr", "rock" }, { "capital inicial", "rock" },
        { "skillet", "rock" }, { "linkin park", "rock" }, { "nirvana", "rock" }, { "pitty", "rock" }, { "oficina g3", "rock" },
        { "metallica", "rock" }, { "queen", "rock" }, { "u2", "rock" }, { "coldplay", "pop" },
        // R&B / soul
        { "sza", "rnb" }, { "daniel caesar", "rnb" }, { "beyonce", "rnb" }, { "frank ocean", "rnb" }, { "tim maia", "rnb" },
        { "usher", "rnb" }, { "alicia keys", "rnb" },
        // MPB / acústico
        { "caetano", "mpb" }, { "caetano veloso", "mpb" }, { "gilberto gil", "mpb" }, { "djavan", "mpb" }, { "milton nascimento", "mpb" },
        { "marisa monte", "mpb" }, { "anavitoria", "mpb" }, { "tiago iorc", "mpb" }, { "elis regina", "mpb" }, { "tom jobim", "mpb" },
        // country
        { "chris stapleton", "country" }, { "luke combs", "country" }, { "morgan wallen", "country" }, { "johnny cash", "country" },
        { "carrie underwood", "country" }, { "dolly parton", "country" },
        // eletrônica / reggae
        { "alok", "eletronica" }, { "vintage culture", "eletronica" }, { "david guetta", "eletronica" }, { "calvin harris", "eletronica" },
        { "avicii", "eletronica" }, { "bob marley", "reggae" }, { "natiruts", "reggae" }, { "armandinho", "reggae" }, { "ponto de equilibrio", "reggae" },
    };
    return a;
}

std::string titleCase (const std::string& s)
{
    std::string r = s; bool up = true;
    for (auto& c : r) { if (up && c >= 'a' && c <= 'z') c = (char) (c - 32); up = (c == ' '); }
    return r;
}

const std::vector<Concept>& concepts()
{
    static std::vector<Concept> cs = []
    {
        std::vector<Concept> v;
        auto add = [&] (const char* label, Cat cat, std::vector<const char*> keys, std::vector<BT> rm, BT prim,
                        std::function<void (B&, float)> f, std::function<bool (B&, const Num&)> num = nullptr)
        {
            v.push_back ({ label, cat, std::move (keys), std::move (rm), prim, std::move (f), std::move (num), {} });
        };

        // ---------------- timbre / caráter
        add ("brilho", Tone, { "brilho", "brilhante", "brilhoso", "agudo", "agudos", "aberto", "aberta", "arejado", "arejada", "cristalino", "cristalina", "claro", "clara", "brilhar", "bright", "air" }, {}, BT::EQ,
             [] (B& b, float k) { b.add (BT::EQ, eq::AGU, 3 * k); },
             [] (B& b, const Num& n) { return hookTone (b, n, eq::AGU); });
        add ("presença", Tone, { "presenca", "na cara", "na frente", "pra frente", "frente", "perto", "proximo", "proxima", "intimo", "intima", "em cima", "colado na cara", "inteligivel", "definido", "definida", "clareza" }, {}, BT::EQ,
             [] (B& b, float k)
             {
                 auto& e = b.ensure (BT::EQ);
                 if (std::abs (e.p[eq::MED]) < 0.01f) e.p[eq::MF] = 3000;
                 e.p[eq::MED] += 2.5f * k;
                 if (k > 0 && ! b.has (BT::Compressor)) comp (b, -22, 3.5f, 8, 120, 4);
                 b.dryBias += 0.6f * k;
             },
             [] (B& b, const Num& n) { return hookTone (b, n, eq::MED); });
        add ("quente", Tone, { "quente", "calor", "aveludado", "aveludada", "analogico", "analogica", "valvulado", "valvulada", "valvula", "warm", "aconchegante" }, {}, BT::Saturacao,
             [] (B& b, float k)
             {
                 b.add (BT::EQ, eq::GRAVE, 2 * k); b.add (BT::EQ, eq::AGU, -1 * k);
                 bool c = false; auto& s = b.ensure (BT::Saturacao, &c);
                 if (c) { s.p[st::TYPE] = 0; s.p[st::DRV] = 0; }
                 s.p[st::DRV] += 5 * k;
             });
        add ("corpo", Tone, { "grave", "graves", "corpo", "encorpado", "encorpada", "gordo", "gorda", "peso", "pesado", "pesada", "grosso", "grossa", "cheio", "cheia", "robusto", "robusta", "fat" }, {}, BT::EQ,
             [] (B& b, float k) { b.add (BT::EQ, eq::GRAVE, 3 * k); if (k < 0) b.atLeast (BT::EQ, eq::HPF, 20 - 60 * k); },
             [] (B& b, const Num& n) { return hookTone (b, n, eq::GRAVE); });
        add ("escuro", Tone, { "escuro", "escura", "abafado", "abafada", "fechado", "fechada", "sombrio", "sombria", "macio", "macia", "fosco", "fosca", "dark", "opaco", "opaca" }, {}, BT::EQ,
             [] (B& b, float k)
             {
                 b.add (BT::EQ, eq::AGU, -3 * k);
                 if (k > 0) b.atMost (BT::EQ, eq::LPF, std::max (2500.0f, 12000 - 4000 * k));
             });
        add ("fino", Tone, { "fino", "fina", "magro", "magra", "magrinho", "leve demais", "thin" }, {}, BT::EQ,
             [] (B& b, float k) { b.add (BT::EQ, eq::GRAVE, -3 * k); if (k > 0) b.atLeast (BT::EQ, eq::HPF, 150 * k); });
        add ("embolado", Tone, { "embolado", "embolada", "lama", "enlameado", "abafamento", "boxy", "som de caixa", "som de lata", "lata" }, {}, BT::EQ,
             [] (B& b, float k) { auto& e = b.ensure (BT::EQ); e.p[eq::MF] = 350; e.p[eq::MED] -= 3 * k; });
        add ("nasal", Tone, { "nasal", "fanho", "fanhoso", "nariz" }, {}, BT::EQ,
             [] (B& b, float k) { auto& e = b.ensure (BT::EQ); e.p[eq::MF] = 1000; e.p[eq::MED] -= 3 * k; });
        add ("médio", Tone, { "medio", "medios", "meio", "mid", "mids" }, {}, BT::EQ,
             [] (B& b, float k) { b.add (BT::EQ, eq::MED, 2.5f * k); },
             [] (B& b, const Num& n)
             {
                 if (n.unit == "hz" || n.unit == "khz") { b.set (BT::EQ, eq::MF, toHz (n)); return true; }
                 return hookTone (b, n, eq::MED);
             });
        add ("corte de graves", Tone, { "corta grave", "corta os graves", "corta o grave", "corte de grave", "corte de graves", "corte grave", "passa alta", "high pass", "hpf", "highpass", "abaixo de", "abaixo", "rumble", "ronco", "tirar o ronco" }, {}, BT::EQ,
             [] (B& b, float k) { if (k > 0) b.atLeast (BT::EQ, eq::HPF, 100); else b.set (BT::EQ, eq::HPF, 20); },
             [] (B& b, const Num& n) { if (n.unit == "db" || n.unit == "%") return false; b.set (BT::EQ, eq::HPF, toHz (n)); return true; });
        add ("corte de agudos", Tone, { "corta agudo", "corta os agudos", "corta o agudo", "corte de agudo", "corte de agudos", "passa baixa", "low pass", "lpf", "lowpass", "acima de", "acima" }, {}, BT::EQ,
             [] (B& b, float k) { if (k > 0) b.atMost (BT::EQ, eq::LPF, 8000); else b.set (BT::EQ, eq::LPF, 20000); },
             [] (B& b, const Num& n) { if (n.unit == "db" || n.unit == "%") return false; b.set (BT::EQ, eq::LPF, toHz (n)); return true; });
        add ("suave", Tone, { "suave", "suavizar", "suaviza", "doce", "delicado", "delicada", "calmo", "calma", "liso", "lisa", "sedoso", "sedosa", "gentil", "smooth", "tranquilo", "tranquila" }, {}, BT::Compressor,
             [] (B& b, float k)
             {
                 if (k > 0)
                 {
                     if (b.inst.empty() || b.inst == "voz" || b.inst == "backs") deess (b, -32, 6 + 2 * k);
                     if (! b.has (BT::Compressor)) comp (b, -22, 2.5f, 20, 200, 3, 10);
                     else { auto& c = b.ensure (BT::Compressor); c.p[cp::RAT] = std::max (1.5f, c.p[cp::RAT] * 0.75f); c.p[cp::KNEE] = 10; }
                 }
                 b.add (BT::EQ, eq::AGU, -1 * k);
                 if (auto* s = b.find (BT::Saturacao)) s->p[st::DRV] -= 4 * k;
             });
        add ("saturação", Effect, { "saturacao", "saturado", "saturada", "saturar", "satura", "fita", "tape", "harmonico", "harmonicos", "colorido", "colorida", "cor analogica" }, { BT::Saturacao }, BT::Saturacao,
             [] (B& b, float k)
             {
                 bool c = false; auto& s = b.ensure (BT::Saturacao, &c);
                 if (c) { s.p[st::TYPE] = 1; s.p[st::DRV] = 0; }
                 s.p[st::DRV] += 8 * k;
             },
             [] (B& b, const Num& n) { if (n.unit != "%" && n.unit != "db") return false; b.set (BT::Saturacao, st::DRV, n.unit == "%" ? 36 * n.v / 100 : n.v); return true; });
        add ("distorção", Effect, { "distorcao", "distorcido", "distorcida", "distorce", "distorcer", "agressivo", "agressiva", "sujo", "suja", "sujeira", "rasgado", "rasgada", "crunch", "overdrive", "fuzz", "drive", "estourado de proposito", "podre", "raivoso", "raivosa", "bravo", "brava" }, { BT::Saturacao }, BT::Saturacao,
             [] (B& b, float k)
             {
                 bool c = false; auto& s = b.ensure (BT::Saturacao, &c);
                 if (c || k > 0) s.p[st::TYPE] = 2;
                 if (c) s.p[st::DRV] = 0;
                 s.p[st::DRV] += 16 * k;
                 if (k > 0) { auto& e = b.ensure (BT::EQ); if (std::abs (e.p[eq::MED]) < 0.01f) e.p[eq::MF] = 1500; e.p[eq::MED] += 2 * k; }
             },
             [] (B& b, const Num& n) { if (n.unit != "%" && n.unit != "db") return false; b.set (BT::Saturacao, st::DRV, n.unit == "%" ? 36 * n.v / 100 : n.v); return true; });
        add ("pressão", Effect, { "pressao", "punch", "socado", "socada", "pegada", "compressao", "comprimido", "comprimida", "compressor", "comprime", "comprimir", "firme", "colado", "colada", "cola", "segurado", "segurada", "nivelado", "nivelada", "controlado", "controlada", "estavel", "constante", "impacto", "forca", "energia", "energico" }, { BT::Compressor }, BT::Compressor,
             [] (B& b, float k)
             {
                 bool c = false; auto& x = b.ensure (BT::Compressor, &c);
                 if (c) { x.p[cp::THR] = -18; x.p[cp::RAT] = 3; x.p[cp::MK] = 0; }
                 x.p[cp::THR] -= 6 * k;
                 x.p[cp::RAT] *= std::pow (1.35f, k);
                 x.p[cp::MK] += 4.5f * k;
             },
             [] (B& b, const Num& n)
             {
                 auto& x = b.ensure (BT::Compressor);
                 if (n.unit == ":1") { x.p[cp::RAT] = n.v; return true; }
                 if (n.unit == "%")
                 {   // "compressão em 70%": 0% = leve, 100% = esmagado
                     const float a = std::clamp (n.v / 100.0f, 0.0f, 1.0f);
                     x.p[cp::THR] = -10 - 26 * a; x.p[cp::RAT] = 1.5f + 8.5f * a * a; x.p[cp::MK] = 2 + 10 * a; return true;
                 }
                 if (n.unit == "db" && n.v < 0) { x.p[cp::THR] = n.v; return true; }
                 return false;
             });
        add ("natural", Tone, { "natural", "organico", "organica", "dinamico", "dinamica", "respirando", "cru", "crua", "real", "verdadeiro", "verdadeira" }, {}, BT::Compressor,
             [] (B& b, float k)
             {
                 b.dryBias += 0.3f * k;
                 if (auto* c = b.find (BT::Compressor)) c->p[cp::RAT] = std::max (1.5f, c->p[cp::RAT] * std::pow (0.75f, k));
                 if (auto* s = b.find (BT::Saturacao)) s->p[st::DRV] -= 3 * k;
             });
        add ("volume", Effect, { "volume", "alto", "loud", "loudness", "estourado", "estourada", "chapado", "chapada", "maximizado", "maximizar", "mais alto", "na pancada", "forte no volume" }, { BT::Limitador }, BT::Limitador,
             [] (B& b, float k) { bool c = false; auto& l = b.ensure (BT::Limitador, &c); if (c) l.p[lm::GAIN] = 0; l.p[lm::GAIN] += 4 * k; },
             [] (B& b, const Num& n) { if (n.unit != "db") return false; auto& l = b.ensure (BT::Limitador); if (n.v < 0) l.p[lm::CEIL] = n.v; else l.p[lm::GAIN] = n.v; return true; });
        add ("limitador", Effect, { "limitador", "limiter", "teto", "brickwall", "segura os picos", "picos", "pico" }, { BT::Limitador }, BT::Limitador,
             [] (B& b, float k) { if (k > 0) b.ensure (BT::Limitador); else b.forbid.insert ((int) BT::Limitador); },
             [] (B& b, const Num& n) { if (n.unit != "db" && ! (n.unit.empty() && n.v <= 0)) return false; b.set (BT::Limitador, lm::CEIL, n.v); return true; });
        add ("de-esser", Effect, { "sibilancia", "sibilante", "sibilantes", "deesser", "de esser", "esse", "esses", "sss", "ssss", "os s", "chiado do s", "s estourado", "s chiando" }, { BT::DeEsser }, BT::DeEsser,
             [] (B& b, float k) { bool c = false; auto& d = b.ensure (BT::DeEsser, &c); if (! c) { d.p[ds::RED] += 4 * k; d.p[ds::THR] -= 4 * k; } else if (k > 1.2f) { d.p[ds::RED] = 14; d.p[ds::THR] = -36; } },
             [] (B& b, const Num& n) { if (n.unit == "hz" || n.unit == "khz") { b.set (BT::DeEsser, ds::FREQ, toHz (n)); return true; } if (n.unit == "db") { b.set (BT::DeEsser, ds::THR, n.v); return true; } return false; });
        add ("limpo", Tone, { "limpo", "limpa", "limpar", "limpeza", "clean", "sem sujeira", "sem ruido", "higienizado" }, {}, BT::EQ,
             [] (B& b, float k)
             {
                 if (k <= 0) return;
                 b.atLeast (BT::EQ, eq::HPF, 80);
                 if (b.inst.empty() || b.inst == "voz" || b.inst == "backs") { if (! b.has (BT::DeEsser)) deess (b); auto& g = b.ensure (BT::Gate); g.p[gt::THR] = -50; g.p[gt::RANGE] = 20; }
                 if (auto* s = b.find (BT::Saturacao)) s->p[st::DRV] -= 6 * k;
             });
        add ("gate", Effect, { "gate", "noise gate", "ruido de fundo", "barulho de fundo", "vazamento", "ruido do ar", "ar condicionado", "ventilador", "chiado de fundo", "silencio" }, { BT::Gate }, BT::Gate,
             [] (B& b, float k) { if (k > 0) { bool c = false; auto& g = b.ensure (BT::Gate, &c); if (! c) g.p[gt::RANGE] += 10 * k; } },
             [] (B& b, const Num& n) { if (n.unit != "db") return false; b.set (BT::Gate, gt::THR, n.v); return true; });

        // ---------------- espaço
        add ("reverb", Effect, { "reverb", "reverberacao", "reverbe", "reverber", "ambiencia", "ambiente", "espaco", "profundidade", "molhado", "molhada", "wet", "verb" }, { BT::Reverb }, BT::Reverb,
             [] (B& b, float k) { reverb (b, k, 2, 50, 1.8f, 20, 55, 20); },
             [] (B& b, const Num& n)
             {
                 auto& r = b.ensure (BT::Reverb);
                 if (n.unit == "%") { r.mix = n.v; return true; }
                 if (n.unit == "s") { r.p[rv::DEC] = n.v; return true; }
                 if (n.unit == "ms") { r.p[rv::PRE] = n.v; return true; }
                 return false;
             });
        add ("sala", Effect, { "sala", "quarto", "estudio", "room", "sala pequena" }, { BT::Reverb }, BT::Reverb,
             [] (B& b, float k) { reverb (b, k, 0, 35, 0.9f, 8, 55, 15); });
        add ("igreja", Effect, { "igreja", "templo", "capela", "congregacao", "santuario", "church" }, { BT::Reverb }, BT::Reverb,
             [] (B& b, float k) { reverb (b, k, 1, 70, 3.2f, 30, 40, 22); });
        add ("catedral", Effect, { "catedral", "basilica", "galpao", "salao enorme", "cathedral", "hall" }, { BT::Reverb }, BT::Reverb,
             [] (B& b, float k) { reverb (b, k, 3, 85, 6, 40, 45, 25); });
        add ("plate", Effect, { "plate", "placa", "chapa", "plate reverb" }, { BT::Reverb }, BT::Reverb,
             [] (B& b, float k) { reverb (b, k, 2, 50, 1.8f, 15, 70, 18); });
        add ("estádio", Effect, { "estadio", "arena", "ao vivo", "show", "palco", "ginasio", "live" }, { BT::Reverb }, BT::Reverb,
             [] (B& b, float k) { reverb (b, k, 3, 70, 3.5f, 60, 50, 22); delay (b, k, 0, 140, 10, 4000, false, 0, 8); });
        add ("distante", Tone, { "distante", "longe", "ao fundo", "la no fundo", "no fundo", "atras", "la atras", "de longe", "afastado", "afastada", "lonjura" }, {}, BT::Reverb,
             [] (B& b, float k)
             {
                 b.add (BT::EQ, eq::AGU, -3 * k);
                 if (k > 0) b.atLeast (BT::EQ, eq::HPF, 150 + 50 * k);
                 reverb (b, k, 1, 65, 2.8f, 0, 40, 30);
                 b.dryBias -= 0.4f * k;
             });
        add ("seco", Tone, { "seco", "seca", "sequinho", "sem ambiencia", "dry", "sem efeito" }, {}, BT::Reverb,
             [] (B& b, float k)
             {
                 for (BT t : { BT::Reverb, BT::Delay, BT::Chorus })
                     if (auto* x = b.find (t)) x->mix *= std::clamp (1 - 0.55f * k, 0.0f, 2.0f);
                 b.dryBias += 0.5f * k;
             });
        add ("largo", Effect, { "largo", "larga", "estereo", "stereo", "amplo", "ampla", "espacoso", "espacosa", "wide", "3d", "imersivo", "imersiva", "envolvente", "abrir o estereo", "abre", "abrir", "panoramico" }, { BT::Estereo }, BT::Estereo,
             [] (B& b, float k) { b.add (BT::Estereo, es::WIDTH, 40 * k); },
             [] (B& b, const Num& n) { if (n.unit != "%") return false; b.set (BT::Estereo, es::WIDTH, n.v); return true; });
        add ("mono", Tone, { "mono", "centralizado", "centralizada", "no meio", "no centro" }, {}, BT::Estereo,
             [] (B& b, float k) { b.add (BT::Estereo, es::WIDTH, -100 * k); });

        // ---------------- tempo e modulação
        add ("delay", Effect, { "delay", "eco", "ecos", "repeticao", "repeticoes", "repete", "repetindo", "echo", "delei", "dilei" }, { BT::Delay }, BT::Delay,
             [] (B& b, float k) { delay (b, k, 1, 350, 30, 5000, false, 0, 18); }, nullptr /* tratado à parte */);
        add ("ping-pong", Effect, { "ping pong", "pingpong", "pinpong", "de um lado pro outro", "lado a lado", "pula de lado" }, { BT::Delay }, BT::Delay,
             [] (B& b, float k) { delay (b, k, 3, 350, 35, 5000, true, 0, 16); if (k > 0) b.set (BT::Delay, dl::PP, 1); });
        add ("slap", Effect, { "slap", "slapback", "eco curto", "eco rapido", "rockabilly", "dobra curta" }, { BT::Delay }, BT::Delay,
             [] (B& b, float k) { delay (b, k, 0, 110, 8, 4500, false, 0, 18); if (k > 0) { b.set (BT::Delay, dl::SYNC, 0); b.set (BT::Delay, dl::TIME, 110); } });
        add ("espacial", Effect, { "espacial", "sideral", "universo", "cosmico", "cosmica", "galaxia", "flutuando", "flutuante", "sonho", "sonhador", "sonhadora", "etereo", "eterea", "ceu", "celestial", "nuvem", "nuvens", "ambient", "atmosferico", "atmosferica", "atmosfera", "infinito" }, { BT::Reverb, BT::Delay, BT::Chorus }, BT::Reverb,
             [] (B& b, float k)
             {
                 reverb (b, k, 3, 95, 8, 30, 60, 30);
                 delay (b, k, 2, 450, 55, 4000, true, 30, 20);
                 bool c = false; auto& x = b.ensure (BT::Chorus, &c); if (c) { x.p[ch::RATE] = 0.3f; x.p[ch::DEPTH] = 50; x.p[ch::VOX] = 2; } x.mix += 20 * k;
                 b.add (BT::Estereo, es::WIDTH, 50 * k);
             });
        add ("dobra", Effect, { "chorus", "coro", "dobra", "dobrado", "dobrada", "dobrar", "duplicado", "duplicada", "double", "doubler", "grupo", "coral", "multidao", "varias vozes", "um monte de gente", "galera cantando" }, { BT::Chorus }, BT::Chorus,
             [] (B& b, float k)
             {
                 bool c = false; auto& x = b.ensure (BT::Chorus, &c);
                 if (c) { x.p[ch::VOX] = 2; x.p[ch::DEPTH] = 35; x.p[ch::RATE] = 0.6f; x.p[ch::DLY] = 15; }
                 x.mix += 35 * k;
                 if (k > 0) b.add (BT::Estereo, es::WIDTH, 30 * k);
             },
             [] (B& b, const Num& n) { if (n.unit != "%") return false; b.ensure (BT::Chorus).mix = n.v; return true; });
        add ("flanger", Effect, { "flanger", "flange", "jato", "aviao", "turbina" }, { BT::Modulacao }, BT::Modulacao,
             [] (B& b, float k) { if (k <= 0) return; auto& m = b.ensure (BT::Modulacao); m.p[md::TYPE] = 1; m.p[md::DEPTH] = std::min (100.0f, 60 * k); m.p[md::FB] = 50; m.p[md::RATE] = 0.25f; m.p[md::CENTER] = 1000; });
        add ("phaser", Effect, { "phaser", "phase", "fase", "phasing", "psicodelico", "psicodelica", "giratorio", "rodando" }, { BT::Modulacao }, BT::Modulacao,
             [] (B& b, float k) { if (k <= 0) return; auto& m = b.ensure (BT::Modulacao); m.p[md::TYPE] = 0; m.p[md::DEPTH] = std::min (100.0f, 60 * k); m.p[md::FB] = 40; m.p[md::RATE] = 0.35f; });
        add ("tremolo", Effect, { "tremolo", "tremulo", "pulsando", "pulsante", "pulsar", "pulsa", "helicoptero", "tremendo", "picotado", "picotada", "gaguejando" }, { BT::Tremolo }, BT::Tremolo,
             [] (B& b, float k) { bool c = false; auto& t = b.ensure (BT::Tremolo, &c); if (c) { t.p[tr::SYNC] = 3; t.p[tr::DEPTH] = 0; } t.p[tr::DEPTH] += 55 * k; });
        add ("auto-pan", Effect, { "autopan", "auto pan", "girando", "vai e volta", "8 d", "8d", "orbita", "orbitando", "passeando" }, { BT::Tremolo }, BT::Tremolo,
             [] (B& b, float k) { if (k <= 0) return; auto& t = b.ensure (BT::Tremolo); t.p[tr::PAN] = 100; t.p[tr::DEPTH] = std::min (100.0f, 75 * k); t.p[tr::SYNC] = 1; t.p[tr::RATE] = 0.3f; });
        add ("wah", Effect, { "wah", "wahwah", "wah wah", "uau", "auto wah", "autowah", "envelope", "funky" }, { BT::Filtro }, BT::Filtro,
             [] (B& b, float k) { if (k <= 0) return; auto& f = b.ensure (BT::Filtro); f.p[fl::TYPE] = 2; f.p[fl::FREQ] = 500; f.p[fl::RES] = 70; f.p[fl::ENV] = std::min (100.0f, 80 * k); f.p[fl::LFOD] = 0; });
        add ("filtro", Effect, { "filtro", "filtrado", "filtrada", "filtrar", "varredura", "sweep", "filter", "abrindo e fechando" }, { BT::Filtro }, BT::Filtro,
             [] (B& b, float k) { if (k <= 0) return; auto& f = b.ensure (BT::Filtro); f.p[fl::TYPE] = 0; f.p[fl::FREQ] = 1500; f.p[fl::RES] = 40; f.p[fl::LFO] = 0.25f; f.p[fl::LFOD] = std::min (100.0f, 60 * k); },
             [] (B& b, const Num& n) { if (n.unit != "hz" && n.unit != "khz") return false; b.set (BT::Filtro, fl::FREQ, toHz (n)); return true; });

        // ---------------- efeitos de caráter
        add ("rádio antigo", Effect, { "radio antigo", "radio velho", "radinho", "radio", "am", "vitrola", "gramofone", "radio de pilha" }, { BT::LoFi }, BT::LoFi,
             [] (B& b, float k)
             {
                 if (k <= 0) return;
                 eqv (b, 400, 0, 1500, 3, 0, 3800); sat (b, 1, 10);
                 auto& l = b.ensure (BT::LoFi); l.p[lf::BITS] = 10; l.p[lf::RATE] = 11025; l.p[lf::NOISE] = 35; l.p[lf::HP] = 300; l.p[lf::LP] = 4500; l.p[lf::WOW] = 15;
             });
        add ("telefone", Effect, { "telefone", "celular", "ligacao", "orelhao", "interfone", "whatsapp", "audio de zap", "telefonema" }, { BT::LoFi }, BT::LoFi,
             [] (B& b, float k)
             {
                 if (k <= 0) return;
                 eqv (b, 450, 0, 1800, 4, 0, 3200); sat (b, 2, 14);
                 auto& l = b.ensure (BT::LoFi); l.p[lf::BITS] = 8; l.p[lf::RATE] = 8000; l.p[lf::NOISE] = 10; l.p[lf::HP] = 400; l.p[lf::LP] = 3400; l.p[lf::WOW] = 0;
             });
        add ("megafone", Effect, { "megafone", "alto falante", "autofalante", "carro de som", "corneta" }, { BT::Saturacao }, BT::Saturacao,
             [] (B& b, float k) { if (k <= 0) return; eqv (b, 700, 0, 2200, 6, 0, 4000); sat (b, 2, 22); comp (b, -24, 8, 3, 80, 4); });
        add ("lo-fi", Effect, { "lofi", "lo fi", "vintage", "fita k7", "k7", "fita cassete", "cassete", "vinil", "disco velho", "nostalgico", "nostalgica", "nostalgia", "retro", "antigo", "antiga", "chiado", "velhinho", "anos 70", "anos 60", "anos 50" }, { BT::LoFi }, BT::LoFi,
             [] (B& b, float k)
             {
                 bool c = false; auto& l = b.ensure (BT::LoFi, &c);
                 if (c) { l.p[lf::BITS] = 14; l.p[lf::RATE] = 22050; l.p[lf::NOISE] = 0; l.p[lf::HP] = 100; l.p[lf::LP] = 9000; l.p[lf::WOW] = 0; }
                 l.p[lf::BITS] -= 2 * k; l.p[lf::RATE] /= std::pow (1.35f, k); l.p[lf::NOISE] += 25 * k; l.p[lf::LP] /= std::pow (1.3f, k); l.p[lf::WOW] += 30 * k;
                 if (k > 0) { bool cs = false; auto& s = b.ensure (BT::Saturacao, &cs); if (cs) { s.p[st::TYPE] = 1; s.p[st::DRV] = 6; } }
             });
        add ("8-bit", Effect, { "8 bit", "8bit", "8 bits", "videogame", "video game", "game", "chiptune", "robo", "robotico", "robotica", "bitcrusher", "crusher", "crush", "digital quebrado", "maquina" }, { BT::LoFi }, BT::LoFi,
             [] (B& b, float k)
             {
                 if (k <= 0) return;
                 auto& l = b.ensure (BT::LoFi); l.p[lf::BITS] = std::max (2.0f, 7 - 2 * k); l.p[lf::RATE] = 6000 / k; l.p[lf::NOISE] = 0; l.p[lf::HP] = 20; l.p[lf::LP] = 20000; l.p[lf::WOW] = 0;
             },
             [] (B& b, const Num& n) { if (n.unit.empty() && n.v >= 2 && n.v <= 24) { b.set (BT::LoFi, lf::BITS, n.v); return true; } return false; });
        add ("debaixo d'água", Effect, { "debaixo dagua", "embaixo dagua", "debaixo da agua", "embaixo da agua", "de baixo dagua", "agua", "submerso", "submersa", "afogado", "fundo do mar", "aquario", "piscina", "mergulho" }, { BT::Filtro }, BT::Filtro,
             [] (B& b, float k)
             {
                 if (k <= 0) return;
                 auto& f = b.ensure (BT::Filtro); f.p[fl::TYPE] = 0; f.p[fl::FREQ] = std::max (250.0f, 700 / k); f.p[fl::RES] = 30; f.p[fl::LFO] = 0.3f; f.p[fl::LFOD] = 25;
                 bool c = false; auto& x = b.ensure (BT::Chorus, &c); if (c) { x.p[ch::RATE] = 0.4f; x.p[ch::DEPTH] = 70; x.p[ch::VOX] = 1; } x.mix += 40 * k;
                 reverb (b, k, 0, 40, 1.2f, 0, 10, 15);
             });
        add ("fantasma", Effect, { "fantasma", "assombrado", "assombrada", "terror", "sinistro", "sinistra", "misterioso", "misteriosa", "assustador", "assustadora" }, { BT::Reverb }, BT::Reverb,
             [] (B& b, float k)
             {
                 if (k <= 0) return;
                 b.atLeast (BT::EQ, eq::HPF, 300);
                 reverb (b, k, 3, 100, 9, 80, 35, 45);
                 delay (b, k, 2, 500, 50, 3000, true, 40, 25);
                 bool c = false; auto& x = b.ensure (BT::Chorus, &c); if (c) { x.p[ch::RATE] = 0.15f; x.p[ch::DEPTH] = 80; } x.mix += 25 * k;
             });
        add ("aéreo", Tone, { "aereo", "aerea", "soproso", "soprosa", "respirado", "respirada", "sussurro", "sussurrado", "sussurrada", "airy", "ar" }, {}, BT::EQ,
             [] (B& b, float k) { b.add (BT::EQ, eq::AGU, 4 * k); if (k > 0) { b.atLeast (BT::EQ, eq::HPF, 120); if (! b.has (BT::Compressor)) comp (b, -24, 3, 15, 150, 5); } });

        add ("adlibs", Effect, { "adlib", "adlibs", "ad lib", "ad libs", "voz de fundo", "vozes de fundo", "resposta", "respostas", "eco da voz" }, {}, BT::Delay,
             [] (B& b, float k)
             {
                 if (k <= 0) return;
                 b.atLeast (BT::EQ, eq::HPF, 250); b.set (BT::EQ, eq::MF, 2500); b.add (BT::EQ, eq::MED, 3 * k); b.atMost (BT::EQ, eq::LPF, 9000);
                 if (! b.has (BT::Compressor)) comp (b, -26, 5, 5, 100, 6);
                 delay (b, k, 3, 250, 35, 4000, true, 0, 22);
                 reverb (b, k, 2, 45, 1.6f, 15, 55, 18);
                 b.ensure (BT::Estereo).p[es::WIDTH] = 160;
             });
        add ("anos 80", Effect, { "anos 80", "oitentista", "eighties", "synthwave", "retrowave", "sonhador", "sonhadora", "dream pop", "dreampop" }, {}, BT::Chorus,
             [] (B& b, float k)
             {
                 if (k <= 0) return;
                 bool c = false; auto& x = b.ensure (BT::Chorus, &c); if (c) { x.p[ch::RATE] = 0.5f; x.p[ch::DEPTH] = 55; x.p[ch::VOX] = 1; } x.mix += 35 * k;
                 delay (b, k, 2, 450, 35, 4000, true, 20, 16);
                 reverb (b, k, 2, 75, 2.8f, 30, 45, 24);
                 b.ensure (BT::Estereo).p[es::WIDTH] = 140;
             });
        add ("surpresa", Effect, { "surpreenda", "surpreende", "surpresa", "aleatorio", "aleatoria", "sorteia", "sortear", "qualquer coisa", "inventa", "inventar", "criativo", "criativa", "doido", "doida", "maluco", "maluca", "experimental" }, {}, BT::Vazio,
             [] (B& b, float k) { if (k > 0) surpriseChain (b, k); });

        // ---------------- gêneros (voz por padrão)
        add ("gospel", Genre, { "gospel", "louvor", "black gospel", "musica gospel", "crista", "cristao", "evangelico", "evangelica" }, {}, BT::Reverb,
             [] (B& b, float k)
             {
                 eqv (b, 90, -1, 3200, 2, 2.5f); deess (b, -30, 8); comp (b, -22, 4, 8, 120, 5); sat (b, 0, 4);
                 delay (b, 1, 1, 350, 25, 4500, false, 0, 12 * k); reverb (b, 1, 1, 55, 2.2f, 35, 50, 18 * k);
             });
        add ("adoração", Genre, { "adoracao", "worship", "ministracao", "devocional", "congregacional", "intimidade", "presenca de deus", "contemplativo", "contemplativa" }, {}, BT::Reverb,
             [] (B& b, float k)
             {
                 eqv (b, 90, 0, 3000, 1.5f, 3); deess (b, -30, 8); comp (b, -22, 3, 15, 200, 4, 10);
                 delay (b, 1, 2, 450, 40, 5000, true, 15, 18 * k); reverb (b, 1, 3, 80, 5, 40, 50, 26 * k);
                 b.ensure (BT::Estereo).p[es::WIDTH] = 130;
             });
        add ("trap", Genre, { "trap", "drill", "plugg", "rage", "trapzin" }, {}, BT::Compressor,
             [] (B& b, float k)
             {
                 eqv (b, 100, 0, 3500, 2, 3.5f); deess (b, -28, 10); comp (b, -26, 6, 3, 80, 7); sat (b, 1, 8);
                 delay (b, 1, 3, 250, 20, 5000, true, 0, 12 * k); reverb (b, 1, 2, 35, 1.1f, 10, 65, 10 * k);
             });
        add ("rap", Genre, { "rap", "hip hop", "hiphop", "boombap", "boom bap", "freestyle", "rima", "batalha" }, {}, BT::Compressor,
             [] (B& b, float k)
             {
                 eqv (b, 100, 1, 3000, 2.5f, 2); deess (b, -28, 10); comp (b, -26, 5, 5, 100, 7); sat (b, 1, 5);
                 reverb (b, 1, 0, 30, 0.8f, 5, 50, 6 * k);
             });
        add ("funk", Genre, { "funk", "mandelao", "baile", "baile funk", "proibidao", "funk carioca", "bh", "150 bpm", "putaria" }, {}, BT::Compressor,
             [] (B& b, float k)
             {
                 eqv (b, 120, 0, 2500, 3, 2); comp (b, -26, 8, 2, 80, 8); sat (b, 2, 10);
                 auto& l = b.ensure (BT::Limitador); l.p[lm::GAIN] = 4 * k; reverb (b, 1, 0, 25, 0.6f, 0, 50, 5 * k);
             });
        add ("sertanejo", Genre, { "sertanejo", "sertanejo universitario", "modao", "moda de viola", "arrocha", "piseiro", "forro", "vaquejada", "bachata" }, {}, BT::Reverb,
             [] (B& b, float k)
             {
                 eqv (b, 90, 0, 3000, 2, 2); deess (b, -30, 8); comp (b, -22, 4, 8, 120, 5); sat (b, 0, 3);
                 delay (b, 1, 1, 350, 20, 4500, false, 0, 10 * k); reverb (b, 1, 2, 45, 1.6f, 20, 60, 16 * k);
             });
        add ("pagode", Genre, { "pagode", "samba", "pagodinho", "roda de samba", "pagodao", "axe" }, {}, BT::Reverb,
             [] (B& b, float k) { eqv (b, 90, 0, 3000, 1.5f, 2); deess (b); comp (b, -22, 3.5f, 10, 120, 4); reverb (b, 1, 2, 40, 1.4f, 15, 60, 14 * k); });
        add ("pop", Genre, { "pop", "radio pop", "hit", "comercial", "mainstream" }, {}, BT::Compressor,
             [] (B& b, float k)
             {
                 eqv (b, 100, 0, 3000, 2, 3.5f); deess (b, -28, 9); comp (b, -24, 4, 6, 100, 6); sat (b, 1, 4);
                 delay (b, 1, 4, 350, 20, 5000, false, 0, 10 * k); reverb (b, 1, 2, 45, 1.5f, 20, 65, 14 * k); b.ensure (BT::Limitador).p[lm::GAIN] = 2;
             });
        add ("rock", Genre, { "rock", "rock gospel", "metal", "hard rock", "punk", "grunge", "rock and roll", "rockn roll", "heavy" }, {}, BT::Saturacao,
             [] (B& b, float k)
             {
                 eqv (b, 100, 0, 1500, 2, 1.5f); comp (b, -24, 5, 5, 100, 6); sat (b, 2, 9 * k);
                 delay (b, 1, 0, 120, 10, 4000, false, 0, 10 * k); reverb (b, 1, 0, 40, 1.0f, 10, 50, 10 * k);
             });
        add ("R&B / soul", Genre, { "rnb", "r b", "r n b", "soul", "neo soul", "neosoul", "black music", "rhythm and blues" }, {}, BT::Reverb,
             [] (B& b, float k)
             {
                 eqv (b, 90, 1, 3000, 1, 3); deess (b, -30, 8); comp (b, -22, 3, 12, 150, 4); sat (b, 0, 4);
                 auto& c = b.ensure (BT::Chorus); c.p[ch::DEPTH] = 25; c.mix = 15 * k;
                 delay (b, 1, 1, 350, 30, 5000, true, 0, 14 * k); reverb (b, 1, 2, 55, 2.2f, 25, 60, 20 * k);
                 b.ensure (BT::Estereo).p[es::WIDTH] = 120;
             });
        add ("MPB / acústico", Genre, { "mpb", "bossa", "bossa nova", "acustico", "acustica", "voz e violao", "folk", "indie", "intimista", "unplugged", "luau" }, {}, BT::Reverb,
             [] (B& b, float k) { eqv (b, 80, 0, 3000, 0, 1.5f); comp (b, -20, 2.5f, 15, 150, 3, 10); reverb (b, 1, 0, 45, 1.4f, 15, 55, 15 * k); });
        add ("country", Genre, { "country", "country gospel", "bluegrass", "western", "cowboy", "faroeste", "americana" }, {}, BT::Delay,
             [] (B& b, float k)
             {
                 eqv (b, 90, 0, 2800, 2, 1.5f); deess (b); comp (b, -22, 4, 10, 120, 5); sat (b, 1, 4);
                 delay (b, 1, 0, 110, 8, 4500, false, 0, 14 * k); reverb (b, 1, 2, 45, 1.6f, 15, 60, 14 * k);
             });
        add ("reggae / dub", Genre, { "reggae", "dub", "reggaeton", "ska" }, {}, BT::Delay,
             [] (B& b, float k)
             {
                 eqv (b, 90, 1, 1200, 1, 0); comp (b, -22, 4, 10, 120, 4);
                 delay (b, 1, 2, 450, 60, 2500, true, 10, 25 * k); reverb (b, 1, 2, 50, 2, 20, 45, 15 * k);
             });
        add ("eletrônica", Genre, { "eletronica", "eletronico", "edm", "house", "techno", "dance", "rave", "balada" }, {}, BT::Delay,
             [] (B& b, float k)
             {
                 eqv (b, 100, 0, 3000, 1.5f, 3); comp (b, -24, 5, 5, 100, 6); sat (b, 1, 5);
                 delay (b, 1, 3, 250, 30, 5000, true, 0, 14 * k); reverb (b, 1, 2, 55, 1.8f, 20, 70, 16 * k);
                 b.ensure (BT::Estereo).p[es::WIDTH] = 130; b.ensure (BT::Limitador).p[lm::GAIN] = 2;
             });
        add ("podcast / pregação", Genre, { "podcast", "pregacao", "pregador", "pregadora", "pastor", "padre", "locucao", "locutor", "locutora", "fala", "falado", "falada", "narracao", "narrador", "entrevista", "palestra", "sermao", "mensagem", "aula", "videoaula", "voz falada" }, {}, BT::Compressor,
             [] (B& b, float)
             {
                 auto& g = b.ensure (BT::Gate); g.p[gt::THR] = -50; g.p[gt::RANGE] = 18;
                 eqv (b, 90, 1.5f, 250, -2.5f, 2); deess (b, -30, 8); comp (b, -24, 3.5f, 10, 150, 6);
                 auto& l = b.ensure (BT::Limitador); l.p[lm::CEIL] = -1; l.p[lm::GAIN] = 3;
                 b.forbid.insert ((int) BT::Reverb); b.forbid.insert ((int) BT::Delay);
             });

        // ---------------- instrumentos (contexto)
        add ("voz", Instr, { "voz", "vozes", "vocal", "vocais", "cantor", "cantora", "canto", "lead", "voz principal", "solista", "cantando" }, {}, BT::Vazio, nullptr);
        add ("backs", Instr, { "back", "backs", "backing", "backing vocal", "backing vocals", "segunda voz", "vozes de apoio", "coro de apoio" }, {}, BT::Vazio, nullptr);
        add ("bateria", Instr, { "bateria", "batera", "drums", "drum", "bumbo", "kick", "caixa", "snare", "percussao", "loop", "beat", "batida", "groove" }, {}, BT::Vazio, nullptr);
        add ("baixo", Instr, { "baixo", "bass", "contrabaixo", "sub", "808", "subgrave" }, {}, BT::Vazio, nullptr);
        add ("violão", Instr, { "violao", "violoes", "nylon", "viola", "cavaco", "cavaquinho", "ukulele", "acoustic guitar" }, {}, BT::Vazio, nullptr);
        add ("guitarra", Instr, { "guitarra", "guitar", "guita", "solo de guitarra", "riff" }, {}, BT::Vazio, nullptr);
        add ("teclado", Instr, { "piano", "teclado", "teclados", "teclas", "rhodes", "pad", "pads", "synth", "sintetizador", "orgao", "keys", "wurlitzer" }, {}, BT::Vazio, nullptr);
        add ("master", Instr, { "master", "mix", "mixagem", "mixdown", "masterizacao", "masterizar", "musica inteira", "musica toda", "bus", "barramento", "soma", "faixa toda" }, {}, BT::Vazio, nullptr);

        for (auto& c : v)
            for (auto* k : c.keys) c.split.push_back (splitWords (k));
        return v;
    }();
    return cs;
}

const std::set<std::string>& stopwords()
{
    static const std::set<std::string> s {
        "de", "do", "da", "dos", "das", "com", "e", "o", "a", "os", "as", "um", "uma", "uns", "umas", "no", "na", "nos", "nas",
        "pra", "pro", "para", "que", "eu", "quero", "queria", "gostaria", "deixa", "deixe", "deixar", "faz", "faca", "fazer", "som",
        "efeito", "efeitos", "tipo", "estilo", "me", "meu", "minha", "isso", "esse", "essa", "este", "esta", "ai", "aqui", "favor",
        "por", "ficar", "fique", "fica", "soar", "soando", "parecer", "parecendo", "igual", "como", "em", "ao", "aos", "ta", "ficou",
        "tem", "ter", "tenha", "seja", "vibe", "vibes", "pegada de", "assim", "tambem", "agora", "so", "apenas", "ser", "nesse",
        "nessa", "desse", "dessa", "dele", "dela", "nele", "nela", "algo", "coisa", "jeito", "estilo", "aquele", "aquela", "mesmo",
        "voce", "vc", "pf", "pfv", "pls", "please", "ok", "beleza", "show de bola", "massa", "top", "legal", "bom", "boa", "melhor",
        "faixa", "audio", "musica", "track", "ele", "ela", "la", "nao", "quer", "quero", "precisa", "preciso", "dar", "da", "dando",
        "deixando", "colocando", "usar", "usa", "use", "ou", "mas", "porem", "entao", "tudo", "todo", "toda", "q", "tb", "tbm", "vai",
        "fazendo", "pouquinho", "bastante", "the", "and", "with", "of", "sound", "pouca", "sobre", "ponto", "fim", "final", "inicio"
    };
    return s;
}

float modifierValue (const std::string& w)
{
    static const std::set<std::string> weak { "pouco", "pouquinho", "pouca", "poquinho", "levemente", "leve", "leves", "sutil", "sutis", "sutilmente", "discreto", "discreta", "toque", "toquezinho", "pitada", "ligeiramente", "minimo", "minima", "curto", "curta", "pequeno", "pequena", "fraco", "fraca", "baixinho", "devagar", "suavemente" };
    static const std::set<std::string> strong { "bem", "muito", "muita", "bastante", "forte", "fortes", "grande", "grandes", "longo", "longa", "monte", "super", "mega", "intenso", "intensa", "caprichado", "caprichada", "generoso", "generosa", "carregado", "carregada", "pesadao", "bastantinho", "beeem", "mto", "mt" };
    static const std::set<std::string> extreme { "extremo", "extrema", "absurdo", "absurda", "insano", "insana", "enorme", "gigante", "gigantesco", "gigantesca", "maximo", "maxima", "exagerado", "exagerada", "demais", "muitissimo", "totalmente", "infinito", "monstro", "brutal", "violento", "violenta" };
    if (weak.count (w)) return 0.5f;
    if (strong.count (w)) return 1.6f;
    if (extreme.count (w)) return 2.2f;
    return 0;
}

int relVerb (const std::string& w) // +1 mais, -1 menos, +2 adicionar, 0 nada
{
    static const std::set<std::string> plus { "mais", "aumenta", "aumentar", "aumente", "aumentando", "sobe", "subir", "suba", "capricha", "caprichar", "reforca", "reforcar", "reforce", "realca", "realcar", "realce", "turbina", "turbinar", "puxa", "puxar" };
    static const std::set<std::string> minus { "menos", "diminui", "diminuir", "diminua", "abaixa", "abaixar", "abaixe", "reduz", "reduzir", "reduza", "alivia", "aliviar", "alivie", "segura", "maneira", "maneirar", "maneire", "tira um pouco" };
    static const std::set<std::string> addv { "adiciona", "adicionar", "adicione", "acrescenta", "acrescentar", "acrescente", "coloca", "colocar", "coloque", "bota", "botar", "bote", "poe", "ponha", "joga", "jogar", "mete", "meter", "insere", "inserir", "liga", "ligar", "ligue", "inclui", "incluir", "inclua", "junta", "juntar" };
    if (plus.count (w)) return 1;
    if (minus.count (w)) return -1;
    if (addv.count (w)) return 2;
    return 0;
}

bool isNegation (const std::string& w)
{
    static const std::set<std::string> s { "sem", "tira", "tirar", "tire", "tirando", "remove", "remover", "remova", "retira", "retirar", "retire", "desliga", "desligar", "desligue", "nada", "nenhum", "nenhuma", "nao", "zero", "chega", "corta", "cortar", "corte" };
    return s.count (w) > 0;
}

bool isResetPhrase (const std::vector<Tok>& t)
{
    auto has = [&] (const char* a, const char* b = nullptr)
    {
        for (size_t i = 0; i < t.size(); ++i)
        {
            if (t[i].w != a) continue;
            if (! b) return true;
            if (i + 1 < t.size() && t[i + 1].w == b) return true;
        }
        return false;
    };
    return has ("zera") || has ("zerar") || has ("reset") || has ("resetar") || has ("recomeca") || has ("recomecar")
        || has ("limpa", "tudo") || has ("tira", "tudo") || has ("apaga", "tudo") || has ("desliga", "tudo") || has ("remove", "tudo")
        || has ("rack", "vazio") || has ("do", "zero") || (has ("sem", "efeito") && t.size() <= 3);
}

const char* divisionWord (const std::string& w, int& sync)
{
    if (w == "seminima" || w == "seminimas" || w == "quarto") { sync = 1; return "1/4"; }
    if (w == "colcheia" || w == "colcheias") { sync = 3; return "1/8"; }
    if (w == "semicolcheia" || w == "semicolcheias") { sync = 6; return "1/16"; }
    if (w == "minima" || w == "minimas") { sync = 7; return "1/2"; }
    return nullptr;
}

struct Match { int ci; int start; int len; };
} // namespace

// ==================================================================== interpretação
BrainResult LocalBrain::interpret (const std::string& text, const Treatment& current) const
{
    BrainResult res;
    std::vector<Tok> toks = tokenize (text);

    if (isResetPhrase (toks))
    {
        res.reset = true; res.confidence = 1;
        res.explanation = "Zerei o rack.";
        res.understood.push_back ("zerar");
        return res;
    }

    // 0) artistas viram o estilo correspondente ("voz estilo Fernandinho" → gospel)
    for (int pos = 0; pos < (int) toks.size(); ++pos)
    {
        int bestLen = 0; const Artist* best = nullptr;
        for (auto& a : artists())
        {
            const auto words = splitWords (a.name);
            const int len = (int) words.size();
            if (len <= bestLen || pos + len > (int) toks.size()) continue;
            bool ok = true;
            for (int j = 0; j < len && ok; ++j)
            {
                const auto& tk = toks[(size_t) (pos + j)];
                const auto& w = words[(size_t) j];
                ok = ! tk.used && (tk.w == w || (! tk.num && w.size() >= 6 && tk.w.size() >= 5 && fuzzyDistance (tk.w, w) <= 1));
            }
            if (ok) { best = &a; bestLen = len; }
        }
        if (! best) continue;
        static const std::map<std::string, std::string> styleLabel {
            { "adoracao", "adoração" }, { "eletronica", "eletrônica" }, { "rnb", "R&B / soul" }, { "mpb", "MPB / acústico" }, { "reggae", "reggae / dub" } };
        const auto sl = styleLabel.find (best->style);
        res.understood.push_back (titleCase (best->name) + " → " + (sl != styleLabel.end() ? sl->second : std::string (best->style)));
        toks[(size_t) pos].w = best->style;
        for (int j = 1; j < bestLen; ++j) toks[(size_t) (pos + j)].used = true;
        pos += bestLen - 1;
    }

    // palavras que existem exatamente no vocabulário não passam pela tolerância
    // ("ratio" não é "radio", "baixo" não é "abaixo")
    static const std::set<std::string> exactWords = []
    {
        std::set<std::string> s;
        for (auto& c : concepts()) for (auto& k : c.split) if (k.size() == 1) s.insert (k[0]);
        for (int t = 1; t < kNumTypes; ++t)
        {
            s.insert (semAcento (info (t).key));
            for (auto& p : info (t).params) if (p.used()) { std::string k = semAcento (p.key); for (auto& ch : k) if (ch == '_') ch = ' '; for (auto& w : splitWords (k)) s.insert (w); }
        }
        s.insert ("ratio");
        return s;
    }();

    // 1) casar conceitos (frases mais longas primeiro)
    const auto& cs = concepts();
    std::vector<Match> matches;
    for (int pos = 0; pos < (int) toks.size(); ++pos)
    {
        if (toks[(size_t) pos].used) continue;
        int bestC = -1, bestLen = 0;
        for (int c = 0; c < (int) cs.size(); ++c)
            for (auto& key : cs[(size_t) c].split)
            {
                const int len = (int) key.size();
                if (len <= bestLen || pos + len > (int) toks.size()) continue;
                bool ok = true;
                for (int j = 0; j < len && ok; ++j)
                {
                    const auto& tk = toks[(size_t) (pos + j)];
                    ok = ! tk.used && (tk.w == key[(size_t) j]
                                       || (! tk.num && key[(size_t) j].size() > 2 && ! exactWords.count (tk.w) && wordEq (tk.w, key[(size_t) j])));
                }
                if (ok) { bestC = c; bestLen = len; }
            }
        if (bestC >= 0)
        {
            // palavras curtas e ambíguas: "baixo" depois de "volume"/"mais" não é instrumento
            const auto& w = toks[(size_t) pos].w;
            const bool ambiguousBaixo = cs[(size_t) bestC].label == std::string ("baixo") && w == "baixo" && pos > 0
                                        && (toks[(size_t) pos - 1].w == "volume" || toks[(size_t) pos - 1].w == "mais" || toks[(size_t) pos - 1].w == "bem");
            if (! ambiguousBaixo)
            {
                matches.push_back ({ bestC, pos, bestLen });
                for (int j = 0; j < bestLen; ++j) toks[(size_t) (pos + j)].used = true;
                pos += bestLen - 1;
            }
        }
    }

    // 2) parâmetros pelo nome ("ratio 4", "ataque 5 ms", "decaimento 3 s")
    struct ParamHit { int type; int param; int pos; };
    std::vector<ParamHit> paramHits;
    for (int pos = 0; pos < (int) toks.size(); ++pos)
    {
        auto& tk = toks[(size_t) pos];
        if (tk.used || tk.num) continue;
        std::string w = tk.w;
        int extraLen = 0;
        if (w == "pre" && pos + 1 < (int) toks.size() && toks[(size_t) pos + 1].w == "delay") { w = "predelay"; extraLen = 1; }
        // número logo depois (até 2 palavras)
        int npos = -1;
        for (int j = pos + 1 + extraLen; j < (int) toks.size() && j <= pos + 3 + extraLen; ++j)
            if (toks[(size_t) j].num && ! toks[(size_t) j].used) { npos = j; break; }
        if (npos < 0) continue;
        for (int t = 1; t < kNumTypes; ++t)
        {
            const int pi = paramFromName ((BT) t, w);
            if (pi < 0) continue;
            const auto& ps = info (t).params[(size_t) pi];
            if (ps.scale == Scale::Choice || ps.scale == Scale::Toggle) continue;
            paramHits.push_back ({ t, pi, pos });
        }
        // marca como usado se achou algum
        bool any = false;
        for (auto& h : paramHits) if (h.pos == pos) any = true;
        if (any) { tk.used = true; if (extraLen) toks[(size_t) pos + 1].used = true; }
    }

    // 3) palavras restantes
    bool genre = false, onlyInstr = true, hasRel = false, hasNeg = false, hasAdd = false;
    for (auto& m : matches)
    {
        const auto cat = cs[(size_t) m.ci].cat;
        if (cat == Genre) genre = true;
        if (cat != Instr) onlyInstr = false;
    }
    if (! paramHits.empty()) onlyInstr = false;
    for (auto& tk : toks)
    {
        if (tk.used || tk.num) continue;
        const int r = relVerb (tk.w);
        if (r == 2) hasAdd = true; else if (r != 0) hasRel = true;
        if (isNegation (tk.w)) hasNeg = true;
    }
    const bool refine = ! current.empty() && ! genre && (hasRel || hasNeg || hasAdd || (matches.empty() && ! paramHits.empty()))
                        && ! (onlyInstr && ! matches.empty() && ! hasRel && ! hasNeg && ! hasAdd);
    res.refinement = refine;

    B b;
    if (refine) b.blocks = current.blocks;
    for (auto& m : matches) if (cs[(size_t) m.ci].cat == Instr) b.inst = cs[(size_t) m.ci].label;
    if (b.inst == "violão") b.inst = "violao";

    // instrumento citado: começa pela cadeia base dele, a não ser que o pedido seja um gênero
    // (que já monta a própria cadeia) ou um efeito de caráter (telefone, rádio, água...)
    bool character = false, explicitWet = false;
    for (auto& m : matches)
    {
        const auto& c = cs[(size_t) m.ci];
        static const std::set<std::string> chars { "rádio antigo", "telefone", "megafone", "lo-fi", "8-bit", "debaixo d'água", "fantasma", "surpresa" };
        if (chars.count (c.label)) character = true;
        if (c.cat == Effect && (c.primary == BT::Reverb || c.primary == BT::Delay || c.primary == BT::Chorus)) explicitWet = true;
    }
    if (! b.inst.empty() && ! refine && ! genre && ! character) applyInstrumentBase (b, b.inst);

    // 4) intensidade de cada conceito
    std::vector<float> intensity (matches.size(), 1.0f);
    std::vector<int> sign (matches.size(), 0);      // +1 / -1 / +2 (adicionar)
    std::vector<bool> negated (matches.size(), false);
    for (int i = 0; i < (int) toks.size(); ++i)
    {
        auto& tk = toks[(size_t) i];
        if (tk.used) continue;
        const float mv = tk.num ? 0.0f : modifierValue (tk.w);
        const int rv = tk.num ? 0 : relVerb (tk.w);
        const bool ng = ! tk.num && isNegation (tk.w);
        if (mv == 0 && rv == 0 && ! ng) continue;
        // alvo: próximo conceito em até 3 palavras; senão o anterior em até 2
        int target = -1;
        for (int m = 0; m < (int) matches.size(); ++m)
            if (matches[(size_t) m].start > i && matches[(size_t) m].start - i <= 3) { target = m; break; }
        if (target < 0 && mv != 0)
            for (int m = (int) matches.size() - 1; m >= 0; --m)
            {
                const int end = matches[(size_t) m].start + matches[(size_t) m].len - 1;
                if (end < i && i - end <= 2) { target = m; break; }
            }
        if (target < 0) continue;
        tk.used = true;
        if (mv != 0) intensity[(size_t) target] *= mv;
        if (rv != 0 && sign[(size_t) target] == 0) sign[(size_t) target] = rv;
        if (ng) negated[(size_t) target] = true;
    }
    // "tira um pouco": negação com intensidade fraca = diminuir, não remover
    for (size_t m = 0; m < matches.size(); ++m)
        if (negated[m] && intensity[m] < 0.9f) { negated[m] = false; sign[m] = -1; }

    // 5) aplicar
    std::vector<int> consumedNum;
    auto findNumber = [&] (const Match& m, Num& out) -> bool
    {
        auto take = [&] (int j) -> bool
        {
            auto& tk = toks[(size_t) j];
            if (! tk.num || tk.used) return false;
            out.v = tk.v; out.unit = tk.unit; tk.used = true;
            // "1/4 pontuado"
            if (tk.unit == "frac" && j + 1 < (int) toks.size())
            {
                const auto& nx = toks[(size_t) j + 1].w;
                if (nx == "pontuado" || nx == "pontuada" || nx == "dotted") { out.extra = "pont"; toks[(size_t) j + 1].used = true; }
                if (nx == "tercina" || nx == "triplet" || nx == "quialtera") { out.extra = "terc"; toks[(size_t) j + 1].used = true; }
            }
            return true;
        };
        const int end = m.start + m.len;
        for (int j = end; j < (int) toks.size() && j < end + 4; ++j)
        {
            if (toks[(size_t) j].num) { if (take (j)) return true; break; }
            // para no próximo conceito
            bool isConcept = false;
            for (auto& mm : matches) if (mm.start == j) isConcept = true;
            if (isConcept) break;
        }
        for (int j = m.start - 1; j >= 0 && j >= m.start - 3; --j)
            if (toks[(size_t) j].num) { return take (j); }
        return false;
    };

    for (size_t mi = 0; mi < matches.size(); ++mi)
    {
        const auto& m = matches[mi];
        const auto& c = cs[(size_t) m.ci];
        std::string label = c.label;
        if (intensity[mi] > 1.9f) label += " (extremo)";
        else if (intensity[mi] > 1.2f) label += " (bem)";
        else if (intensity[mi] < 0.8f) label += " (pouco)";
        if (c.cat == Instr) { res.understood.push_back (label); continue; }

        if (negated[mi])
        {
            if (! c.removes.empty())
            {
                for (BT t : c.removes) { b.remove (t); b.forbid.insert ((int) t); }
                res.understood.push_back ("sem " + std::string (c.label));
                continue;
            }
            sign[mi] = -1;
        }
        float k = intensity[mi];
        if (sign[mi] == -1) k = refine ? -0.6f * intensity[mi] : -intensity[mi];
        else if (sign[mi] == 1 && refine) k = 0.6f * intensity[mi];
        if (sign[mi] == -1) label = "menos " + std::string (c.label);
        else if (sign[mi] == 1 && refine) label = "mais " + std::string (c.label);

        if (c.apply) c.apply (b, k);
        res.understood.push_back (label);

        // números ligados ao conceito
        Num n;
        if (c.primary == BT::Delay || c.label == std::string ("delay") || c.label == std::string ("ping-pong") || c.label == std::string ("slap"))
        {
            // divisão por palavra ("colcheia", "semínima pontuada")
            for (int j = m.start + m.len; j < (int) toks.size() && j < m.start + m.len + 4; ++j)
            {
                int sy = 0;
                if (toks[(size_t) j].used || ! divisionWord (toks[(size_t) j].w, sy)) continue;
                toks[(size_t) j].used = true;
                if (j + 1 < (int) toks.size() && (toks[(size_t) j + 1].w == "pontuada" || toks[(size_t) j + 1].w == "pontuado")) { toks[(size_t) j + 1].used = true; if (sy == 1) sy = 2; else if (sy == 3) sy = 4; }
                b.set (BT::Delay, dl::SYNC, (float) sy);
            }
            if (findNumber (m, n))
            {
                auto& d = b.ensure (BT::Delay);
                if (n.unit == "frac")
                {
                    const float f = n.v; int sy = 1;
                    if (std::abs (f - 0.25f) < 1e-3f) sy = n.extra == "pont" ? 2 : 1;
                    else if (std::abs (f - 0.125f) < 1e-3f) sy = n.extra == "pont" ? 4 : (n.extra == "terc" ? 5 : 3);
                    else if (std::abs (f - 0.0625f) < 1e-3f) sy = 6;
                    else if (std::abs (f - 0.5f) < 1e-3f) sy = 7;
                    d.p[dl::SYNC] = (float) sy;
                }
                else if (n.unit == "%") d.mix = n.v;
                else if (n.unit == "ms" || n.unit == "s" || (n.unit.empty() && n.v >= 10 && n.v <= 2000)) { d.p[dl::TIME] = toMs (n); d.p[dl::SYNC] = 0; }
            }
        }
        else if (c.numeric && findNumber (m, n))
        {
            if (! c.numeric (b, n))
            {   // número não serviu: devolve
                for (auto& tk : toks) if (tk.num && tk.v == n.v && tk.unit == n.unit) { tk.used = false; break; }
            }
        }
    }

    // 6) parâmetros pelo nome
    for (auto& h : paramHits)
    {
        // escolhe o bloco: um que já exista no tratamento, senão o único tipo com esse nome
        int chosen = -1, countTypes = 0;
        for (auto& hh : paramHits) if (hh.pos == h.pos) { ++countTypes; if (b.has ((BT) hh.type) && chosen < 0) chosen = hh.type; }
        if (chosen < 0 && countTypes == 1) chosen = h.type;
        if (chosen != h.type) continue;
        int npos = -1;
        for (int j = h.pos + 1; j < (int) toks.size() && j <= h.pos + 4; ++j) if (toks[(size_t) j].num && ! toks[(size_t) j].used) { npos = j; break; }
        if (npos < 0) continue;
        auto& tk = toks[(size_t) npos]; tk.used = true;
        const auto& ps = info (h.type).params[(size_t) h.param];
        float v = tk.v;
        const std::string unit = ps.unit;
        if (unit == "Hz" && tk.unit == "khz") v *= 1000;
        if (unit == "ms" && tk.unit == "s") v *= 1000;
        if (unit == "s" && tk.unit == "ms") v /= 1000;
        b.set ((BT) h.type, h.param, v);
        std::string nm = ps.name; for (auto& ch : nm) ch = (char) std::tolower ((unsigned char) ch);
        res.understood.push_back (nm + " " + ps.format (ps.clamp (v)));
    }

    // 7) instrumento sozinho → cadeia base do instrumento
    // (a cadeia base já entrou antes dos conceitos)

    // 8) acabamento
    if (! b.inst.empty() && ! b.blocks.empty() && ! refine)
    {
        const std::string& in = b.inst;
        if (in == "voz") { if (b.has (BT::Compressor) || b.has (BT::EQ)) b.atLeast (BT::EQ, eq::HPF, 80); }
        else if (in == "backs") { b.atLeast (BT::EQ, eq::HPF, 140); b.add (BT::Estereo, es::WIDTH, 30); }
        else if (in == "baixo" || in == "bateria")
        {
            b.remove (BT::DeEsser);
            if (auto* e = b.find (BT::EQ)) e->p[eq::HPF] = std::min (e->p[eq::HPF], 35.0f);
            if (auto* e = b.find (BT::Estereo)) e->p[es::MONOLOW] = std::max (e->p[es::MONOLOW], 150.0f);
        }
        else if (in == "master")
        {
            b.remove (BT::DeEsser); b.remove (BT::Gate);
            // estilos montam eco e ambiência pensando em voz; no master só se foi pedido
            if (! explicitWet) { b.remove (BT::Delay); b.remove (BT::Chorus); b.remove (BT::Reverb); }
            if (auto* e = b.find (BT::EQ)) e->p[eq::HPF] = std::min (e->p[eq::HPF], 30.0f);
            if (auto* c = b.find (BT::Compressor)) { c->p[cp::RAT] = std::min (c->p[cp::RAT], 2.5f); c->p[cp::ATK] = std::max (c->p[cp::ATK], 25.0f); c->p[cp::MK] = std::min (c->p[cp::MK], 4.0f); c->p[cp::KNEE] = 10; }
        }
    }
    // presença/seco/distante: quanto de ambiência
    if (std::abs (b.dryBias) > 1e-3f)
    {
        const float s = std::clamp (1 - 0.45f * b.dryBias, 0.25f, 1.8f);
        for (BT t : { BT::Reverb, BT::Delay })
            if (auto* x = b.find (t); x && b.touched.count ((int) t)) x->mix *= s;
    }
    for (int t : b.forbid) b.remove ((BT) t);
    // efeitos "molhados" sem mistura saem; o resto é limitado à faixa
    b.blocks.erase (std::remove_if (b.blocks.begin(), b.blocks.end(), [] (const BlockState& x) { return isWet (x.bt()) && x.mix < 1.5f; }), b.blocks.end());
    for (auto& x : b.blocks)
    {
        x.clampAll();
        if (x.bt() == BT::Estereo && std::abs (x.p[es::WIDTH] - 100) < 0.5f && x.p[es::MONOLOW] <= 20.5f && std::abs (x.p[es::PAN]) < 0.5f && x.p[es::HAAS] < 0.05f)
            x.type = -1; // estéreo neutro não precisa ocupar slot
    }
    b.blocks.erase (std::remove_if (b.blocks.begin(), b.blocks.end(), [] (const BlockState& x) { return x.type < 0; }), b.blocks.end());
    // no máximo 8 blocos
    static const BT dropOrder[] = { BT::Estereo, BT::Tremolo, BT::Modulacao, BT::Gate, BT::Filtro, BT::Chorus, BT::Limitador, BT::Saturacao, BT::DeEsser, BT::LoFi };
    for (BT t : dropOrder) if ((int) b.blocks.size() > kNumSlots) b.remove (t);
    while ((int) b.blocks.size() > kNumSlots) b.blocks.pop_back();

    res.treatment.blocks = b.blocks;

    // rótulos sem repetição ("Travis Scott → trap" já diz "trap")
    {
        std::vector<std::string> u;
        for (auto& x : res.understood)
        {
            bool dup = std::find (u.begin(), u.end(), x) != u.end();
            for (auto& y : u) { const auto p = y.find (" → "); if (p != std::string::npos && y.substr (p + 5) == x) dup = true; }
            if (! dup) u.push_back (x);
        }
        res.understood = u;
    }

    // 9) palavras não entendidas e confiança
    const auto& stop = stopwords();
    int recognized = 0;
    for (auto& m : matches) recognized += m.len;
    recognized += (int) paramHits.size();
    for (auto& tk : toks)
    {
        if (tk.used || tk.num) continue;
        if (stop.count (tk.w) || modifierValue (tk.w) != 0 || relVerb (tk.w) != 0 || isNegation (tk.w)) continue;
        int sy; if (divisionWord (tk.w, sy)) continue;
        if (tk.w.size() <= 1) continue;
        if (tk.w == "pontuado" || tk.w == "pontuada" || tk.w == "tercina") continue;
        res.unknown.push_back (tk.w);
    }
    const int content = recognized + (int) res.unknown.size();
    res.confidence = content > 0 ? (float) recognized / (float) content : 0.0f;
    if (matches.empty() && paramHits.empty()) res.confidence = 0;

    // 10) explicação
    std::string ex;
    if (! res.recognized())
        ex = "Não entendi esse pedido. Tente algo como: \"voz quente, na cara, com reverb de igreja\".";
    else
    {
        std::string und;
        for (size_t i = 0; i < res.understood.size(); ++i) { if (i) und += " · "; und += res.understood[i]; }
        ex = "Entendi: " + und + ". ";
        if (refine) ex += describeChanges (current, res.treatment);
        else ex += res.treatment.empty() ? "O rack ficou vazio." : "Montei: " + res.treatment.chainText() + ".";
    }
    if (! res.unknown.empty())
    {
        ex += " (Não entendi: ";
        for (size_t i = 0; i < res.unknown.size() && i < 5; ++i) { if (i) ex += ", "; ex += res.unknown[i]; }
        ex += ".)";
    }
    res.explanation = ex;
    return res;
}
} // namespace wr
