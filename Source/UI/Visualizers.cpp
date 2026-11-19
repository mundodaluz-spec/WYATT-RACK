#include "Visualizers.h"
#include "Theme.h"
#include "Labels.h"
#include "../Engine/Dsp.h"
#include <cmath>
#include <complex>

namespace wr::ui
{
using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::Point; using juce::Colour;
static constexpr float kTwoPi = 6.28318530718f;
static constexpr double kSr = 48000.0;

// ------------------------------------------------------------------ utilidades de desenho
static void txt (Graphics& g, const juce::String& s, float x, float y, float size, Colour c, juce::Justification j = juce::Justification::left, int w = 300, int weight = 500)
{
    g.setFont (bc (size, weight)); g.setColour (c);
    float x0 = x;
    if (j == juce::Justification::centred) x0 = x - (float) w / 2;
    else if (j == juce::Justification::right) x0 = x - (float) w;
    g.drawText (s, Rectangle<float> (x0, y - size, (float) w, size * 1.3f), j, false);
}
static void caption (Graphics& g, const juce::String& s, float x, float y, Colour c = col::faint, juce::Justification j = juce::Justification::left)
{
    g.setFont (bc (12.5f, 600)); g.setColour (c);
    const int w = 420;
    float x0 = j == juce::Justification::right ? x - (float) w : (j == juce::Justification::centred ? x - (float) w / 2 : x);
    g.drawText (s, Rectangle<float> (x0, y - 12, (float) w, 16), j, false);
}
static void glowStroke (Graphics& g, const Path& p, Colour c, float w)
{
    g.setColour (c.withAlpha (0.22f)); g.strokePath (p, juce::PathStrokeType (w * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c); g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
static void fillUnder (Graphics& g, const Path& curve, float x0, float x1, float baseY, Colour c, float alpha, float topY)
{
    Path a (curve); a.lineTo (x1, baseY); a.lineTo (x0, baseY); a.closeSubPath();
    juce::ColourGradient cg (c.withAlpha (alpha), 0, topY, c.withAlpha (0.0f), 0, baseY, false);
    g.setGradientFill (cg); g.fillPath (a);
}
static void drawNode (Graphics& g, Point<float> p, const juce::String& label, Colour c = col::red)
{
    g.setColour (juce::Colour (0xff0b0b0c)); g.fillEllipse (p.x - 8, p.y - 8, 16, 16);
    g.setColour (c); g.drawEllipse (p.x - 8, p.y - 8, 16, 16, 2.5f);
    g.setColour (col::cream); g.fillEllipse (p.x - 3, p.y - 3, 6, 6);
    if (label.isNotEmpty()) txt (g, label, p.x, p.y - 12, 12.5f, col::cream, juce::Justification::centred, 120, 600);
}
static void tabsRow (Graphics& g, std::vector<juce::String> names, int sel, float x, float y, float w, float h, Colour c)
{
    for (int i = 0; i < (int) names.size(); ++i)
    {
        auto r = Rectangle<float> (x + (float) i * (w + 6), y, w, h);
        g.setColour (i == sel ? c.withAlpha (0.12f) : juce::Colour (0xff0d0d0f)); g.fillRoundedRectangle (r, 5);
        g.setColour (i == sel ? c : col::grid2); g.drawRoundedRectangle (r, 5, i == sel ? 1.6f : 1.0f);
        g.setFont (bc (13.5f, 700)); g.setColour (i == sel ? col::cream : col::dim);
        g.drawText (names[(size_t) i], r, juce::Justification::centred);
    }
}

// grade de frequência (20 Hz–20 kHz) e dB
static float fx (float f, float x0, float x1) { return x0 + (std::log10 (juce::jmax (f, 20.0f)) - std::log10 (20.0f)) / 3.0f * (x1 - x0); }
static void freqGrid (Graphics& g, Rectangle<float> r, float dbRange, float dbStep)
{
    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = fx (f, r.getX(), r.getRight());
        g.setColour (col::grid); g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        txt (g, f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), x, r.getBottom() + 18, 12, col::faint, juce::Justification::centred, 40);
    }
    for (float db = -dbRange; db <= dbRange + 0.01f; db += dbStep)
    {
        const float y = r.getY() + (dbRange - db) / (2 * dbRange) * r.getHeight();
        g.setColour (std::abs (db) < 0.01f ? col::grid2 : col::grid); g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        txt (g, (db > 0 ? "+" : "") + juce::String ((int) db), r.getX() - 8, y + 4, 11, col::faint, juce::Justification::right, 30);
    }
}
static double magDb (const dsp::Biquad& b, double f)
{
    const double w = 2 * dsp::kPi * f / kSr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2 * w);
    const auto h = (b.b0 + b.b1 * z1 + b.b2 * z2) / (1.0 + b.a1 * z1 + b.a2 * z2);
    return 20 * std::log10 (juce::jmax (std::abs (h), 1e-9));
}
static Path curvePath (Rectangle<float> r, std::function<float (float)> dbAt, float dbRange)
{
    Path p;
    for (int i = 0; i <= 300; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / 300.0f);
        const float db = juce::jlimit (-dbRange * 1.2f, dbRange * 1.2f, dbAt (f));
        const float x = fx (f, r.getX(), r.getRight()), y = r.getY() + (dbRange - db) / (2 * dbRange) * r.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}
static Path spectrumPath (const std::vector<float>& s, Rectangle<float> r, float topDb, float rangeDb)
{
    Path p;
    const int n = (int) s.size();
    for (int i = 0; i < n; ++i)
    {
        const float x = r.getX() + (float) i / (float) (n - 1) * r.getWidth();
        const float y = r.getY() + juce::jlimit (0.0f, 1.0f, (topDb - s[(size_t) i]) / rangeDb) * r.getHeight();
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}
static void grMeter (Graphics& g, float x, float y, int n, float gr, Colour c, float maxDb = 20)
{
    caption (g, "GR", x + 20, y - 10, col::faint, juce::Justification::centred);
    const float step = maxDb / (float) n;
    for (int k = 0; k < n; ++k)
    {
        const bool on = gr > (float) k * step + step * 0.3f;
        auto r = Rectangle<float> (x, y + (float) k * 11.5f, 40, 8);
        if (on) { g.setColour (c.withAlpha (0.25f)); g.fillRoundedRectangle (r.expanded (2), 2); }
        g.setColour (on ? c : c.withAlpha (0.12f)); g.fillRoundedRectangle (r, 1.5f);
        if (k % 4 == 0) txt (g, juce::String ((int) ((float) k * step)), x - 6, y + (float) k * 11.5f + 8, 11, col::faint, juce::Justification::right, 30);
    }
    txt (g, gr > 0.05f ? "-" + juce::String (gr, 1) : "0.0", x + 20, y + (float) n * 11.5f + 24, 21, col::cream, juce::Justification::centred, 80, 700);
}

// ------------------------------------------------------------------ dispatch
void Visualizer::paint (Graphics& g)
{
    handles.clear(); tabs.clear();
    switch ((BlockType) state.type)
    {
        case BlockType::EQ: paintEq (g); break;
        case BlockType::Compressor: paintComp (g); break;
        case BlockType::Saturacao: paintSat (g); break;
        case BlockType::Reverb: paintReverb (g); break;
        case BlockType::Delay: paintDelay (g); break;
        case BlockType::Chorus: paintChorus (g); break;
        case BlockType::Modulacao: paintPhaser (g); break;
        case BlockType::Filtro: paintFilter (g); break;
        case BlockType::DeEsser: paintDeEsser (g); break;
        case BlockType::Gate: paintGate (g); break;
        case BlockType::Estereo: paintStereo (g); break;
        case BlockType::Tremolo: paintTremolo (g); break;
        case BlockType::LoFi: paintLoFi (g); break;
        case BlockType::Limitador: paintLimiter (g); break;
        default: break;
    }
    if (! state.on)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRect (getLocalBounds());
        txt (g, "BYPASSED", (float) getWidth() / 2, (float) getHeight() / 2 + 10, 30, col::dim, juce::Justification::centred, 300, 700);
    }
}

float Visualizer::freqToX (float f, float x0, float x1) const { return fx (f, x0, x1); }
float Visualizer::xToFreq (float x, float x0, float x1) const { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - x0) / (x1 - x0))); }

// ------------------------------------------------------------------ interação
void Visualizer::mouseMove (const juce::MouseEvent& e)
{
    bool over = false;
    for (auto& h : handles) if (h.pos.getDistanceFrom (e.position) <= h.radius) over = true;
    for (auto& t : tabs) if (t.first.contains (e.position)) over = true;
    setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void Visualizer::mouseDown (const juce::MouseEvent& e)
{
    for (auto& t : tabs)
        if (t.first.contains (e.position) && setParam)
        {
            if (beginDrag) beginDrag (t.second.first);
            setParam (t.second.first, (float) t.second.second);
            if (endDrag) endDrag (t.second.first);
            return;
        }
    dragging = -1;
    float best = 1e9f;
    for (int i = 0; i < (int) handles.size(); ++i)
    {
        const float d = handles[(size_t) i].pos.getDistanceFrom (e.position);
        if (d <= handles[(size_t) i].radius && d < best) { best = d; dragging = i; }
    }
    if (dragging >= 0)
    {
        auto& h = handles[(size_t) dragging];
        if (beginDrag) { if (h.px >= 0) beginDrag (h.px); if (h.py >= 0) beginDrag (h.py); }
    }
}

void Visualizer::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0 || dragging >= (int) handles.size()) return;
    const auto h = handles[(size_t) dragging];
    applyDrag (h, e.position);
}

void Visualizer::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0 && dragging < (int) handles.size() && endDrag)
    {
        auto& h = handles[(size_t) dragging];
        if (h.px >= 0) endDrag (h.px);
        if (h.py >= 0) endDrag (h.py);
    }
    dragging = -1;
}

void Visualizer::applyDrag (const Handle& h, Point<float> m)
{
    if (! setParam) return;
    const auto t = (BlockType) state.type;
    auto yToDb = [&] (float y) { return plotDbRange - (y - plot.getY()) / plot.getHeight() * 2 * plotDbRange; };
    if (t == BlockType::EQ)
    {
        if (h.px >= 0) setParam (h.px, xToFreq (m.x, plot.getX(), plot.getRight()));
        if (h.py >= 0) setParam (h.py, yToDb (m.y));
    }
    else if (t == BlockType::Compressor)
    {   // eixo de entrada −60..0 dB
        setParam (h.px, juce::jlimit (-50.0f, 0.0f, -60.0f + (m.x - plot.getX()) / plot.getWidth() * 60.0f));
    }
    else if (t == BlockType::DeEsser)
    {
        if (h.px >= 0) setParam (h.px, xToFreq (m.x, plot.getX(), plot.getRight()));
        if (h.py >= 0) setParam (h.py, juce::jlimit (-60.0f, 0.0f, -(m.y - plot.getY()) / plot.getHeight() * 60.0f));
    }
    else if (t == BlockType::Gate)
    {
        setParam (h.py, juce::jlimit (-80.0f, 0.0f, -(m.y - plot.getY()) / plot.getHeight() * 80.0f));
    }
    else if (t == BlockType::Limitador)
    {
        setParam (h.py, juce::jlimit (-12.0f, 0.0f, -(m.y - plot.getY()) / plot.getHeight() * 24.0f));
    }
    else if (t == BlockType::Filtro)
    {
        setParam (h.px, xToFreq (m.x, plot.getX(), plot.getRight()));
    }
}

// ------------------------------------------------------------------ EQ
void Visualizer::paintEq (Graphics& g)
{
    const auto& p = state.p;
    plot = Rectangle<float> (50, 20, (float) getWidth() - 70, (float) getHeight() - 56);
    plotDbRange = 18;
    freqGrid (g, plot, 18, 6);
    // analisador
    if (! state.specDb.empty())
    {
        Rectangle<float> sr (plot.getX(), plot.getY(), plot.getWidth(), plot.getHeight());
        auto sp = spectrumPath (state.specDb, sr, 0.0f, 72.0f);
        fillUnder (g, sp, sr.getX(), sr.getRight(), sr.getBottom(), col::dim, 0.28f, sr.getY());
        g.setColour (juce::Colour (0xff5c5a57)); g.strokePath (sp, juce::PathStrokeType (1.2f));
    }
    dsp::Biquad hp1, hp2, ls, pk, hs, lp1, lp2;
    const bool hpOn = p[0] > 20.5f, lpOn = p[5] < 19900.0f;
    hp1.highpass (p[0], 0.54119610, kSr); hp2.highpass (p[0], 1.30656296, kSr);
    lp1.lowpass (p[5], 0.54119610, kSr); lp2.lowpass (p[5], 1.30656296, kSr);
    ls.lowShelf (120.0, p[1], kSr); pk.peak (p[2], 1.0, p[3], kSr); hs.highShelf (8000.0, p[4], kSr);
    auto total = [&] (float f)
    {
        double d = magDb (ls, f) + magDb (pk, f) + magDb (hs, f);
        if (hpOn) d += magDb (hp1, f) + magDb (hp2, f);
        if (lpOn) d += magDb (lp1, f) + magDb (lp2, f);
        return (float) d;
    };
    auto curve = curvePath (plot, total, 18);
    const float zeroY = plot.getCentreY();
    fillUnder (g, curve, plot.getX(), plot.getRight(), zeroY, col::red, 0.35f, plot.getY());
    glowStroke (g, curve, col::cream, 2.6f);
    auto at = [&] (float f, float db) { return Point<float> (fx (f, plot.getX(), plot.getRight()), plot.getY() + (18 - db) / 36 * plot.getHeight()); };
    struct N { float f, db; const char* l; int px, py; };
    const N nodes[] = { { p[0], 0, "LOW CUT", 0, -1 }, { 120, (float) total (120), "LOW", -1, 1 }, { p[2], (float) total (p[2]), "MID", 2, 3 },
                        { 8000, (float) total (8000), "HIGH", -1, 4 }, { juce::jmin (p[5], 19000.0f), 0, "HIGH CUT", 5, -1 } };
    for (auto& n : nodes)
    {
        const auto pt = at (n.f, n.db);
        const bool off = (n.px == 0 && ! hpOn) || (n.px == 5 && ! lpOn);
        drawNode (g, pt, n.l, off ? juce::Colour (0xff5c5a57) : col::red);
        addHandle (n.px, n.py, pt, 14);
    }
    caption (g, "ANALYZER  ·  DRAG THE POINTS", plot.getRight() - 6, 36, col::faint, juce::Justification::right);
}

// ------------------------------------------------------------------ COMPRESSOR
void Visualizer::paintComp (Graphics& g)
{
    const auto& p = state.p;
    const float X0 = 40, Y0 = 22, S = 250;
    plot = { X0, Y0, S, S };
    auto tx = [&] (float db) { return X0 + (db + 60) / 60 * S; };
    auto ty = [&] (float db) { return Y0 + (-db) / 60 * S; };
    for (int db = -60; db <= 0; db += 12)
    {
        g.setColour (col::grid); g.drawVerticalLine ((int) tx ((float) db), Y0, Y0 + S); g.drawHorizontalLine ((int) ty ((float) db), X0, X0 + S);
    }
    g.setColour (juce::Colour (0xff333333));
    { Path d; d.startNewSubPath (X0, Y0 + S); d.lineTo (X0 + S, Y0); float dl[] = { 4, 4 }; juce::PathStrokeType (1).createDashedStroke (d, d, dl, 2); g.fillPath (d); }
    const float thr = p[0], ratio = juce::jmax (1.0f, p[1]), knee = p[5], mk = p[4];
    auto out = [&] (float x)
    {
        const float o = x - thr; float gdb = 0;
        if (knee > 0 && 2 * std::abs (o) <= knee) gdb = (1 / ratio - 1) * (o + knee / 2) * (o + knee / 2) / (2 * knee);
        else if (o > 0) gdb = (1 / ratio - 1) * o;
        return x + gdb + mk;
    };
    Path c;
    for (int i = 0; i <= 120; ++i)
    {
        const float x = -60 + 60 * (float) i / 120;
        const float y = juce::jlimit (Y0, Y0 + S, ty (out (x)));
        if (i == 0) c.startNewSubPath (tx (x), y); else c.lineTo (tx (x), y);
    }
    glowStroke (g, c, col::cream, 2.6f);
    const float thx = tx (thr);
    { Path d; d.startNewSubPath (thx, Y0); d.lineTo (thx, Y0 + S); float dl[] = { 3, 4 }; juce::PathStrokeType (1.2f).createDashedStroke (d, d, dl, 2); g.setColour (col::red.withAlpha (0.85f)); g.fillPath (d); }
    txt (g, "THRESHOLD", thx + 6, Y0 + S - 8, 12.5f, col::red, juce::Justification::left, 100, 600);
    drawNode (g, { thx, Y0 + 14 }, {}, col::red);
    addHandle (0, -1, { thx, Y0 + 14 }, 14);
    addHandle (0, -1, { thx, Y0 + S * 0.5f }, 10);
    const float inDb = juce::jlimit (-60.0f, 0.0f, state.inDb);
    if (state.inDb > -59)
    {
        const Point<float> dot (tx (inDb), juce::jlimit (Y0, Y0 + S, ty (out (inDb))));
        g.setColour (col::red.withAlpha (0.3f)); g.fillEllipse (dot.x - 11, dot.y - 11, 22, 22);
        g.setColour (col::red); g.fillEllipse (dot.x - 7, dot.y - 7, 14, 14);
    }
    caption (g, "INPUT  ->  OUTPUT", X0, Y0 + S + 22);
    // histórico de nível e redução
    const float HX = 330, HW = 470;
    caption (g, "LEVEL & GAIN REDUCTION", HX, 34);
    caption (g, "GAIN REDUCTION", HX + HW, 34, col::red, juce::Justification::right);
    const auto& hi = state.hIn; const auto& hl = state.hLive;
    if (! hi.empty())
    {
        Path lv; Path gr;
        const int n = (int) hi.size();
        for (int i = 0; i < n; ++i)
        {
            const float x = HX + (float) i / (float) (n - 1) * HW;
            const float y = 262 - juce::jlimit (0.0f, 1.0f, (hi[(size_t) i] + 60) / 60) * 200;
            const float gy = 50 + juce::jlimit (0.0f, 20.0f, hl[(size_t) i]) * 5.0f;
            if (i == 0) { lv.startNewSubPath (x, y); gr.startNewSubPath (x, gy); } else { lv.lineTo (x, y); gr.lineTo (x, gy); }
        }
        fillUnder (g, lv, HX, HX + HW, 262, col::cream, 0.18f, 60);
        g.setColour (juce::Colour (0xff9a948a)); g.strokePath (lv, juce::PathStrokeType (1.3f));
        Path ga (gr); ga.lineTo (HX + HW, 50); ga.lineTo (HX, 50); ga.closeSubPath();
        g.setColour (col::red.withAlpha (0.35f)); g.fillPath (ga);
        glowStroke (g, gr, col::red, 2.0f);
    }
    g.setColour (col::grid2); g.drawHorizontalLine (50, HX, HX + HW);
    grMeter (g, 850, 46, 20, state.live, col::red);
}

// ------------------------------------------------------------------ SATURAÇÃO
static float satShape (int type, float u, float driveDb)
{
    const float d = std::pow (10.0f, driveDb / 20.0f);
    const float bias = type == 0 ? 0.25f : 0.0f;
    auto tr = [] (float v) { return v / std::cbrt (1.0f + std::abs (v * v * v)); };
    if (type == 2) return tr (d * u * 0.9f) / juce::jmax (1e-4f, tr (d * 0.9f));
    return (std::tanh (d * u * 0.9f + bias) - std::tanh (bias)) / juce::jmax (1e-4f, std::tanh (d * 0.9f + bias) - std::tanh (bias));
}

void Visualizer::paintSat (Graphics& g)
{
    const auto& p = state.p;
    const int type = juce::jlimit (0, 2, (int) p[1]);
    const char* names[] = { "TUBE", "TAPE", "TRANSISTOR" };
    const char* subs[] = { "12AX7 STYLE", "15 IPS", "CLASS A" };
    for (int i = 0; i < 3; ++i)
    {
        auto r = Rectangle<float> (24 + (float) i * 112, 22, 104, 160);
        const bool on = i == type;
        g.setColour (on ? juce::Colour (0xff1a0d0d) : juce::Colour (0xff0d0d0f)); g.fillRoundedRectangle (r, 8);
        g.setColour (on ? col::red : col::grid2); g.drawRoundedRectangle (r, 8, on ? 2.0f : 1.0f);
        const float cx = r.getCentreX();
        g.setColour (col::cream);
        if (i == 0)
        {
            Path v; v.startNewSubPath (cx - 18, 140); v.lineTo (cx - 18, 70); v.quadraticTo (cx - 18, 42, cx, 42); v.quadraticTo (cx + 18, 42, cx + 18, 70); v.lineTo (cx + 18, 140); v.closeSubPath();
            if (on)
            {
                const float glowA = 0.25f + 0.5f * juce::jlimit (0.0f, 1.0f, p[0] / 36.0f);
                juce::ColourGradient cg (juce::Colour (0xffff7a2a).withAlpha (glowA), cx, 108, juce::Colour (0xffff7a2a).withAlpha (0.0f), cx + 24, 108, true);
                g.setGradientFill (cg); g.fillEllipse (cx - 24, 70, 48, 76);
                g.setColour (juce::Colour (0xffffb066)); g.drawLine (cx, 84, cx, 132, 2);
            }
            g.setColour (on ? col::cream : col::dim); g.strokePath (v, juce::PathStrokeType (2));
            for (int k = 0; k < 4; ++k) g.drawLine (cx - 10 + (float) k * 7, 140, cx - 10 + (float) k * 7, 152, 2);
        }
        else if (i == 1)
        {
            g.setColour (on ? col::cream : col::dim);
            for (float dx : { -22.0f, 22.0f })
            {
                g.drawEllipse (cx + dx - 20, 68, 40, 40, 2); g.fillEllipse (cx + dx - 5, 83, 10, 10);
                for (int a = 0; a < 3; ++a) { const float ang = (float) a * 2.094f + (on ? (float) juce::Time::getMillisecondCounter() * 0.003f : 0); g.drawLine (cx + dx, 88, cx + dx + 14 * std::cos (ang), 88 + 14 * std::sin (ang), 1.5f); }
            }
            Path t; t.startNewSubPath (cx - 42, 104); t.lineTo (cx - 30, 140); t.lineTo (cx + 30, 140); t.lineTo (cx + 42, 104);
            g.setColour (col::dim); g.strokePath (t, juce::PathStrokeType (1.5f));
        }
        else
        {
            g.setColour (on ? col::cream : col::dim);
            Path t; t.startNewSubPath (cx - 16, 64); t.lineTo (cx + 16, 64); t.lineTo (cx + 16, 100); t.addCentredArc (cx, 100, 16, 16, 0, 1.5708f, 4.712f, false); t.closeSubPath();
            g.strokePath (t, juce::PathStrokeType (2));
            for (int k = 0; k < 3; ++k) g.drawLine (cx - 10 + (float) k * 10, 116, cx - 10 + (float) k * 10, 146, 2);
        }
        txt (g, names[i], cx, 204, 15, on ? col::cream : col::dim, juce::Justification::centred, 104, 700);
        txt (g, subs[i], cx, 220, 11, col::faint, juce::Justification::centred, 104, 500);
        addTab (r, 1, i);
    }
    // curva de transferência
    const float X0 = 380, Y0 = 24, S = 236;
    g.setColour (col::grid); g.drawRect (X0, Y0, S, S); g.drawHorizontalLine ((int) (Y0 + S / 2), X0, X0 + S); g.drawVerticalLine ((int) (X0 + S / 2), Y0, Y0 + S);
    Path c;
    for (int i = 0; i <= 120; ++i)
    {
        const float u = -1 + 2 * (float) i / 120;
        const float y = juce::jlimit (-1.2f, 1.2f, satShape (type, u, p[0]));
        const float px = X0 + (u + 1) / 2 * S, py = Y0 + (1 - (y + 1) / 2) * S;
        if (i == 0) c.startNewSubPath (px, py); else c.lineTo (px, py);
    }
    glowStroke (g, c, col::cream, 2.6f);
    caption (g, "TRANSFER CURVE", X0, Y0 + S + 22);
    // harmônicos (DFT de um seno passado pela curva)
    const int N = 256; float h[9] {};
    for (int k = 1; k <= 8; ++k)
    {
        double re = 0, im = 0;
        for (int i = 0; i < N; ++i)
        {
            const float x = 0.7f * std::sin (kTwoPi * (float) i / N);
            const float y = satShape (type, x, p[0]);
            re += y * std::cos (kTwoPi * k * i / N); im += y * std::sin (kTwoPi * k * i / N);
        }
        h[k] = (float) std::sqrt (re * re + im * im) * 2 / N;
    }
    const float HX = 660;
    caption (g, "HARMONICS", HX, 36);
    for (int k = 1; k <= 8; ++k)
    {
        const float rel = h[k] / juce::jmax (1e-6f, h[1]);
        const float hh = 200 * juce::jlimit (0.03f, 1.0f, 0.15f + 0.85f * (1 + std::log10 (juce::jmax (rel, 1e-3f)) / 3));
        auto r = Rectangle<float> (HX + (float) (k - 1) * 30, 256 - hh, 20, hh);
        g.setColour (k == 1 ? col::cream : col::red.withAlpha (0.9f)); g.fillRoundedRectangle (r, 2);
        txt (g, juce::String (k), r.getCentreX(), 276, 11, col::faint, juce::Justification::centred, 30);
    }
}

// ------------------------------------------------------------------ REVERB
void Visualizer::paintReverb (Graphics& g)
{
    const auto& p = state.p;
    const int type = juce::jlimit (0, 3, (int) p[0]);
    const char* names[] = { "ROOM", "CHURCH", "PLATE", "CATHEDRAL" };
    for (int i = 0; i < 4; ++i)
    {
        auto r = Rectangle<float> (22 + (float) i * 92, 20, 84, 84);
        const bool on = i == type;
        g.setColour (on ? juce::Colour (0xff0d1520) : juce::Colour (0xff0d0d0f)); g.fillRoundedRectangle (r, 7);
        g.setColour (on ? col::blue : col::grid2); g.drawRoundedRectangle (r, 7, on ? 2.0f : 1.0f);
        const float cx = r.getCentreX(); const auto c = on ? col::cream : col::dim;
        Path s;
        if (i == 0) { s.startNewSubPath (cx - 24, 86); s.lineTo (cx - 24, 52); s.lineTo (cx, 36); s.lineTo (cx + 24, 52); s.lineTo (cx + 24, 86); s.closeSubPath(); }
        else if (i == 1)
        {
            s.startNewSubPath (cx - 26, 88); s.lineTo (cx - 26, 58); s.lineTo (cx - 12, 46); s.lineTo (cx - 12, 30); s.lineTo (cx, 20); s.lineTo (cx + 12, 30);
            s.lineTo (cx + 12, 46); s.lineTo (cx + 26, 58); s.lineTo (cx + 26, 88); s.closeSubPath();
            s.startNewSubPath (cx - 6, 88); s.lineTo (cx - 6, 72); s.quadraticTo (cx, 64, cx + 6, 72); s.lineTo (cx + 6, 88);
        }
        else if (i == 2)
        {
            s.addRectangle (cx - 26, 40, 52, 34);
            for (float yy : { 50.0f, 64.0f }) { s.startNewSubPath (cx - 20, yy); s.quadraticTo (cx - 10, yy + 8, cx, yy); s.quadraticTo (cx + 10, yy - 8, cx + 20, yy); }
        }
        else
        {
            s.startNewSubPath (cx - 30, 88); s.lineTo (cx - 30, 50); s.quadraticTo (cx - 30, 28, cx - 14, 22); s.quadraticTo (cx, 14, cx + 14, 22);
            s.quadraticTo (cx + 30, 28, cx + 30, 50); s.lineTo (cx + 30, 88);
            s.startNewSubPath (cx - 14, 88); s.lineTo (cx - 14, 54); s.quadraticTo (cx, 40, cx + 14, 54); s.lineTo (cx + 14, 88);
        }
        g.setColour (c); g.strokePath (s, juce::PathStrokeType (2));
        txt (g, names[i], cx, 124, 13, c, juce::Justification::centred, 90, 700);
        addTab (r, 0, i);
    }
    // resposta ao impulso
    const float X0 = 400, X1 = (float) getWidth() - 20, Y = 250;
    const float decay = p[2], pre = p[3] / 1000.0f;
    const float span = juce::jlimit (1.0f, 12.0f, pre + decay * 1.15f);
    g.setColour (col::grid2); g.drawHorizontalLine ((int) Y, X0, X1);
    for (int k = 0; k <= 6; ++k)
    {
        const float x = X0 + (float) k * (X1 - X0) / 6;
        g.setColour (col::grid); g.drawVerticalLine ((int) x, 30, Y);
        txt (g, juce::String (span * (float) k / 6, 1) + "s", x, Y + 18, 11, col::faint, juce::Justification::centred, 40);
    }
    auto tX = [&] (float t) { return X0 + t / span * (X1 - X0); };
    g.setColour (col::cream); g.fillRect (X0 - 1, 50.0f, 3.0f, Y - 50);
    txt (g, "DRY", X0 + 6, 46, 11, col::cream);
    juce::Random rnd (42 + type);
    const int early = 6 + (int) (p[1] / 100 * 14);
    for (int k = 0; k < early; ++k)
    {
        const float t = pre + rnd.nextFloat() * (0.01f + 0.05f * p[1] / 100);
        const float h = (40 + rnd.nextFloat() * 110) * (1 - (float) k / (float) (early + 4));
        g.setColour (col::blue); g.fillRect (tX (t) - 1, Y - h, 2.0f, h);
    }
    Path env; const float t0 = pre + 0.02f;
    for (int i = 0; i <= 100; ++i)
    {
        const float t = t0 + (span - t0) * (float) i / 100;
        const float a = 150 * std::exp (-6.9f * (t - t0) / juce::jmax (0.2f, decay));
        if (i == 0) env.startNewSubPath (tX (t), Y - a); else env.lineTo (tX (t), Y - a);
    }
    fillUnder (g, env, tX (t0), X1, Y, col::blue, 0.45f, Y - 150);
    glowStroke (g, env, col::blue, 2.2f);
    txt (g, "EARLY", tX (pre) + 8, Y - 162, 11, col::blue);
    txt (g, "TAIL  ·  DECAY " + juce::String (decay, 1) + " s", tX (pre) + 190, Y - 120, 11.5f, col::blue);
    // espaço estéreo
    caption (g, "STEREO SPACE", 22, 160);
    const float w = p[5] / 100;
    g.setColour (col::grid2);
    { Path a; a.startNewSubPath (40, 270); a.quadraticTo (190, 150, 340, 270); g.strokePath (a, juce::PathStrokeType (2)); }
    { Path a; a.startNewSubPath (190 - 130 * w, 266); a.quadraticTo (190, 176, 190 + 130 * w, 266); g.setColour (col::blue.withAlpha (0.85f)); g.strokePath (a, juce::PathStrokeType (3)); }
    g.setColour (col::cream); g.fillEllipse (184, 264, 12, 12);
    txt (g, "L", 40, 292, 11, col::faint); txt (g, "R", 334, 292, 11, col::faint);
}

// ------------------------------------------------------------------ DELAY
void Visualizer::paintDelay (Graphics& g)
{
    const auto& p = state.p;
    const int sync = (int) p[1];
    const float secs = sync > 0 ? dsp::syncSeconds (sync, state.bpm, false) : p[0] / 1000.0f;
    const float beat = (float) (60.0 / juce::jmax (20.0, state.bpm));
    const float span = juce::jmax (4 * beat, secs * 6.5f);
    const float X0 = 60, X1 = (float) getWidth() - 30;
    auto tX = [&] (float t) { return X0 + t / span * (X1 - X0); };
    for (int b = 0; (float) b * beat / 2 <= span + 1e-3f; ++b)
    {
        const float x = tX ((float) b * beat / 2);
        g.setColour (b % 2 == 0 ? juce::Colour (0xff26262a) : col::grid); g.drawVerticalLine ((int) x, 30, 262);
        if (b % 2 == 0) txt (g, juce::String (b / 2 + 1), x, 282, 11, col::faint, juce::Justification::centred, 30);
    }
    txt (g, "L", 20, 96, 14, col::cream, juce::Justification::left, 20, 700);
    txt (g, "R", 20, 216, 14, col::cream, juce::Justification::left, 20, 700);
    g.setColour (col::grid2); g.drawHorizontalLine (140, X0, X1); g.drawHorizontalLine (262, X0, X1);
    g.setColour (col::cream); g.fillRoundedRectangle (X0 - 3, 40, 6, 100, 2); g.fillRoundedRectangle (X0 - 3, 162, 6, 100, 2);
    const bool pp = p[4] >= 0.5f; const float fb = p[2] / 100;
    float amp = 1;
    for (int k = 1; k < 40; ++k)
    {
        const float t = secs * (float) k;
        if (t > span) break;
        amp = k == 1 ? 1.0f : amp * fb;
        const float h = 100 * amp * (0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, p[3] / 20000.0f * 1.5f));
        if (h < 1.5f) break;
        const bool top = ! pp || k % 2 == 1;
        const float yb = top ? 140.0f : 262.0f;
        auto r = Rectangle<float> (tX (t) - 3, yb - h, 6, h);
        g.setColour (col::blue.withAlpha (0.3f)); g.fillRoundedRectangle (r.expanded (3), 3);
        g.setColour (col::blue); g.fillRoundedRectangle (r, 2);
        if (! pp) { g.setColour (col::blue.withAlpha (0.8f)); g.fillRoundedRectangle (r.withY (262 - h), 2); }
    }
    juce::String info = (sync > 0 ? choiceName (state.type, 1, sync) : juce::String ("FREE")) + "  ·  " + juce::String (juce::roundToInt (secs * 1000)) + " ms @ "
                      + juce::String (juce::roundToInt (state.bpm)) + " BPM" + (pp ? "  ·  PING-PONG" : "");
    caption (g, info, X1, 22, col::faint, juce::Justification::right);
}

// ------------------------------------------------------------------ CHORUS
void Visualizer::paintChorus (Graphics& g)
{
    const auto& p = state.p;
    const int voices = juce::jlimit (1, 3, (int) p[2] + 1);
    const float X0 = 40, X1 = (float) getWidth() - 260, ym = 150, amp = 25 + 70 * p[1] / 100;
    caption (g, "VOICE DELAY MODULATION", X0, 30);
    for (int k = 0; k < 5; ++k) { g.setColour (col::grid); g.drawHorizontalLine (60 + k * 45, X0, X1); }
    const float ph = state.live;
    for (int v = 0; v < voices; ++v)
    {
        Path c;
        for (int i = 0; i <= 200; ++i)
        {
            const float t = (float) i / 200;
            const float y = ym - amp * std::sin (kTwoPi * (2 * t + (float) v / (float) voices + ph));
            if (i == 0) c.startNewSubPath (X0 + t * (X1 - X0), y); else c.lineTo (X0 + t * (X1 - X0), y);
        }
        glowStroke (g, c, col::violet.withAlpha (1.0f - (float) v * 0.22f), 2.2f);
        txt (g, "V" + juce::String (v + 1), X1 - 32, 80 + (float) v * 18, 12, col::violet.withAlpha (1.0f - (float) v * 0.22f), juce::Justification::left, 40, 700);
    }
    g.setColour (col::cream); g.drawVerticalLine ((int) (X0 + 0.0f), 44, 256);
    const float cx = (float) getWidth() - 130, cy = 170;
    caption (g, "STEREO FIELD", cx, 30, col::faint, juce::Justification::centred);
    { Path a; a.addCentredArc (cx, cy + 60, 120, 120, 0, -1.2f, 1.2f, true); g.setColour (col::grid2); g.strokePath (a, juce::PathStrokeType (2)); }
    const float spread = 50 * p[3] / 100;
    for (int v = 0; v < voices; ++v)
    {
        const float a = voices == 1 ? 0 : -spread + 2 * spread * (float) v / (float) (voices - 1);
        const float x = cx + 100 * std::sin (juce::degreesToRadians (a)), y = cy + 60 - 100 * std::cos (juce::degreesToRadians (a)) * 0.9f;
        g.setColour (col::violet.withAlpha (0.3f)); g.fillEllipse (x - 15, y - 15, 30, 30);
        g.setColour (col::violet); g.fillEllipse (x - 11, y - 11, 22, 22);
        txt (g, juce::String (v + 1), x, y + 5, 12, juce::Colour (0xff111111), juce::Justification::centred, 20, 700);
    }
    g.setColour (col::cream); g.fillEllipse (cx - 5, cy + 55, 10, 10);
}

// ------------------------------------------------------------------ PHASER / FLANGER
void Visualizer::paintPhaser (Graphics& g)
{
    const auto& p = state.p;
    const bool flanger = (int) p[0] == 1;
    plot = Rectangle<float> (50, 20, (float) getWidth() - 70, (float) getHeight() - 56);
    freqGrid (g, plot, 24, 12);
    const float depth = p[2] / 100, center = p[4], fb = p[3] / 100;
    auto respAt = [&] (float ph)
    {
        const float lfo = std::sin (kTwoPi * ph);
        return [=] (float f)
        {
            if (! flanger)
            {   // 6 passa-tudo de 1ª ordem somados ao seco: notches onde a fase soma 180°
                const float fc = juce::jlimit (20.0f, 20000.0f, center * std::pow (2.0f, 2.0f * depth * lfo));
                const float phase = 6 * 2 * std::atan (f / fc);
                const float m = std::abs (std::cos (phase / 2));
                return 20 * std::log10 (juce::jmax (0.02f, m * (1 + 0.6f * fb) + (1 - std::abs (fb)) * 0.02f));
            }
            const float baseMs = juce::jlimit (0.3f, 10.0f, 2000.0f / center);
            const float dms = baseMs * (1 + 0.9f * depth * lfo);
            const float m = std::abs (std::cos (juce::MathConstants<float>::pi * f * dms / 1000.0f));
            return 20 * std::log10 (juce::jmax (0.02f, m));
        };
    };
    auto ghost = curvePath (plot, respAt (0.0f), 24);
    g.setColour (juce::Colour (0xff6c5a8c).withAlpha (0.35f)); g.strokePath (ghost, juce::PathStrokeType (1.4f));
    auto now = curvePath (plot, respAt (state.live), 24);
    glowStroke (g, now, col::violet, 2.6f);
    if (! flanger)
    {
        const float lo = center * std::pow (2.0f, -2 * depth), hi = center * std::pow (2.0f, 2 * depth);
        const float xa = fx (lo, plot.getX(), plot.getRight()), xb = fx (hi, plot.getX(), plot.getRight());
        g.setColour (col::violet.withAlpha (0.06f)); g.fillRect (xa, plot.getY(), xb - xa, plot.getHeight());
        caption (g, "SWEEP RANGE", (xa + xb) / 2, plot.getY() + 16, col::violet, juce::Justification::centred);
    }
    tabsRow (g, { "PHASER", "FLANGER" }, flanger ? 1 : 0, (float) getWidth() - 196, plot.getY() + 4, 80, 26, col::violet);
    addTab ({ (float) getWidth() - 196, plot.getY() + 4, 80, 26 }, 0, 0);
    addTab ({ (float) getWidth() - 110, plot.getY() + 4, 80, 26 }, 0, 1);
}

// ------------------------------------------------------------------ FILTRO
void Visualizer::paintFilter (Graphics& g)
{
    const auto& p = state.p;
    const int type = juce::jlimit (0, 2, (int) p[0]);
    plot = Rectangle<float> (50, 20, (float) getWidth() - 70, (float) getHeight() - 56);
    freqGrid (g, plot, 24, 12);
    const float q = 0.5f / (1 - 0.975f * juce::jlimit (0.0f, 1.0f, p[2] / 100));
    auto resp = [&] (float fc)
    {
        dsp::Biquad b;
        if (type == 0) b.lowpass (fc, q, kSr); else if (type == 1) b.highpass (fc, q, kSr); else b.bandpass (fc, q, kSr);
        const float bpGain = type == 2 ? 20 * std::log10 (juce::jmax (0.05f, 2 * (1 - 0.975f * p[2] / 100))) : 0.0f;
        return [b, bpGain] (float f) { return (float) magDb (b, f) + bpGain; };
    };
    const float lfoOct = p[4] / 100 * 3;
    if (lfoOct > 0.01f)
    {
        for (float o : { -lfoOct, lfoOct })
        {
            auto gpath = curvePath (plot, resp (juce::jlimit (20.0f, 20000.0f, p[1] * std::pow (2.0f, o))), 24);
            Path d; float dl[] = { 5, 4 }; juce::PathStrokeType (1.4f).createDashedStroke (d, gpath, dl, 2);
            g.setColour (juce::Colour (0xff5c5a57)); g.fillPath (d);
        }
        const float xa = fx (p[1] * std::pow (2.0f, -lfoOct), plot.getX(), plot.getRight()), xb = fx (juce::jmin (20000.0f, p[1] * std::pow (2.0f, lfoOct)), plot.getX(), plot.getRight());
        g.setColour (col::red.withAlpha (0.7f)); g.fillRoundedRectangle (xa, plot.getBottom() - 12, xb - xa, 6, 3);
        caption (g, "LFO RANGE", (xa + xb) / 2, plot.getBottom() - 18, col::red, juce::Justification::centred);
    }
    const float fNow = state.live > 20 ? state.live : p[1];
    auto curve = curvePath (plot, resp (fNow), 24);
    fillUnder (g, curve, plot.getX(), plot.getRight(), plot.getBottom(), col::red, 0.3f, plot.getY());
    glowStroke (g, curve, col::cream, 2.6f);
    const auto r = resp (fNow);
    const Point<float> nd (fx (fNow, plot.getX(), plot.getRight()), plot.getY() + (24 - juce::jlimit (-24.0f, 24.0f, r (fNow))) / 48 * plot.getHeight());
    drawNode (g, nd, "CUTOFF");
    addHandle (1, -1, nd, 16);
    tabsRow (g, { "LOW-PASS", "HIGH-PASS", "BAND-PASS" }, type, (float) getWidth() - 282, plot.getY() + 4, 80, 26, col::cream);
    for (int i = 0; i < 3; ++i) addTab ({ (float) getWidth() - 282 + (float) i * 86, plot.getY() + 4, 80, 26 }, 0, i);
}

// ------------------------------------------------------------------ DE-ESSER
void Visualizer::paintDeEsser (Graphics& g)
{
    const auto& p = state.p;
    const float X1 = (float) getWidth() - 130;
    plot = Rectangle<float> (50, 20, X1 - 50, (float) getHeight() - 56);
    freqGrid (g, plot, 30, 10);
    if (! state.specDb.empty())
    {
        auto sp = spectrumPath (state.specDb, plot, 0.0f, 60.0f);
        fillUnder (g, sp, plot.getX(), plot.getRight(), plot.getBottom(), col::cream, 0.2f, plot.getY());
        g.setColour (juce::Colour (0xff9a948a)); g.strokePath (sp, juce::PathStrokeType (1.4f));
    }
    const float bx = fx (p[0], plot.getX(), plot.getRight());
    g.setColour (col::amber.withAlpha (0.09f)); g.fillRect (bx, plot.getY(), plot.getRight() - bx, plot.getHeight());
    g.setColour (col::amber); g.drawLine (bx, plot.getY(), bx, plot.getBottom(), 2);
    const float ty = plot.getY() + (-p[1]) / 60 * plot.getHeight();
    { Path d; d.startNewSubPath (bx, ty); d.lineTo (plot.getRight(), ty); float dl[] = { 6, 4 }; juce::PathStrokeType (2).createDashedStroke (d, d, dl, 2); g.setColour (col::red); g.fillPath (d); }
    txt (g, "THRESHOLD", bx + 8, ty - 6, 12.5f, col::red, juce::Justification::left, 100, 600);
    caption (g, (int) p[3] == 1 ? "SIBILANCE · WIDEBAND" : "SIBILANCE BAND", bx + 8, plot.getY() + 18, col::amber);
    drawNode (g, { bx, plot.getBottom() - 16 }, {}, col::amber);
    addHandle (0, -1, { bx, plot.getBottom() - 16 }, 14);
    drawNode (g, { (bx + plot.getRight()) / 2, ty }, {}, col::red);
    addHandle (-1, 1, { (bx + plot.getRight()) / 2, ty }, 14);
    grMeter (g, (float) getWidth() - 90, 46, 18, state.live, col::amber, 18);
}

// ------------------------------------------------------------------ GATE
void Visualizer::paintGate (Graphics& g)
{
    const auto& p = state.p;
    const float X0 = 60, X1 = (float) getWidth() - 150, Y0 = 30, Y1 = 250;
    plot = { X0, Y0, X1 - X0, Y1 - Y0 };
    auto toY = [&] (float db) { return Y0 + juce::jlimit (0.0f, 80.0f, -db) / 80 * (Y1 - Y0); };
    for (int db = 0; db >= -80; db -= 20) { g.setColour (col::grid); g.drawHorizontalLine ((int) toY ((float) db), X0, X1); txt (g, juce::String (db), X0 - 8, toY ((float) db) + 4, 11, col::faint, juce::Justification::right, 30); }
    const auto& hi = state.hIn; const auto& hl = state.hLive;
    const int n = (int) hi.size();
    if (n > 1)
    {
        for (int i = 0; i < n; ++i)
            if (hl[(size_t) i] > 6)
            {
                const float x = X0 + (float) i / (float) (n - 1) * (X1 - X0);
                g.setColour (col::red.withAlpha (0.08f)); g.fillRect (x, Y0, (X1 - X0) / (float) (n - 1) + 1, Y1 - Y0);
            }
        Path lv;
        for (int i = 0; i < n; ++i)
        {
            const float x = X0 + (float) i / (float) (n - 1) * (X1 - X0), y = toY (hi[(size_t) i]);
            if (i == 0) lv.startNewSubPath (x, y); else lv.lineTo (x, y);
        }
        fillUnder (g, lv, X0, X1, Y1, col::cream, 0.25f, Y0);
        g.setColour (col::label); g.strokePath (lv, juce::PathStrokeType (1.4f));
        caption (g, "GATE", X0, Y1 + 30);
        for (int i = 0; i < n; i += 2)
        {
            const bool open = hl[(size_t) i] < 6;
            g.setColour (open ? col::green.withAlpha (0.9f) : col::red.withAlpha (0.5f));
            g.fillRect (X0 + 40 + (float) i / (float) n * (X1 - X0 - 40), Y1 + 20, 3.0f, 12.0f);
        }
    }
    const float ty = toY (p[0]);
    Path th; th.startNewSubPath (X0, ty); th.lineTo (X1, ty);
    glowStroke (g, th, col::amber, 2.2f);
    txt (g, "THRESHOLD " + juce::String (juce::roundToInt (p[0])) + " dB", X0 + 6, ty - 8, 12.5f, col::amber, juce::Justification::left, 200, 600);
    drawNode (g, { X1 - 20, ty }, {}, col::amber);
    addHandle (-1, 0, { X1 - 20, ty }, 16);
    addHandle (-1, 0, { (X0 + X1) / 2, ty }, 10);
    const bool open = state.live < 6;
    const float cx = (float) getWidth() - 70;
    drawLed (g, { cx - 30, 80, 60, 60 }, col::green, open);
    txt (g, "OPEN", cx, 170, 20, open ? col::cream : col::dim, juce::Justification::centred, 100, 700);
    drawLed (g, { cx - 12, 208, 24, 24 }, col::red, ! open);
    txt (g, "CLOSED", cx, 256, 12.5f, ! open ? col::cream : col::faint, juce::Justification::centred, 100, 600);
}

// ------------------------------------------------------------------ ESTÉREO
void Visualizer::paintStereo (Graphics& g)
{
    const auto& p = state.p;
    const float cx = 190, cy = 160, r = 120;
    g.setColour (col::grid); g.drawEllipse (cx - r, cy - r, 2 * r, 2 * r, 1.5f); g.drawEllipse (cx - r / 2, cy - r / 2, r, r, 1);
    g.drawLine (cx - r, cy, cx + r, cy); g.drawLine (cx, cy - r, cx, cy + r);
    const auto& L = state.scopeL; const auto& R = state.scopeR;
    double sLR = 0, sLL = 0, sRR = 0;
    if (! L.empty())
    {
        float peak = 1e-4f;
        for (size_t i = 0; i < L.size(); ++i) peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
        const float sc = juce::jmin (1.0f / peak, 8.0f) * 0.9f;
        g.setColour (col::blue.withAlpha (0.5f));
        for (size_t i = 0; i < L.size(); i += 2)
        {
            const float m = (L[i] + R[i]) * 0.7071f * sc, s = (L[i] - R[i]) * 0.7071f * sc;
            const float x = cx + s * r, y = cy - m * r;
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r) g.fillRect (x, y, 1.6f, 1.6f);
        }
        for (size_t i = 0; i < L.size(); ++i) { sLR += L[i] * R[i]; sLL += L[i] * L[i]; sRR += R[i] * R[i]; }
    }
    txt (g, "M", cx, cy - r - 6, 12, col::faint, juce::Justification::centred, 20);
    txt (g, "S", cx + r + 12, cy + 4, 12, col::faint, juce::Justification::centred, 20);
    const float corr = (sLL > 1e-9 && sRR > 1e-9) ? (float) (sLR / std::sqrt (sLL * sRR)) : 1.0f;
    const float X0 = 400, X1 = (float) getWidth() - 40, mid = (X0 + X1) / 2;
    caption (g, "CORRELATION", X0, 44);
    g.setColour (juce::Colour (0xff0d0d0f)); g.fillRoundedRectangle (X0, 56, X1 - X0, 14, 7);
    g.setColour (col::grid2); g.drawRoundedRectangle (X0, 56, X1 - X0, 14, 7, 1);
    const float xc = X0 + (juce::jlimit (-1.0f, 1.0f, corr) + 1) / 2 * (X1 - X0);
    g.setColour ((corr < 0 ? col::red : col::blue).withAlpha (0.85f)); g.fillRoundedRectangle (juce::jmin (mid, xc), 56, std::abs (xc - mid), 14, 7);
    txt (g, "-1", X0, 92, 11, col::faint, juce::Justification::centred, 30); txt (g, "0", mid, 92, 11, col::faint, juce::Justification::centred, 30); txt (g, "+1", X1, 92, 11, col::faint, juce::Justification::centred, 30);
    caption (g, "WIDTH " + juce::String (juce::roundToInt (p[0])) + " %", X0, 130);
    const float w = juce::jlimit (0.0f, 2.0f, p[0] / 100);
    auto fan = [&] (float ww, bool fill, Colour c)
    {
        const float ang = juce::jlimit (0.05f, 1.45f, 0.62f * ww);
        Path f; f.startNewSubPath (mid, 262); f.lineTo (mid - 210 * std::sin (ang), 262 - 210 * std::cos (ang));
        f.addCentredArc (mid, 262, 210, 210, 0, -ang, ang); f.closeSubPath();
        if (fill) { g.setColour (c.withAlpha (0.12f)); g.fillPath (f); g.setColour (c); g.strokePath (f, juce::PathStrokeType (1.5f)); }
        else { Path d; float dl[] = { 4, 4 }; juce::PathStrokeType (1.2f).createDashedStroke (d, f, dl, 2); g.setColour (col::cream.withAlpha (0.6f)); g.fillPath (d); }
    };
    fan (w, true, col::blue); fan (1.0f, false, col::cream);
    if (p[1] > 20.5f)
    {
        auto rr = Rectangle<float> (mid - 46, 252, 92, 18);
        g.setColour (col::red.withAlpha (0.85f)); g.fillRoundedRectangle (rr, 4);
        txt (g, "MONO < " + formatValue (state.type, 1, p[1]), mid, 266, 12, juce::Colours::white, juce::Justification::centred, 92, 700);
    }
    const float pan = p[2] / 100;
    g.setColour (col::cream); g.fillEllipse (mid + pan * 150 - 5, 228, 10, 10);
}

// ------------------------------------------------------------------ TREMOLO
void Visualizer::paintTremolo (Graphics& g)
{
    const auto& p = state.p;
    const float X0 = 60, X1 = (float) getWidth() - 30;
    for (int k = 0; k <= 8; ++k) { g.setColour (col::grid); g.drawVerticalLine ((int) (X0 + (float) k * (X1 - X0) / 8), 30, 270); }
    txt (g, "L", 20, 96, 14, col::cream, juce::Justification::left, 20, 700);
    txt (g, "R", 20, 216, 14, col::cream, juce::Justification::left, 20, 700);
    const float depth = p[1] / 100, a = p[3] / 100; const int shape = (int) p[2];
    auto lfo = [&] (float ph)
    {
        ph -= std::floor (ph);
        if (shape == 0) return 0.5f + 0.5f * std::sin (kTwoPi * ph);
        if (shape == 1) return ph < 0.5f ? 2 * ph : 2 - 2 * ph;
        return ph < 0.5f ? 1.0f : 0.0f;
    };
    for (int lane = 0; lane < 2; ++lane)
    {
        const float ym = lane == 0 ? 90.0f : 210.0f;
        Path c;
        for (int i = 0; i <= 400; ++i)
        {
            const float t = (float) i / 400, l = lfo (4 * t);
            const float gT = 1 - depth * (1 - l), gP = lane == 0 ? 1 - depth * l : 1 - depth * (1 - l);
            const float gg = (1 - a) * gT + a * gP;
            const float y = ym + 50 - 100 * gg;
            if (i == 0) c.startNewSubPath (X0 + t * (X1 - X0), y); else c.lineTo (X0 + t * (X1 - X0), y);
        }
        fillUnder (g, c, X0, X1, ym + 55, col::violet, 0.3f, ym - 50);
        glowStroke (g, c, col::violet, 2.4f);
    }
    const float ph = state.live - std::floor (state.live);
    const float x = X0 + ph / 4 * (X1 - X0);
    g.setColour (col::cream); g.drawLine (x, 30, x, 270, 1.5f);
    const char* shapes[] = { "SINE", "TRIANGLE", "SQUARE" };
    juce::String info = shapes[juce::jlimit (0, 2, shape)];
    info << "  ·  " << ((int) p[4] > 0 ? choiceName (state.type, 4, (int) p[4]) : formatValue (state.type, 0, p[0])) << "  ·  AUTO-PAN " << juce::roundToInt (p[3]) << " %";
    caption (g, info, X1, 22, col::faint, juce::Justification::right);
}

// ------------------------------------------------------------------ LO-FI
void Visualizer::paintLoFi (Graphics& g)
{
    const auto& p = state.p;
    const float X0 = 40, X1 = 560, ym = 150;
    for (int k = -4; k <= 4; ++k) { g.setColour (col::grid); g.drawHorizontalLine ((int) (ym + (float) k * 27), X0, X1); }
    auto wave = [] (float t) { return std::sin (kTwoPi * 2 * t) * (0.85f + 0.15f * std::sin (kTwoPi * 0.5f * t)); };
    { Path s; for (int i = 0; i <= 300; ++i) { const float t = (float) i / 300; const float y = ym - 100 * wave (t); if (i == 0) s.startNewSubPath (X0 + t * (X1 - X0), y); else s.lineTo (X0 + t * (X1 - X0), y); }
      Path d; float dl[] = { 4, 4 }; juce::PathStrokeType (1.6f).createDashedStroke (d, s, dl, 2); g.setColour (juce::Colour (0xff5c5a57)); g.fillPath (d); }
    const float bits = juce::jlimit (2.0f, 24.0f, p[0]);
    const float levels = std::pow (2.0f, juce::jmin (bits, 7.0f)) / 2;   // acima de 7 bits os degraus ficam finos demais para ver
    const int steps = juce::jlimit (8, 300, (int) (p[1] / 48000.0f * 70));
    Path st;
    for (int i = 0; i < steps; ++i)
    {
        const float t0 = (float) i / (float) steps, t1 = (float) (i + 1) / (float) steps;
        const float q = std::round (wave (t0) * levels) / levels;
        const float y = ym - 100 * q;
        if (i == 0) st.startNewSubPath (X0, y); else st.lineTo (X0 + t0 * (X1 - X0), y);
        st.lineTo (X0 + t1 * (X1 - X0), y);
    }
    glowStroke (g, st, col::red, 2.4f);
    caption (g, formatValue (state.type, 0, p[0]) + "  ·  " + formatValue (state.type, 1, p[1]), X0, 30);
    juce::Random rnd (3);
    const int dots = (int) (p[2] * 2.5f);
    g.setColour (col::cream.withAlpha (0.25f));
    for (int i = 0; i < dots; ++i) g.fillEllipse (X0 + rnd.nextFloat() * (X1 - X0), 40 + rnd.nextFloat() * 220, 1.8f, 1.8f);
    const float BX = 600, BW = (float) getWidth() - 640;
    caption (g, "BAND", BX, 30);
    dsp::Biquad hp, lp; hp.highpass (p[3], 0.7071, kSr); lp.lowpass (p[4], 0.7071, kSr);
    const bool hpOn = p[3] > 20.5f, lpOn = p[4] < 19900;
    Path b;
    for (int i = 0; i <= 120; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / 120);
        const float db = (float) ((hpOn ? magDb (hp, f) : 0) + (lpOn ? magDb (lp, f) : 0));
        const float x = BX + (float) i / 120 * BW, y = 60 - juce::jmax (-30.0f, db) * 2.2f;
        if (i == 0) b.startNewSubPath (x, y); else b.lineTo (x, y);
    }
    fillUnder (g, b, BX, BX + BW, 130, col::cream, 0.2f, 60);
    g.setColour (col::cream); g.strokePath (b, juce::PathStrokeType (2.2f));
    caption (g, "WOW & FLUTTER", BX, 170);
    const float w = p[5] / 100;
    Path wf;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = (float) i / 200;
        const float y = 225 - 34 * w * std::sin (kTwoPi * 1.5f * t + (float) juce::Time::getMillisecondCounter() * 0.001f) - 9 * w * std::sin (kTwoPi * 9 * t);
        if (i == 0) wf.startNewSubPath (BX + t * BW, y); else wf.lineTo (BX + t * BW, y);
    }
    glowStroke (g, wf, col::red, 2.2f);
}

// ------------------------------------------------------------------ LIMITADOR
void Visualizer::paintLimiter (Graphics& g)
{
    const auto& p = state.p;
    const float X0 = 60, X1 = (float) getWidth() - 150, Y0 = 30, Y1 = 270;
    plot = { X0, Y0, X1 - X0, Y1 - Y0 };
    auto toY = [&] (float db) { return Y0 + juce::jlimit (0.0f, 24.0f, -db) / 24 * (Y1 - Y0); };
    for (int db = 0; db >= -24; db -= 6) { g.setColour (col::grid); g.drawHorizontalLine ((int) toY ((float) db), X0, X1); txt (g, juce::String (db), X0 - 8, toY ((float) db) + 4, 11, col::faint, juce::Justification::right, 30); }
    const auto& hi = state.hIn; const auto& ho = state.hOut; const auto& hl = state.hLive;
    const int n = (int) ho.size();
    if (n > 1)
    {
        Path pin, pout;
        for (int i = 0; i < n; ++i)
        {
            const float x = X0 + (float) i / (float) (n - 1) * (X1 - X0);
            const float yi = toY (juce::jmin (6.0f, hi[(size_t) i] + p[1])), yo = toY (ho[(size_t) i]);
            if (i == 0) { pin.startNewSubPath (x, yi); pout.startNewSubPath (x, yo); } else { pin.lineTo (x, yi); pout.lineTo (x, yo); }
            if (hl[(size_t) i] > 0.1f) { g.setColour (col::amber.withAlpha (0.75f)); g.fillRect (x - 1, Y0, 2.2f, juce::jmin (Y1 - Y0, hl[(size_t) i] * 6)); }
        }
        g.setColour (juce::Colour (0xff5c5a57)); g.strokePath (pin, juce::PathStrokeType (1.2f));
        fillUnder (g, pout, X0, X1, Y1, col::cream, 0.22f, Y0);
        g.setColour (col::cream); g.strokePath (pout, juce::PathStrokeType (1.8f));
    }
    const float cy = toY (p[0]);
    Path c; c.startNewSubPath (X0, cy); c.lineTo (X1, cy);
    glowStroke (g, c, col::red, 2.4f);
    txt (g, "CEILING " + formatValue (state.type, 0, p[0]), X1 - 4, cy - 8, 12.5f, col::red, juce::Justification::right, 200, 600);
    drawNode (g, { X1 - 20, cy + 0.0f }, {}, col::red);
    addHandle (-1, 0, { X1 - 20, cy }, 16);
    addHandle (-1, 0, { (X0 + X1) / 2, cy }, 10);
    const float cx = (float) getWidth() - 70;
    caption (g, "GR", cx, 44, col::faint, juce::Justification::centred);
    txt (g, state.live > 0.05f ? "-" + juce::String (state.live, 1) : "0.0", cx, 98, 34, col::cream, juce::Justification::centred, 120, 700);
    caption (g, "OUT PEAK", cx, 150, col::faint, juce::Justification::centred);
    txt (g, state.outDb > -99 ? juce::String (state.outDb, 1) : "-inf", cx, 184, 22, col::cream, juce::Justification::centred, 120, 700);
    caption (g, "IN PEAK", cx, 226, col::faint, juce::Justification::centred);
    txt (g, state.inDb > -99 ? juce::String (state.inDb + p[1], 1) : "-inf", cx, 258, 22, col::cream, juce::Justification::centred, 120, 700);
}
} // namespace wr::ui
