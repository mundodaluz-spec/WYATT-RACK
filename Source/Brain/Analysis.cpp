#include "Analysis.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace wr
{
static constexpr double kPi = 3.14159265358979323846;
static float db (double pow) { return (float) (10.0 * std::log10 (std::max (pow, 1e-20))); }

float TrackAnalyzer::Bq::run (float x)
{
    const double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return (float) y;
}

static void butter (double fc, double sr, bool high, double& b0, double& b1, double& b2, double& a1, double& a2)
{
    const double w = 2 * kPi * std::min (fc, sr * 0.45) / sr, c = std::cos (w), a = std::sin (w) / (2 * 0.7071), a0 = 1 + a;
    if (high) { b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = (1 + c) / 2 / a0; }
    else      { b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0;  b2 = (1 - c) / 2 / a0; }
    a1 = -2 * c / a0; a2 = (1 - a) / a0;
}

void TrackAnalyzer::prepare (double s)
{
    sr = s;
    // bordas: graves <150 | 150–500 | 500–2k | 2k–6k | >6k   (0 = sem filtro nessa borda)
    const double lo[kNumBands] = { 0, 150, 500, 2000, 6000 };
    const double hi[kNumBands] = { 150, 500, 2000, 6000, 0 };
    for (int b = 0; b < kNumBands; ++b)
    {
        auto& h = hp[(size_t) b]; auto& l = lp[(size_t) b];
        h = {}; l = {};
        if (lo[b] > 0) butter (lo[b], sr, true, h.b0, h.b1, h.b2, h.a1, h.a2);
        if (hi[b] > 0) butter (hi[b], sr, false, l.b0, l.b1, l.b2, l.a1, l.a2);
    }
    decay = std::exp (-1.0 / (6.0 * sr));   // memória de ~6 s
    reset();
}

void TrackAnalyzer::reset()
{
    for (auto& q : hp) q.z1 = q.z2 = 0;
    for (auto& q : lp) q.z1 = q.z2 = 0;
    for (auto& a : bandAcc) a = 0;
    totAcc = 0; peakHold = 0; voicedSamples = 0; gateEnv = 0;
    for (auto& b : outBand) b.store (0);
    outRms.store (-100); outPeak.store (-100); outSecs.store (0);
}

void TrackAnalyzer::process (const float* L, const float* R, int n)
{
    const float envA = (float) std::exp (-1.0 / (0.05 * sr));
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.5f * (L[i] + (R ? R[i] : L[i]));
        const float ax = std::abs (x);
        gateEnv = std::max (ax, gateEnv * envA);
        peakHold = std::max (peakHold * (1.0 - 1.0 / (10.0 * sr)), (double) ax);
        for (int b = 0; b < kNumBands; ++b)
        {
            const float y = lp[(size_t) b].run (hp[(size_t) b].run (x));
            if (gateEnv >= 0.003f) bandAcc[b] = bandAcc[b] * decay + (double) y * y;
        }
        if (gateEnv < 0.003f) continue;           // ignora silêncio (< −50 dBFS)
        voicedSamples += 1;
        totAcc = totAcc * decay + (double) x * x;
    }
    // publica (barato: uma vez por bloco)
    const double norm = (1.0 - decay);
    outRms.store (db (totAcc * norm));
    outPeak.store (db (peakHold * peakHold));
    outSecs.store ((float) (voicedSamples / sr));
    for (int b = 0; b < kNumBands; ++b) outBand[b].store (db (bandAcc[b]) - db (totAcc));
}

TrackProfile TrackAnalyzer::snapshot() const
{
    TrackProfile p;
    p.seconds = outSecs.load();
    p.rmsDb = outRms.load();
    p.peakDb = outPeak.load();
    for (int b = 0; b < kNumBands; ++b) p.bandDb[(size_t) b] = outBand[b].load();
    // a média exponencial só é confiável depois de alguns segundos; antes, corrige pela fração preenchida
    if (p.seconds > 0 && p.seconds < 6) p.rmsDb -= db (1.0 - std::exp (-p.seconds / 6.0));
    p.crestDb = p.peakDb - p.rmsDb;
    p.valid = p.seconds >= 1.5f && p.rmsDb > -70;
    return p;
}

std::string TrackProfile::describe() const
{
    if (! valid) return "faixa ainda não ouvida";
    char b[256];
    std::snprintf (b, sizeof b, "nível médio %.0f dBFS, pico %.0f dBFS, transientes %s; graves %+.0f, médio-graves %+.0f, médios %+.0f, presença %+.0f, agudos %+.0f dB",
                   rmsDb, peakDb, crestDb > 16 ? "fortes" : (crestDb > 10 ? "normais" : "suaves"),
                   bandDb[0], bandDb[1], bandDb[2], bandDb[3], bandDb[4]);
    std::string s = b;
    for (auto& c : s) if (c == '.') c = ',';
    return s;
}

// O cérebro escreve limiares pensando numa faixa com nível médio de −18 dBFS.
std::string adaptToProfile (Treatment& t, const TrackProfile& p)
{
    if (! p.valid) return "";
    const float shift = std::clamp (p.rmsDb - (-18.0f), -24.0f, 12.0f);
    std::string notes;
    char buf[160];
    auto note = [&] (const char* s) { if (! notes.empty()) notes += "; "; notes += s; };

    for (auto& b : t.blocks)
    {
        const auto& bi = info (b.type);
        auto setP = [&] (int i, float v) { b.p[(size_t) i] = bi.params[(size_t) i].clamp (v); };
        switch (b.bt())
        {
            case BlockType::Compressor:
                if (std::abs (shift) >= 2) { setP (0, b.p[0] + shift); std::snprintf (buf, sizeof buf, "limiar do compressor %.0f dB", b.p[0]); note (buf); }
                break;
            case BlockType::DeEsser:
                if (std::abs (shift) >= 2) { setP (1, b.p[1] + shift); std::snprintf (buf, sizeof buf, "limiar do de-esser %.0f dB", b.p[1]); note (buf); }
                break;
            case BlockType::Gate:
            {   // fecha 25 dB abaixo do nível médio, nunca acima de −30
                const float thr = std::min (p.rmsDb - 25.0f, -30.0f);
                if (std::abs (thr - b.p[0]) >= 2) { setP (0, thr); std::snprintf (buf, sizeof buf, "limiar do gate %.0f dB", b.p[0]); note (buf); }
                break;
            }
            case BlockType::Limitador:
            {   // não empurra mais que ~8 dB acima do teto: evita esmagar faixas que já estão altas
                const float maxGain = std::max (0.0f, b.p[0] - p.peakDb + 8.0f);
                if (b.p[1] > maxGain + 0.5f) { setP (1, maxGain); std::snprintf (buf, sizeof buf, "ganho do limitador %.1f dB", b.p[1]); note (buf); }
                break;
            }
            case BlockType::EQ:
            {   // médio-graves sobrando ("embolado") e o médio do EQ livre: tira um pouco em 300 Hz
                if (p.bandDb[1] - p.bandDb[2] > 6 && std::abs (b.p[3]) < 0.01f)
                {
                    setP (2, 300); setP (3, -2.5f); note ("médio-graves −2,5 dB em 300 Hz (estavam sobrando)");
                }
                break;
            }
            default: break;
        }
    }
    std::string out = notes;
    for (auto& c : out) if (c == '.') c = ',';
    return out.empty() ? "" : "Ajustei à sua faixa: " + out + ".";
}
} // namespace wr
