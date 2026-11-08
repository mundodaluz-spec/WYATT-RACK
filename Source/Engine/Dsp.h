// WYATT RACK — peças de DSP (filtros, linhas de atraso, utilidades)
#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace wr::dsp
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-12f)); }

// Coeficiente de um polo para uma constante de tempo (ms).
inline float onePoleCoef (double ms, double sr)
{
    if (ms <= 0) return 0.0f;
    return (float) std::exp (-1.0 / (ms * 0.001 * sr));
}

// Biquad (forma direta transposta II), estado em double.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    inline float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    // Fórmulas do "Audio EQ Cookbook" (R. Bristow-Johnson).
    void lowpass (double f, double q, double sr)
    {
        const double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void highpass (double f, double q, double sr)
    {
        const double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void bandpass (double f, double q, double sr)
    {
        const double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set (a, 0, -a, 1 + a, -2 * c, 1 - a);
    }
    void peak (double f, double q, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
    void lowShelf (double f, double db, double sr, double s = 0.8)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), sn = std::sin (w);
        const double a = sn / 2 * std::sqrt ((A + 1 / A) * (1 / s - 1) + 2), r = 2 * std::sqrt (A) * a;
        set (A * ((A + 1) - (A - 1) * c + r), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - r),
             (A + 1) + (A - 1) * c + r, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - r);
    }
    void highShelf (double f, double db, double sr, double s = 0.8)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), sn = std::sin (w);
        const double a = sn / 2 * std::sqrt ((A + 1 / A) * (1 / s - 1) + 2), r = 2 * std::sqrt (A) * a;
        set (A * ((A + 1) + (A - 1) * c + r), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - r),
             (A + 1) - (A - 1) * c + r, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - r);
    }
    // Resposta em dB numa frequência (usada pelos testes e pela tela).
    double magnitudeDb (double f, double sr) const
    {
        const double w = 2 * kPi * f / sr;
        const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        const double nr = b0 + b1 * cr + b2 * c2r, ni = b1 * ci + b2 * c2i;
        const double dr = 1 + a1 * cr + a2 * c2r, di = a1 * ci + a2 * c2i;
        return 10 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};

// Filtro de estado variável (Cytomic TPT): saídas LP, BP, HP de uma vez.
struct Svf
{
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    inline void process (float x, float g, float k, float& lp, float& bp, float& hp)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + g * a1 * v3;
        const float v2 = ic2 + g * v1;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

// Linha de atraso circular com leitura fracionária (Hermite).
struct DelayLine
{
    std::vector<float> buf;
    int w = 0, mask = 0;

    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d = 0 → amostra mais recente.
    inline float read (float d) const
    {
        d = std::max (0.0f, d);
        const int di = (int) d; const float f = d - (float) di;
        const int i0 = (w - 1 - di) & mask;
        const float y0 = buf[(size_t) ((i0 + 1) & mask)], y1 = buf[(size_t) i0];
        const float y2 = buf[(size_t) ((i0 - 1) & mask)], y3 = buf[(size_t) ((i0 - 2) & mask)];
        const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2 * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
    inline float readInt (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
};

// Gerador de ruído determinístico.
struct Noise
{
    uint32_t s = 0x9E3779B9u;
    inline float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) ((double) s / 4294967295.0 * 2.0 - 1.0);
    }
};

inline float syncSeconds (int division, double bpm, bool tremolo)
{
    const double beat = 60.0 / std::max (20.0, bpm);
    if (tremolo)
    {   // Livre, 1/2, 1/4, 1/8, 1/16, 1/8 terc.
        static const double m[] = { 0, 2, 1, 0.5, 0.25, 1.0 / 3.0 };
        return (float) (beat * m[std::clamp (division, 0, 5)]);
    }
    // Livre, 1/4, 1/4 pont., 1/8, 1/8 pont., 1/8 terc., 1/16, 1/2
    static const double m[] = { 0, 1, 1.5, 0.5, 0.75, 1.0 / 3.0, 0.25, 2 };
    return (float) (beat * m[std::clamp (division, 0, 7)]);
}
} // namespace wr::dsp
