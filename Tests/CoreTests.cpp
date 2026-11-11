// WYATT RACK — testes do motor de áudio e do cérebro offline (sem JUCE)
#include "../Source/Engine/Rack.h"
#include "../Source/Engine/Dsp.h"
#include "../Source/Brain/LocalBrain.h"
#include "../Source/Brain/Macros.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace wr;
static int passed = 0, failed = 0;
#define CHECK(cond, ...) do { if (cond) { ++passed; } else { ++failed; std::printf ("  FALHOU: " __VA_ARGS__); std::printf ("   [%s:%d]\n", __FILE__, __LINE__); } } while (0)

// ------------------------------------------------------------------ rack de teste com parâmetros próprios
struct TestRack
{
    Rack rack;
    std::atomic<float> type[kNumSlots], on[kNumSlots], mix[kNumSlots], p[kNumSlots][kNumParams];
    std::atomic<float> inDb { 0 }, outDb { 0 }, byp { 0 };
    std::atomic<float> mval[kNumMacros];
    double sr;

    explicit TestRack (double s = 48000, int block = 256) : sr (s)
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            type[i] = 0; on[i] = 1; mix[i] = 100;
            auto& sp = rack.slots[(size_t) i].params;
            sp.type = &type[i]; sp.on = &on[i]; sp.mix = &mix[i];
            for (int k = 0; k < kNumParams; ++k) { p[i][k] = 0; sp.p[k] = &p[i][k]; }
        }
        rack.inGainDb = &inDb; rack.outGainDb = &outDb; rack.bypass = &byp;
        for (int m = 0; m < kNumMacros; ++m) { mval[m] = 50; rack.macros.value[m] = &mval[m]; }
        rack.prepare (sr, block);
    }
    void setBlock (int slot, const BlockState& b)
    {
        type[slot] = (float) b.type; on[slot] = b.on ? 1.0f : 0.0f; mix[slot] = b.mix;
        const auto& bi = info (b.type);
        for (int k = 0; k < kNumParams; ++k)
            p[slot][k] = bi.params[(size_t) k].used() ? bi.params[(size_t) k].toNorm (b.p[(size_t) k]) : 0.0f;
    }
    void setTreatment (const Treatment& t)
    {
        for (int i = 0; i < kNumSlots; ++i) type[i] = 0;
        for (size_t i = 0; i < t.blocks.size() && i < (size_t) kNumSlots; ++i) setBlock ((int) i, t.blocks[i]);
        rack.macros.clear();
        for (int m = 0; m < kNumMacros; ++m)
        {
            mval[m] = t.macros[(size_t) m].value * 100.0f;
            for (auto& tg : t.macros[(size_t) m].targets)
            {
                auto& a = rack.macros.depth[m][tg.block][tg.param == kMacroMix ? kNumParams : tg.param];
                a.store (a.load() + tg.depth);
            }
        }
    }
    void run (std::vector<float>& L, std::vector<float>& R, int block = 256, double bpm = 120)
    {
        Ctx c; c.bpm = bpm;
        for (size_t off = 0; off < L.size(); off += (size_t) block)
        {
            const int n = (int) std::min ((size_t) block, L.size() - off);
            rack.process (L.data() + off, R.data() + off, n, c);
        }
    }
};

static std::vector<float> sine (double f, double sr, double secs, float amp = 0.5f)
{
    std::vector<float> v ((size_t) (sr * secs));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2 * dsp::kPi * f * (double) i / sr);
    return v;
}
static std::vector<float> music (double sr, double secs)
{   // seno + ruído + transientes
    std::vector<float> v ((size_t) (sr * secs));
    dsp::Noise nz;
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = (double) i / sr;
        const double env = std::exp (-8 * std::fmod (t, 0.5));
        v[i] = (float) (0.3 * std::sin (2 * dsp::kPi * 220 * t) + 0.15 * std::sin (2 * dsp::kPi * 1760 * t) + 0.25 * env * nz.next());
    }
    return v;
}
static float peak (const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0) to = v.size();
    float p = 0; for (size_t i = from; i < to; ++i) p = std::max (p, std::abs (v[i])); return p;
}
static float rms (const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0) to = v.size();
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i]; return (float) std::sqrt (s / (double) std::max<size_t> (1, to - from));
}
static bool finite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }
static float levelAt (BlockState b, double f, double sr = 48000)
{
    TestRack tr (sr); tr.setBlock (0, b);
    auto L = sine (f, sr, 1.0, 0.1f), R = L;
    tr.run (L, R);
    return dsp::gainToDb (rms (L, L.size() / 2) / (0.1f / std::sqrt (2.0f)));
}

// ------------------------------------------------------------------ motor
static void engineTests()
{
    std::printf ("== Motor\n");
    const double sr = 48000;
    {   // rack vazio = idêntico, bit a bit
        TestRack tr (sr);
        auto L = music (sr, 1), R = L; const auto L0 = L, R0 = R;
        tr.run (L, R);
        CHECK (L == L0 && R == R0, "rack vazio deveria devolver o áudio idêntico\n");
    }
    for (int t = 1; t < kNumTypes; ++t)
    {
        const auto& bi = info (t);
        // padrão: estável e limitado
        {
            TestRack tr (sr); tr.setBlock (0, BlockState::defaults ((BlockType) t));
            auto L = music (sr, 3), R = L;
            tr.run (L, R);
            CHECK (finite (L) && finite (R), "%s: saída com NaN/inf\n", bi.name);
            CHECK (peak (L) < 8 && peak (R) < 8, "%s: saída explodiu (pico %.2f)\n", bi.name, peak (L));
        }
        // valores extremos (todos os parâmetros no máximo e no mínimo)
        for (float edge : { 0.0f, 1.0f })
        {
            TestRack tr (sr); auto b = BlockState::defaults ((BlockType) t);
            for (int k = 0; k < kNumParams; ++k) if (bi.params[(size_t) k].used()) b.p[(size_t) k] = bi.params[(size_t) k].toReal (edge);
            tr.setBlock (0, b);
            auto L = music (sr, 3), R = L;
            tr.run (L, R);
            CHECK (finite (L) && finite (R) && peak (L) < 30, "%s: instável com tudo em %.0f%% (pico %.2f)\n", bi.name, edge * 100, peak (L));
        }
        // mix 0 = seco idêntico
        {
            TestRack tr (sr); auto b = BlockState::defaults ((BlockType) t); b.mix = 0; tr.setBlock (0, b);
            auto L = music (sr, 1), R = L; const auto L0 = L;
            tr.run (L, R);
            CHECK (L == L0, "%s: mix 0%% deveria ser o seco idêntico\n", bi.name);
        }
        // desligado = seco idêntico
        {
            TestRack tr (sr); auto b = BlockState::defaults ((BlockType) t); b.on = false; tr.setBlock (0, b);
            auto L = music (sr, 1), R = L; const auto L0 = L;
            tr.run (L, R);
            CHECK (L == L0, "%s: desligado deveria ser o seco idêntico\n", bi.name);
        }
    }
    // EQ
    {
        auto b = BlockState::defaults (BlockType::EQ); b.p[1] = 12;
        const float g100 = levelAt (b, 60), g5k = levelAt (b, 5000);
        CHECK (g100 > 9 && std::abs (g5k) < 1.0f, "EQ grave +12: 60 Hz %+.1f dB, 5 kHz %+.1f dB\n", g100, g5k);
        b = BlockState::defaults (BlockType::EQ); b.p[0] = 200;
        CHECK (levelAt (b, 50) < -15, "EQ corte grave 200 Hz deveria derrubar 50 Hz (%.1f dB)\n", levelAt (b, 50));
        b = BlockState::defaults (BlockType::EQ);
        CHECK (std::abs (levelAt (b, 1000)) < 0.1f, "EQ neutro deveria ser 0 dB\n");
    }
    // Compressor reduz um sinal alto
    {
        auto b = BlockState::defaults (BlockType::Compressor); b.p[0] = -30; b.p[1] = 8; b.p[4] = 0;
        TestRack tr (sr); tr.setBlock (0, b);
        auto L = sine (1000, sr, 1, 0.7f), R = L; tr.run (L, R);
        const float red = dsp::gainToDb (rms (L, L.size() / 2) / (0.7f / std::sqrt (2.0f)));
        CHECK (red < -12, "compressor -30 dB 8:1 deveria reduzir mais de 12 dB (%.1f)\n", red);
    }
    // Limitador segura o teto
    {
        auto b = BlockState::defaults (BlockType::Limitador); b.p[0] = -3; b.p[1] = 12;
        TestRack tr (sr); tr.setBlock (0, b);
        auto L = music (sr, 2), R = L; tr.run (L, R);
        const float pk = dsp::gainToDb (peak (L, L.size() / 4));
        CHECK (pk <= -3 + 0.3f, "limitador teto -3 dB: pico %.2f dB\n", pk);
    }
    // Delay sincronizado: 1/4 a 120 BPM = 500 ms
    {
        auto b = BlockState::defaults (BlockType::Delay); b.p[1] = 1; b.p[2] = 0; b.mix = 100; b.p[3] = 20000; b.p[5] = 0;
        TestRack tr (sr); tr.setBlock (0, b);
        std::vector<float> L ((size_t) sr, 0.0f), R = L; L[100] = R[100] = 1;
        tr.run (L, R, 256, 120);
        size_t at = 0; float best = 0;
        for (size_t i = 200; i < L.size(); ++i) if (std::abs (L[i]) > best) { best = std::abs (L[i]); at = i; }
        CHECK (std::abs ((double) (at - 100) - 0.5 * sr) < 0.002 * sr, "delay 1/4 a 120 BPM: eco em %.1f ms\n", (double) (at - 100) / sr * 1000);
    }
    // Reverb tem cauda e ela some
    {
        auto b = BlockState::defaults (BlockType::Reverb); b.mix = 100; b.p[2] = 2;
        TestRack tr (sr); tr.setBlock (0, b);
        std::vector<float> L ((size_t) (sr * 6), 0.0f), R = L;
        for (int i = 0; i < 2400; ++i) L[(size_t) i] = R[(size_t) i] = 0.5f * (float) std::sin (i * 0.3);
        tr.run (L, R);
        const float tail = rms (L, (size_t) (sr * 0.5), (size_t) (sr * 0.8)), late = rms (L, (size_t) (sr * 5.5));
        CHECK (tail > 1e-3f && late < tail * 0.02f, "reverb: cauda %.5f, final %.6f\n", tail, late);
    }
    // Troca de tipo sem clique
    {
        TestRack tr (sr); tr.setBlock (0, BlockState::defaults (BlockType::Saturacao));
        auto L = sine (220, sr, 1, 0.3f), R = L;
        Ctx c;
        float maxJump = 0, prev = 0;
        for (size_t off = 0; off < L.size(); off += 256)
        {
            if (off == 24000) tr.setBlock (0, BlockState::defaults (BlockType::Chorus));
            const size_t n = std::min<size_t> (256, L.size() - off);
            tr.rack.process (L.data() + off, R.data() + off, (int) n, c);
            for (size_t i = off; i < off + n; ++i) { if (off >= 12000) maxJump = std::max (maxJump, std::abs (L[i] - prev)); prev = L[i]; }
        }
        const float sineStep = 0.3f * (float) (2 * dsp::kPi * 220 / sr) * 2.5f;
        CHECK (maxJump < sineStep * 2.0f, "troca de bloco com salto %.4f (limite %.4f)\n", maxJump, sineStep * 2);
    }
    // Bypass geral = seco
    {
        TestRack tr (sr); tr.setBlock (0, BlockState::defaults (BlockType::Saturacao)); tr.byp = 1; tr.rack.reset();
        auto L = music (sr, 1), R = L; const auto L0 = L; tr.run (L, R);
        CHECK (L == L0, "bypass deveria devolver o seco\n");
    }
    // 44,1 kHz, 96 kHz e blocos de tamanhos variados
    for (double s2 : { 44100.0, 96000.0 })
        for (int blk : { 1, 64, 1024 })
        {
            TestRack tr (s2, blk);
            const BlockType all[] = { BlockType::EQ, BlockType::Compressor, BlockType::Saturacao, BlockType::Chorus, BlockType::Delay, BlockType::Reverb, BlockType::Estereo, BlockType::Limitador };
            for (int i = 0; i < 8; ++i) tr.setBlock (i, BlockState::defaults (all[i]));
            auto L = music (s2, 1), R = L; tr.run (L, R, blk);
            CHECK (finite (L) && peak (L) < 4, "%.0f Hz bloco %d: instável\n", s2, blk);
        }
    // CPU: 8 slots pesados
    {
        TestRack tr (sr);
        const BlockType heavy[] = { BlockType::Reverb, BlockType::Reverb, BlockType::Chorus, BlockType::Modulacao, BlockType::Delay, BlockType::Compressor, BlockType::Saturacao, BlockType::LoFi };
        for (int i = 0; i < 8; ++i) tr.setBlock (i, BlockState::defaults (heavy[i]));
        auto L = music (sr, 10), R = L;
        const auto t0 = std::chrono::steady_clock::now(); tr.run (L, R, 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  CPU com 8 blocos pesados: %.1f%% de um núcleo\n", secs / 10 * 100);
        CHECK (secs / 10 < 0.25, "CPU alta demais (%.1f%%)\n", secs / 10 * 100);
    }
}

// ------------------------------------------------------------------ cérebro
static void brainTests()
{
    std::printf ("== Cérebro offline\n");
    LocalBrain br;
    auto ask = [&] (const char* t, const Treatment& cur = {}) { return br.interpret (t, cur); };
    auto has = [] (const BrainResult& r, BlockType t) { return r.treatment.find (t) != nullptr; };

    {
        auto r = ask ("voz quente, na cara, com reverb de igreja");
        auto* rv = r.treatment.find (BlockType::Reverb);
        CHECK (has (r, BlockType::EQ) && has (r, BlockType::Compressor) && rv && (int) rv->p[0] == 1, "voz quente/igreja: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("delay 1/8 pontuado ping pong");
        auto* d = r.treatment.find (BlockType::Delay);
        CHECK (d && (int) d->p[1] == 4 && d->p[4] >= 0.5f, "delay 1/8 pont. ping-pong: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("compressor com ratio 8 e ataque 5 ms");
        auto* c = r.treatment.find (BlockType::Compressor);
        CHECK (c && std::abs (c->p[1] - 8) < 0.01f && std::abs (c->p[2] - 5) < 0.01f && ! has (r, BlockType::LoFi), "ratio 8 / ataque 5: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("corta grave em 120 hz");
        auto* e = r.treatment.find (BlockType::EQ);
        CHECK (e && std::abs (e->p[0] - 120) < 0.5f, "corte em 120 Hz: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("limitador teto -0,5 db");
        auto* l = r.treatment.find (BlockType::Limitador);
        CHECK (l && std::abs (l->p[0] + 0.5f) < 0.01f, "teto -0,5: %s (teto %.2f)\n", r.explanation.c_str(), l ? l->p[0] : 99.0f);
    }
    {
        auto r = ask ("baixo gordo");
        CHECK (has (r, BlockType::Compressor) && ! has (r, BlockType::DeEsser), "baixo gordo: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("master pra gospel com pressão");
        CHECK (! has (r, BlockType::Delay) && ! has (r, BlockType::Reverb) && has (r, BlockType::Compressor), "master gospel sem eco: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("master com reverb de sala bem pouco");
        CHECK (has (r, BlockType::Reverb), "master com reverb pedido: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("telefone");
        CHECK (has (r, BlockType::LoFi) || has (r, BlockType::EQ), "telefone: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("reverbe de igrja");
        CHECK (has (r, BlockType::Reverb), "erros de digitação: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("voz do fernandinho");
        CHECK (r.confidence > 0.99f && has (r, BlockType::Reverb), "artista Fernandinho: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("voz estilo Travis Scott");
        CHECK (r.confidence > 0.99f && has (r, BlockType::Delay), "artista Travis Scott: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("pregação limpa");
        CHECK (! has (r, BlockType::Reverb) && has (r, BlockType::Compressor), "pregação sem reverb: %s\n", r.explanation.c_str());
    }
    {   // refinamentos
        auto base = ask ("voz pop com reverb").treatment;
        const float m0 = base.find (BlockType::Reverb)->mix;
        auto r = ask ("mais reverb", base);
        CHECK (r.refinement && r.treatment.find (BlockType::Reverb)->mix > m0, "mais reverb\n");
        r = ask ("menos reverb", base);
        CHECK (r.refinement && r.treatment.find (BlockType::Reverb)->mix < m0, "menos reverb\n");
        r = ask ("sem reverb", base);
        CHECK (r.refinement && ! has (r, BlockType::Reverb) && has (r, BlockType::Compressor), "sem reverb: %s\n", r.explanation.c_str());
        r = ask ("zera tudo", base);
        CHECK (r.reset, "zerar\n");
    }
    {
        auto r = ask ("me surpreenda");
        CHECK (r.treatment.blocks.size() >= 3, "me surpreenda: %s\n", r.explanation.c_str());
    }
    {
        auto r = ask ("blablabla xyz");
        CHECK (! r.recognized() && r.confidence == 0, "frase sem sentido deveria não ser entendida\n");
    }
    {   // toda frase vira tratamento válido (no máximo 8 blocos, valores dentro da faixa)
        const char* frases[] = { "voz trap", "bateria suja e larga", "violão aberto e espacial", "adlib com eco", "efeito fantasma bem extremo",
            "som de igreja dentro de um rádio debaixo d'água", "anos 80 sonhador", "guitarra rock", "teclado gospel com chorus",
            "voz sertanejo", "funk", "8 bit", "megafone", "phaser lento", "tremolo 1/8", "estéreo bem largo", "backs de adoração" };
        int ok = 0, total = 0;
        for (auto* f : frases)
        {
            auto r = ask (f); ++total;
            bool valid = r.recognized() && ! r.treatment.empty() && (int) r.treatment.blocks.size() <= kNumSlots;
            for (auto& b : r.treatment.blocks)
            {
                auto c = b; c.clampAll();
                if (! (c == b) || b.type <= 0) valid = false;
            }
            if (valid) ++ok; else std::printf ("  frase inválida: %s → %s\n", f, r.explanation.c_str());
        }
        CHECK (ok == total, "%d de %d frases deram tratamento válido\n", ok, total);
    }
}

static void macroTests()
{
    std::printf ("== Macros\n");
    LocalBrain brain;
    const double sr = 48000;

    // 1) macros automáticas a partir do pedido
    auto t = brain.interpret ("voz quente, na cara, com reverb de igreja", {}).treatment;
    autoMacros (t, "voz quente, na cara, com reverb de igreja");
    std::string names;
    int used = 0;
    for (auto& m : t.macros) if (m.used()) { ++used; names += m.name + " "; }
    CHECK (used >= 3, "voz quente/igreja deveria ter 3 ou 4 macros (%s)\n", names.c_str());
    CHECK (names.find ("ESPAÇO") != std::string::npos && names.find ("PRESSÃO") != std::string::npos, "macros de espaço e pressão (%s)\n", names.c_str());
    CHECK (names.find ("CALOR") != std::string::npos, "\"quente\" puxa CALOR (%s)\n", names.c_str());
    bool valid = true;
    for (auto& m : t.macros) for (auto& tg : m.targets)
        if (tg.block < 0 || tg.block >= (int) t.blocks.size() || ! macroCanTarget (t.blocks[(size_t) tg.block].type, tg.param)) valid = false;
    CHECK (valid, "todos os alvos apontam para blocos e parâmetros contínuos\n");
    {
        auto lofi = brain.interpret ("lo-fi", {}).treatment; autoMacros (lofi, "lo-fi");
        bool suj = false; for (auto& m : lofi.macros) if (m.name == "SUJEIRA") suj = true;
        CHECK (suj, "lo-fi ganha SUJEIRA\n");
        auto master = brain.interpret ("master gospel com pressão", {}).treatment; autoMacros (master, "master gospel com pressão");
        bool pres = false, esp = false; for (auto& m : master.macros) { if (m.name == "PRESSÃO") pres = true; if (m.name == "ESPAÇO") esp = true; }
        CHECK (pres && ! esp, "master: PRESSÃO sim, ESPAÇO não (sem reverb no master)\n");
        Treatment vazio; autoMacros (vazio, "qualquer");
        CHECK (! vazio.hasMacros(), "rack vazio não tem macros\n");
    }

    // 2) no meio (50%) o som é idêntico ao tratamento sem macros
    {
        Treatment semMacro = t; semMacro.macros = {};
        TestRack a (sr), b (sr);
        a.setTreatment (t); b.setTreatment (semMacro);
        auto L1 = sine (220, sr, 1.0), R1 = L1, L2 = L1, R2 = L1;
        a.run (L1, R1); b.run (L2, R2);
        bool same = true; for (size_t i = 0; i < L1.size(); ++i) if (L1[i] != L2[i] || R1[i] != R2[i]) { same = false; break; }
        CHECK (same, "macros no meio = som idêntico\n");
    }

    // 3) girar a macro = mesmo som que o tratamento "fixado" (baked) sem macros
    {
        Treatment girado = t;
        for (auto& m : girado.macros) m.value = 0.85f;
        girado.macros[0].value = 0.1f;
        const Treatment fixado = girado.baked();
        CHECK (! fixado.hasMacros() && ! (fixado == girado), "fixar tira as macros e muda os valores\n");
        TestRack a (sr), b (sr);
        a.setTreatment (girado); b.setTreatment (fixado);
        auto L1 = sine (220, sr, 1.0), R1 = L1, L2 = L1, R2 = L1;
        a.run (L1, R1); b.run (L2, R2);
        float maxd = 0; for (size_t i = 0; i < L1.size(); ++i) maxd = std::max (maxd, std::abs (L1[i] - L2[i]));
        CHECK (maxd < 1e-4f, "macro girada soa igual ao tratamento fixado (dif. %.6f)\n", maxd);
        TestRack c (sr); Treatment semMacro = t; semMacro.macros = {}; c.setTreatment (semMacro);
        auto L3 = sine (220, sr, 1.0), R3 = L3; c.run (L3, R3);
        float d2 = 0; for (size_t i = 0; i < L1.size(); ++i) d2 = std::max (d2, std::abs (L1[i] - L3[i]));
        CHECK (d2 > 1e-3f, "girar a macro muda o som de verdade\n");
    }

    // 4) valores: PESO no máximo sobe o grave do EQ 6 dB
    {
        Treatment eq; eq.blocks.push_back (BlockState::defaults (BlockType::EQ));
        autoMacros (eq, "grave");
        int peso = -1; for (int m = 0; m < kNumMacros; ++m) if (eq.macros[(size_t) m].name == "PESO") peso = m;
        CHECK (peso >= 0, "EQ sozinho com \"grave\" tem PESO\n");
        if (peso >= 0)
        {
            eq.macros[(size_t) peso].value = 1.0f;
            const auto f = eq.baked();
            CHECK (std::abs (f.blocks[0].p[1] - 6.0f) < 0.01f, "PESO 100%% → grave +6 dB (%.2f)\n", f.blocks[0].p[1]);
            eq.macros[(size_t) peso].value = 0.0f;
            CHECK (std::abs (eq.baked().blocks[0].p[1] + 6.0f) < 0.01f, "PESO 0%% → grave −6 dB\n");
        }
    }

    // 5) limites: macro nunca empurra parâmetro para fora da faixa
    {
        Treatment r; auto rv = BlockState::defaults (BlockType::Reverb); rv.mix = 95; r.blocks.push_back (rv);
        r.macros[0] = { "ESPAÇO", 1.0f, { { 0, kMacroMix, 0.5f }, { 0, 2, 0.9f } } };
        const auto f = r.baked();
        CHECK (f.blocks[0].mix <= 100.0f && f.blocks[0].p[2] <= 12.0f, "mix e decaimento ficam na faixa\n");
        r.macros[1] = { "RUIM", 0.5f, { { 0, 0, 0.3f }, { 7, 1, 0.3f } } };   // TIPO (escolha) e bloco inexistente
        r.sanitizeMacros();
        CHECK (! r.macros[1].used(), "alvos inválidos são descartados (escolha, bloco que não existe)\n");
    }

    // 6) reorganizar a cadeia leva os alvos junto
    {
        Treatment m = t;
        const int b0 = m.macros[0].targets.empty() ? -1 : m.macros[0].targets[0].block;
        const int type0 = b0 >= 0 ? m.blocks[(size_t) b0].type : -1;
        std::vector<int> rev (m.blocks.size());
        for (size_t i = 0; i < rev.size(); ++i) rev[i] = (int) (rev.size() - 1 - i);
        std::vector<BlockState> nb (m.blocks.rbegin(), m.blocks.rend());
        m.blocks = nb; m.remapMacros (rev);
        CHECK (b0 >= 0 && m.blocks[(size_t) m.macros[0].targets[0].block].type == type0, "alvo segue o bloco depois de inverter a cadeia\n");
        std::vector<int> drop (m.blocks.size(), 0);
        for (size_t i = 0; i < drop.size(); ++i) drop[i] = (int) i;
        const int victim = m.macros[0].targets[0].block;
        drop[(size_t) victim] = -1;
        m.remapMacros (drop);
        bool gone = true; for (auto& mm : m.macros) for (auto& tg : mm.targets) if (tg.block == victim) gone = false;
        CHECK (gone, "bloco que saiu não tem mais alvo de macro\n");
    }

    // 7) automação da macro sem clique
    {
        Treatment eq; eq.blocks.push_back (BlockState::defaults (BlockType::EQ)); eq.blocks.push_back (BlockState::defaults (BlockType::Saturacao));
        autoMacros (eq, "peso brilho calor");
        TestRack a (sr); a.setTreatment (eq);
        auto L = sine (110, sr, 1.0, 0.4f), R = L;
        Ctx c; c.bpm = 120;
        float steadyMax = 0, jumpMax = 0, afterMax = 0;
        for (size_t off = 0, k = 0; off < L.size(); off += 256, ++k)
        {
            if (k == 100) for (int m = 0; m < kNumMacros; ++m) a.mval[m] = 100;   // salto instantâneo do knob
            const int n = (int) std::min<size_t> (256, L.size() - off);
            a.rack.process (L.data() + off, R.data() + off, n, c);
            for (int i = 1; i < n; ++i)
            {
                const float d = std::abs (L[off + (size_t) i] - L[off + (size_t) i - 1]);
                if (k >= 60 && k < 100) steadyMax = std::max (steadyMax, d);
                if (k >= 100 && k < 110) jumpMax = std::max (jumpMax, d);
                if (k >= 150 && k < 180) afterMax = std::max (afterMax, d);
            }
        }
        CHECK (afterMax > steadyMax * 1.2f, "macros no máximo mudaram o som (%.4f → %.4f)\n", steadyMax, afterMax);
        CHECK (jumpMax < std::max (steadyMax, afterMax) * 1.3f, "salto de macro sem clique (passo %.4f; parado antes %.4f, depois %.4f)\n", jumpMax, steadyMax, afterMax);
    }
}

int main()
{
    engineTests();
    brainTests();
    macroTests();
    std::printf ("\n%d verificações OK, %d falharam\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
