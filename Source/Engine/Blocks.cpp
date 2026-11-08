#include "Blocks.h"
#include "Dsp.h"
#include <array>

namespace wr
{
using namespace wr::dsp;

namespace
{
inline bool changed (const float* a, float* cache, int n = kNumParams)
{
    bool c = false;
    for (int i = 0; i < n; ++i) if (a[i] != cache[i]) { cache[i] = a[i]; c = true; }
    return c;
}

// ============================================================ EQ
class EqBlock final : public Block
{
    double sr = 48000;
    Biquad hp[2][2], ls[2], pk[2], hs[2], lp[2][2];
    float cache[kNumParams] { -1, -1, -1, -1, -1, -1 };
    bool hpOn = false, lsOn = false, pkOn = false, hsOn = false, lpOn = false;
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override
    {
        for (int c = 0; c < 2; ++c) { hp[0][c].reset(); hp[1][c].reset(); lp[0][c].reset(); lp[1][c].reset(); ls[c].reset(); pk[c].reset(); hs[c].reset(); }
        for (auto& v : cache) v = -1e9f;
    }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        if (changed (p, cache))
        {
            hpOn = p[0] > 20.5f; lpOn = p[5] < 19900.0f;
            lsOn = std::abs (p[1]) > 0.01f; pkOn = std::abs (p[3]) > 0.01f; hsOn = std::abs (p[4]) > 0.01f;
            for (int c = 0; c < 2; ++c)
            {
                hp[0][c].highpass (p[0], 0.54119610, sr); hp[1][c].highpass (p[0], 1.30656296, sr);
                lp[0][c].lowpass  (p[5], 0.54119610, sr); lp[1][c].lowpass  (p[5], 1.30656296, sr);
                ls[c].lowShelf (120.0, p[1], sr); pk[c].peak (p[2], 1.0, p[3], sr); hs[c].highShelf (8000.0, p[4], sr);
            }
        }
        float* ch[2] { L, R };
        for (int c = 0; c < 2; ++c)
        {
            float* x = ch[c];
            for (int i = 0; i < n; ++i)
            {
                float v = x[i];
                if (hpOn) v = hp[1][c].process (hp[0][c].process (v));
                if (lsOn) v = ls[c].process (v);
                if (pkOn) v = pk[c].process (v);
                if (hsOn) v = hs[c].process (v);
                if (lpOn) v = lp[1][c].process (lp[0][c].process (v));
                x[i] = v;
            }
        }
    }
};

// ============================================================ COMPRESSOR
class CompBlock final : public Block
{
    double sr = 48000;
    float det = 0, grDb = 0;
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { det = 0; grDb = 0; }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        const float thr = p[0], ratio = std::max (1.0f, p[1]), knee = std::max (0.0f, p[5]);
        const float aA = onePoleCoef (p[2], sr), aR = onePoleCoef (p[3], sr);
        const float dRel = onePoleCoef (12.0, sr);
        const float makeup = p[4];
        const float slope = 1.0f / ratio - 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
            det = lvl > det ? lvl : det * dRel + lvl * (1 - dRel);
            const float x = gainToDb (det);
            const float over = x - thr;
            float g = 0;
            if (knee > 0 && 2 * std::abs (over) <= knee) g = slope * (over + knee / 2) * (over + knee / 2) / (2 * knee);
            else if (over > 0) g = slope * over;
            grDb = g < grDb ? aA * grDb + (1 - aA) * g : aR * grDb + (1 - aR) * g;
            const float gain = dbToGain (grDb + makeup);
            L[i] *= gain; R[i] *= gain;
        }
    }
};

// ============================================================ SATURAÇÃO
inline double logCosh (double x) { const double a = std::abs (x); return a + std::log1p (std::exp (-2 * a)) - 0.69314718055994530942; }

class SatBlock final : public Block
{
    double sr = 48000;
    double prevU[2] {}, dcX[2] {}, dcY[2] {};
    float lp[2] {}, tilt[2] {};
    bool first[2] { true, true };

    static float transistor (float u) { return u / std::cbrt (1.0f + std::abs (u * u * u)); }
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override
    {
        for (int c = 0; c < 2; ++c) { prevU[c] = dcX[c] = dcY[c] = 0; lp[c] = tilt[c] = 0; first[c] = true; }
    }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        const double d = dbToGain (p[0]);
        const int type = (int) p[1];
        const double bias = type == 0 ? 0.25 : 0.0;
        const double a = 0.25;
        const double ref = type == 2 ? transistor ((float) (d * a)) : (std::tanh (d * a + bias) - std::tanh (bias));
        const float comp = (float) (a / std::max (1.0e-6, ref)) * dbToGain (p[3]);
        const float dcC = (float) std::exp (-2 * kPi * 10.0 / sr);
        const float lpF = (float) (type == 1 ? 16000.0 - 7000.0 * std::min (1.0, p[0] / 36.0) : 0.0);
        const float lpC = type == 1 ? (float) std::exp (-2 * kPi * lpF / sr) : 0.0f;
        const float tC = (float) std::exp (-2 * kPi * 1000.0 / sr);
        const bool tiltOn = std::abs (p[2]) > 0.01f;
        const float gLo = dbToGain (-p[2] / 2), gHi = dbToGain (p[2] / 2);
        const double tb = std::tanh (bias);

        float* ch[2] { L, R };
        for (int c = 0; c < 2; ++c)
        {
            float* x = ch[c];
            for (int i = 0; i < n; ++i)
            {
                const double u = d * x[i] + bias;
                double y;
                if (type == 2) y = transistor ((float) (u));
                else
                {
                    if (first[c]) { prevU[c] = u; first[c] = false; }
                    const double du = u - prevU[c];
                    y = std::abs (du) > 1.0e-5 ? (logCosh (u) - logCosh (prevU[c])) / du : std::tanh (0.5 * (u + prevU[c]));
                    prevU[c] = u;
                    y -= tb;
                }
                if (type == 0)
                {   // bloqueio de DC (o viés gera componente contínua)
                    const double yy = y - dcX[c] + dcC * dcY[c];
                    dcX[c] = y; dcY[c] = yy; y = yy;
                }
                float v = (float) y * comp;
                if (type == 1) { lp[c] = v + lpC * (lp[c] - v); v = lp[c]; }
                if (tiltOn) { tilt[c] = v + tC * (tilt[c] - v); v = tilt[c] * gLo + (v - tilt[c]) * gHi; }
                x[i] = v;
            }
        }
    }
};

// ============================================================ REVERB (FDN de 8 linhas)
class ReverbBlock final : public Block
{
    static constexpr int N = 8;
    double sr = 48000;
    DelayLine line[N], pre;
    DelayLine ap[4];
    float apLen[4] {}, lpS[N] {}, len[N] {}, lenTarget[N] {};
    bool init = true;
public:
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& l : line) l.prepare ((int) (0.32 * sr));
        pre.prepare ((int) (0.21 * sr));
        static const float apMs[4] { 4.77f, 3.59f, 12.73f, 9.30f };
        for (int i = 0; i < 4; ++i) { apLen[i] = (float) (apMs[i] * 0.001 * sr); ap[i].prepare ((int) apLen[i] + 8); }
        reset();
    }
    void reset() override
    {
        for (auto& l : line) l.reset();
        for (auto& a : ap) a.reset();
        pre.reset();
        for (int i = 0; i < N; ++i) lpS[i] = 0;
        init = true;
    }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        static const float base[N] { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        static const float typeScale[4] { 0.55f, 1.15f, 0.75f, 1.75f };
        static const float typeDiff[4] { 0.55f, 0.65f, 0.72f, 0.7f };
        static const float typeBright[4] { 1.0f, 0.75f, 1.35f, 0.85f };
        const int type = std::clamp ((int) p[0], 0, 3);
        const float s = typeScale[type] * (0.5f + p[1] / 100.0f);
        const float t60 = std::max (0.1f, p[2]);
        const float preD = (float) (p[3] * 0.001 * sr);
        const float cut = std::min (18000.0f, (2000.0f + 14000.0f * p[4] / 100.0f) * typeBright[type]);
        const float dC = (float) std::exp (-2 * kPi * cut / sr);
        const float diff = typeDiff[type];
        const float width = p[5] / 100.0f;
        for (int i = 0; i < N; ++i)
        {
            lenTarget[i] = std::min ((float) (0.3 * sr), (float) (base[i] * s * 0.001 * sr));
            if (init) len[i] = lenTarget[i];
        }
        init = false;
        const float lenC = onePoleCoef (60.0, sr);
        float gain[N];
        for (int i = 0; i < N; ++i) gain[i] = (float) std::pow (10.0, -3.0 * (lenTarget[i] / sr) / t60);

        for (int k = 0; k < n; ++k)
        {
            pre.push (0.5f * (L[k] + R[k]));
            float x = preD >= 1 ? pre.read (preD) : pre.readInt (0);
            for (int a = 0; a < 4; ++a)
            {   // difusores em série (passa-tudo de Schroeder)
                const float dly = ap[a].readInt ((int) apLen[a] - 1);
                const float v = x + diff * dly;
                ap[a].push (v);
                x = dly - diff * v;
            }
            float v[N], sum = 0;
            for (int i = 0; i < N; ++i)
            {
                len[i] = lenTarget[i] + lenC * (len[i] - lenTarget[i]);
                float d = line[i].read (len[i] - 1);
                lpS[i] = d + dC * (lpS[i] - d);
                v[i] = lpS[i] * gain[i];
                sum += v[i];
            }
            const float h = sum * (2.0f / N);
            float outL = 0, outR = 0;
            for (int i = 0; i < N; ++i)
            {
                const float y = v[i] - h;
                line[i].push (y + ((i & 1) ? -x : x) * 0.5f);
                if (i & 1) outR += v[i] * ((i & 2) ? -1.0f : 1.0f);
                else       outL += v[i] * ((i & 2) ? -1.0f : 1.0f);
            }
            outL *= 0.5f; outR *= 0.5f;
            const float m = 0.5f * (outL + outR), sd = 0.5f * (outL - outR) * width;
            L[k] = m + sd; R[k] = m - sd;
        }
    }
};

// ============================================================ DELAY
class DelayBlock final : public Block
{
    double sr = 48000;
    DelayLine dl[2];
    float tSm = -1, tone[2] {}, hp[2] {}, ph = 0;
public:
    void prepare (double s, int) override { sr = s; dl[0].prepare ((int) (2.2 * sr)); dl[1].prepare ((int) (2.2 * sr)); reset(); }
    void reset() override { dl[0].reset(); dl[1].reset(); tSm = -1; tone[0] = tone[1] = hp[0] = hp[1] = 0; ph = 0; }
    void process (float* L, float* R, int n, const float* p, const Ctx& ctx) override
    {
        const int sync = (int) p[1];
        const float secs = sync > 0 ? syncSeconds (sync, ctx.bpm, false) : p[0] * 0.001f;
        const float target = std::clamp ((float) (secs * sr), 1.0f, (float) (2.0 * sr));
        if (tSm < 0) tSm = target;
        const float tc = onePoleCoef (80.0, sr);
        const float fb = std::min (0.95f, p[2] / 100.0f);
        const float toneC = (float) std::exp (-2 * kPi * p[3] / sr);
        const float hpC = (float) std::exp (-2 * kPi * 90.0 / sr);
        const bool pp = p[4] >= 0.5f;
        const float modAmt = (float) (p[5] / 100.0f * 0.003 * sr);
        const float phInc = (float) (0.6 / sr);
        for (int i = 0; i < n; ++i)
        {
            tSm = target + tc * (tSm - target);
            float dL = tSm, dR = tSm;
            if (modAmt > 0)
            {
                ph += phInc; if (ph >= 1) ph -= 1;
                dL += modAmt * (1 + std::sin ((float) (2 * kPi) * ph));
                dR += modAmt * (1 + std::cos ((float) (2 * kPi) * ph));
            }
            float rL = dl[0].read (dL - 1), rR = dl[1].read (dR - 1);
            // tom e corte de graves na realimentação (repetições escurecem)
            tone[0] = rL + toneC * (tone[0] - rL); tone[1] = rR + toneC * (tone[1] - rR);
            float fL = tone[0], fR = tone[1];
            hp[0] = fL + hpC * (hp[0] - fL); hp[1] = fR + hpC * (hp[1] - fR);
            fL -= hp[0]; fR -= hp[1];
            fL = std::tanh (fL * fb); fR = std::tanh (fR * fb);
            if (pp)
            {
                dl[0].push (0.5f * (L[i] + R[i]) + fR);
                dl[1].push (fL);
            }
            else
            {
                dl[0].push (L[i] + fL);
                dl[1].push (R[i] + fR);
            }
            L[i] = rL; R[i] = rR;
        }
    }
};

// ============================================================ CHORUS
class ChorusBlock final : public Block
{
    double sr = 48000;
    DelayLine dl[2];
    float ph = 0, last[2] {};
public:
    void prepare (double s, int) override { sr = s; dl[0].prepare ((int) (0.07 * sr)); dl[1].prepare ((int) (0.07 * sr)); reset(); }
    void reset() override { dl[0].reset(); dl[1].reset(); ph = 0; last[0] = last[1] = 0; }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        const float inc = (float) (p[0] / sr);
        const int voices = std::clamp ((int) p[2] + 1, 1, 3);
        const float baseS = (float) (p[4] * 0.001 * sr);
        const float amp = p[1] / 100.0f * 0.45f * baseS;
        const float wOff = p[3] / 100.0f * 0.25f;
        const float fb = p[5] / 100.0f;
        const float twoPi = (float) (2 * kPi);
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            dl[0].push (L[i] + fb * last[0]);
            dl[1].push (R[i] + fb * last[1]);
            float wl = 0, wr = 0;
            for (int v = 0; v < voices; ++v)
            {
                const float pv = ph + (float) v / (float) voices;
                wl += dl[0].read (baseS + amp * std::sin (twoPi * pv));
                wr += dl[1].read (baseS + amp * std::sin (twoPi * (pv + wOff)));
            }
            wl /= (float) voices; wr /= (float) voices;
            last[0] = wl; last[1] = wr;
            L[i] = wl; R[i] = wr;
        }
    }
};

// ============================================================ PHASER / FLANGER
class ModBlock final : public Block
{
    double sr = 48000;
    float apS[2][6] {}, last[2] {}, ph = 0;
    DelayLine dl[2];
public:
    void prepare (double s, int) override { sr = s; dl[0].prepare ((int) (0.03 * sr)); dl[1].prepare ((int) (0.03 * sr)); reset(); }
    void reset() override
    {
        for (auto& c : apS) for (auto& v : c) v = 0;
        last[0] = last[1] = 0; ph = 0; dl[0].reset(); dl[1].reset();
    }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        const bool flanger = (int) p[0] == 1;
        const float inc = (float) (p[1] / sr), depth = p[2] / 100.0f, fb = p[3] / 100.0f * 0.95f;
        const float center = p[4], stereo = p[5] / 100.0f * 0.5f;
        const float twoPi = (float) (2 * kPi);
        float* ch[2] { L, R };
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = std::sin (twoPi * (ph + (c ? stereo : 0.0f)));
                const float x = ch[c][i];
                if (! flanger)
                {
                    const float f = std::clamp (center * std::pow (2.0f, 2.0f * depth * lfo), 20.0f, (float) (sr * 0.45));
                    const float t = std::tan ((float) kPi * f / (float) sr);
                    const float a = (t - 1) / (t + 1);
                    float y = x + fb * last[c];
                    for (int k = 0; k < 6; ++k)
                    {   // passa-tudo de 1ª ordem
                        const float o = a * y + apS[c][k];
                        apS[c][k] = y - a * o;
                        y = o;
                    }
                    last[c] = y;
                    ch[c][i] = 0.5f * (x + y);
                }
                else
                {
                    const float baseMs = std::clamp (2000.0f / center, 0.3f, 10.0f);
                    const float d = (float) (baseMs * (1.0f + 0.9f * depth * lfo) * 0.001 * sr) + 1.0f;
                    const float y = dl[c].read (d);
                    dl[c].push (x + fb * y);
                    ch[c][i] = 0.5f * (x + y);
                }
            }
        }
    }
};

// ============================================================ FILTRO
class FilterBlock final : public Block
{
    double sr = 48000;
    Svf svf[2];
    float ph = 0, env = 0;
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { svf[0].reset(); svf[1].reset(); ph = 0; env = 0; }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        const int type = (int) p[0];
        const float k = 2.0f * (1.0f - 0.975f * std::clamp (p[2] / 100.0f, 0.0f, 1.0f));
        const float inc = (float) (p[3] / sr), lfoD = p[4] / 100.0f * 3.0f, envD = p[5] / 100.0f * 4.0f;
        const float eA = onePoleCoef (5.0, sr), eR = onePoleCoef (150.0, sr);
        const float maxF = (float) (sr * 0.45);
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
            env = lvl > env ? eA * env + (1 - eA) * lvl : eR * env + (1 - eR) * lvl;
            const float mod = lfoD * std::sin ((float) (2 * kPi) * ph) + envD * std::min (1.0f, env * 4.0f);
            const float f = std::clamp (p[1] * std::pow (2.0f, mod), 20.0f, maxF);
            const float g = std::tan ((float) kPi * f / (float) sr);
            float lp, bp, hp;
            svf[0].process (L[i], g, k, lp, bp, hp);
            L[i] = type == 0 ? lp : type == 1 ? hp : bp * k;
            svf[1].process (R[i], g, k, lp, bp, hp);
            R[i] = type == 0 ? lp : type == 1 ? hp : bp * k;
        }
    }
};

// ============================================================ DE-ESSER
class DeEsserBlock final : public Block
{
    double sr = 48000;
    Biquad side, band[2];
    float env = 0, cacheF = -1;
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { side.reset(); band[0].reset(); band[1].reset(); env = 0; cacheF = -1; }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        if (p[0] != cacheF)
        {
            cacheF = p[0];
            side.highpass (p[0], 0.7071, sr); band[0].highpass (p[0], 0.7071, sr); band[1].highpass (p[0], 0.7071, sr);
        }
        const float thr = p[1], maxRed = p[2];
        const bool wide = (int) p[3] == 1;
        const float aA = onePoleCoef (0.5, sr), aR = onePoleCoef (60.0, sr);
        for (int i = 0; i < n; ++i)
        {
            const float s = std::abs (side.process (0.5f * (L[i] + R[i])));
            env = s > env ? aA * env + (1 - aA) * s : aR * env + (1 - aR) * s;
            const float over = gainToDb (env) - thr;
            const float red = std::clamp (over * 0.75f, 0.0f, maxRed);
            const float g = dbToGain (-red);
            const float hL = band[0].process (L[i]), hR = band[1].process (R[i]);
            if (wide) { L[i] *= g; R[i] *= g; }
            else { L[i] -= (1 - g) * hL; R[i] -= (1 - g) * hR; }
        }
    }
};

// ============================================================ GATE
class GateBlock final : public Block
{
    double sr = 48000;
    float env = 0, g = 1; int holdLeft = 0; bool open = false;
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { env = 0; g = 1; holdLeft = 0; open = false; }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        const float thr = dbToGain (p[0]), close = thr * 0.6f;
        const float aA = onePoleCoef (p[1], sr), aR = onePoleCoef (p[3], sr), dR = onePoleCoef (8.0, sr);
        const int hold = (int) (p[2] * 0.001 * sr);
        const float floorG = dbToGain (-p[4]);
        for (int i = 0; i < n; ++i)
        {
            const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
            env = lvl > env ? lvl : env * dR;
            if (env > thr) { open = true; holdLeft = hold; }
            else if (open && env < close) { if (holdLeft > 0) --holdLeft; else open = false; }
            const float target = open ? 1.0f : floorG;
            g = target > g ? aA * g + (1 - aA) * target : aR * g + (1 - aR) * target;
            L[i] *= g; R[i] *= g;
        }
    }
};

// ============================================================ ESTÉREO
class StereoBlock final : public Block
{
    double sr = 48000;
    Biquad lowS;
    DelayLine haas;
    float cacheF = -1;
public:
    void prepare (double s, int) override { sr = s; haas.prepare ((int) (0.035 * sr)); reset(); }
    void reset() override { lowS.reset(); haas.reset(); cacheF = -1; }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        const float width = p[0] / 100.0f;
        const bool monoLow = p[1] > 20.5f;
        if (monoLow && p[1] != cacheF) { cacheF = p[1]; lowS.lowpass (p[1], 0.7071, sr); }
        const bool ms = monoLow || std::abs (width - 1) > 1e-4f;
        const float pan = std::clamp (p[2] / 100.0f, -1.0f, 1.0f);
        const float ang = (pan + 1) * (float) kPi / 4;
        const float gL = std::min (1.0f, std::cos (ang) * 1.41421356f), gR = std::min (1.0f, std::sin (ang) * 1.41421356f);
        const bool panOn = std::abs (pan) > 1e-4f;
        const float hd = (float) (p[3] * 0.001 * sr);
        const bool haasOn = hd > 0.5f;
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (ms)
            {
                const float m = 0.5f * (l + r);
                float s = 0.5f * (l - r);
                if (monoLow) s -= lowS.process (s);
                s *= width;
                l = m + s; r = m - s;
            }
            if (haasOn) { haas.push (r); r = haas.read (hd); }
            else haas.push (r);
            if (panOn) { l *= gL; r *= gR; }
            L[i] = l; R[i] = r;
        }
    }
};

// ============================================================ TREMOLO
class TremoloBlock final : public Block
{
    double sr = 48000;
    float ph = 0, sq = 0.5f;
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { ph = 0; sq = 0.5f; }
    void process (float* L, float* R, int n, const float* p, const Ctx& ctx) override
    {
        const int sync = (int) p[4];
        const float hz = sync > 0 ? 1.0f / std::max (0.01f, syncSeconds (sync, ctx.bpm, true)) : p[0];
        const float inc = (float) (hz / sr), depth = p[1] / 100.0f, a = p[3] / 100.0f;
        const int shape = (int) p[2];
        const float slew = onePoleCoef (1.5, sr);
        for (int i = 0; i < n; ++i)
        {
            ph += inc; if (ph >= 1) ph -= 1;
            float lfo;
            if (shape == 0) lfo = 0.5f + 0.5f * std::sin ((float) (2 * kPi) * ph);
            else if (shape == 1) lfo = ph < 0.5f ? 2 * ph : 2 - 2 * ph;
            else { const float t = ph < 0.5f ? 1.0f : 0.0f; sq = t + slew * (sq - t); lfo = sq; }
            const float gT = 1 - depth * (1 - lfo);
            const float gPL = 1 - depth * lfo, gPR = 1 - depth * (1 - lfo);
            L[i] *= (1 - a) * gT + a * gPL;
            R[i] *= (1 - a) * gT + a * gPR;
        }
    }
};

// ============================================================ LO-FI
class LoFiBlock final : public Block
{
    double sr = 48000;
    DelayLine dl[2];
    Biquad hp[2], lp[2];
    float cache[2] { -1, -1 }, ph = 0, wph1 = 0, wph2 = 0, hold[2] {}, env = 0;
    Noise noise;
public:
    void prepare (double s, int) override { sr = s; dl[0].prepare ((int) (0.012 * sr)); dl[1].prepare ((int) (0.012 * sr)); reset(); }
    void reset() override
    {
        dl[0].reset(); dl[1].reset();
        for (int c = 0; c < 2; ++c) { hp[c].reset(); lp[c].reset(); hold[c] = 0; }
        cache[0] = cache[1] = -1; ph = wph1 = wph2 = 0; env = 0; noise = Noise();
    }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        if (p[3] != cache[0] || p[4] != cache[1])
        {
            cache[0] = p[3]; cache[1] = p[4];
            for (int c = 0; c < 2; ++c) { hp[c].highpass (p[3], 0.7071, sr); lp[c].lowpass (p[4], 0.7071, sr); }
        }
        const float bits = p[0];
        const bool quant = bits < 23.5f;
        const float q = std::pow (2.0f, bits - 1.0f);
        const bool sh = p[1] < sr * 0.999;
        const float rInc = (float) (p[1] / sr);
        const float nAmp = p[2] > 0.01f ? dbToGain (-75.0f + 0.45f * p[2]) : 0.0f;
        const float wow = (float) (p[5] / 100.0f * 0.0025 * sr);
        const float eA = onePoleCoef (2.0, sr), eR = onePoleCoef (250.0, sr);
        const float i1 = (float) (0.55 / sr), i2 = (float) (3.3 / sr);
        float* ch[2] { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
            env = lvl > env ? eA * env + (1 - eA) * lvl : eR * env + (1 - eR) * lvl;
            wph1 += i1; if (wph1 >= 1) wph1 -= 1;
            wph2 += i2; if (wph2 >= 1) wph2 -= 1;
            const float wm = 0.5f + 0.35f * std::sin ((float) (2 * kPi) * wph1) + 0.15f * std::sin ((float) (2 * kPi) * wph2);
            bool tick = false;
            if (sh) { ph += rInc; if (ph >= 1) { ph -= 1; tick = true; } }
            const float nz = nAmp > 0 ? nAmp * std::min (1.0f, env * 8.0f) * noise.next() : 0.0f;
            for (int c = 0; c < 2; ++c)
            {
                float v = ch[c][i];
                dl[c].push (v);
                if (wow > 0) v = dl[c].read (1.0f + wow * wm);
                v += nz;
                v = lp[c].process (hp[c].process (v));
                if (sh) { if (tick) hold[c] = v; v = hold[c]; }
                if (quant) v = std::round (v * q) / q;
                ch[c][i] = v;
            }
        }
    }
};

// ============================================================ LIMITADOR
class LimiterBlock final : public Block
{
    double sr = 48000;
    float g = 1;
public:
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() override { g = 1; }
    void process (float* L, float* R, int n, const float* p, const Ctx&) override
    {
        const float ceil = dbToGain (p[0]), gin = dbToGain (p[1]);
        const float rc = onePoleCoef (p[2], sr);
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i] * gin, r = R[i] * gin;
            const float pk = std::max (std::abs (l), std::abs (r));
            const float target = pk > ceil ? ceil / pk : 1.0f;
            g = target < g ? target : target + rc * (g - target);
            L[i] = std::clamp (l * g, -ceil, ceil);
            R[i] = std::clamp (r * g, -ceil, ceil);
        }
    }
};
} // namespace

std::unique_ptr<Block> createBlock (BlockType t)
{
    switch (t)
    {
        case BlockType::EQ:         return std::make_unique<EqBlock>();
        case BlockType::Compressor: return std::make_unique<CompBlock>();
        case BlockType::Saturacao:  return std::make_unique<SatBlock>();
        case BlockType::Reverb:     return std::make_unique<ReverbBlock>();
        case BlockType::Delay:      return std::make_unique<DelayBlock>();
        case BlockType::Chorus:     return std::make_unique<ChorusBlock>();
        case BlockType::Modulacao:  return std::make_unique<ModBlock>();
        case BlockType::Filtro:     return std::make_unique<FilterBlock>();
        case BlockType::DeEsser:    return std::make_unique<DeEsserBlock>();
        case BlockType::Gate:       return std::make_unique<GateBlock>();
        case BlockType::Estereo:    return std::make_unique<StereoBlock>();
        case BlockType::Tremolo:    return std::make_unique<TremoloBlock>();
        case BlockType::LoFi:       return std::make_unique<LoFiBlock>();
        case BlockType::Limitador:  return std::make_unique<LimiterBlock>();
        default:                    return nullptr;
    }
}
} // namespace wr
