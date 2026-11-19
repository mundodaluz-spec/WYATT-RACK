#include "Labels.h"
#include <cmath>
#include <cstdio>
#include <vector>

namespace wr::ui
{
namespace
{
struct P { const char* name; const char* shortName; std::vector<const char*> choices; };
struct B { const char* name; const char* tagline; std::vector<P> params; };

const std::vector<B>& table()
{
    static const std::vector<B> t {
        { "EMPTY", "Free slot", {} },
        { "EQ", "Low cut, low shelf, mid bell, high shelf and high cut", {
            { "LOW CUT", "LO CUT", {} }, { "LOW", "LOW", {} }, { "MID FREQ", "MID FRQ", {} },
            { "MID", "MID", {} }, { "HIGH", "HIGH", {} }, { "HIGH CUT", "HI CUT", {} } } },
        { "COMPRESSOR", "Dynamics control with soft knee", {
            { "THRESHOLD", "THRESH", {} }, { "RATIO", "RATIO", {} }, { "ATTACK", "ATTACK", {} },
            { "RELEASE", "RELEASE", {} }, { "MAKE-UP", "MAKE-UP", {} }, { "KNEE", "KNEE", {} } } },
        { "SATURATION", "Tube, tape or transistor harmonics", {
            { "DRIVE", "DRIVE", {} }, { "TYPE", "TYPE", { "Tube", "Tape", "Transistor" } },
            { "TONE", "TONE", {} }, { "OUTPUT", "OUTPUT", {} } } },
        { "REVERB", "Room, church, plate or cathedral", {
            { "TYPE", "TYPE", { "Room", "Church", "Plate", "Cathedral" } }, { "SIZE", "SIZE", {} },
            { "DECAY", "DECAY", {} }, { "PRE-DELAY", "PRE-DLY", {} }, { "HIGHS", "HIGHS", {} }, { "WIDTH", "WIDTH", {} } } },
        { "DELAY", "Tempo-synced echo with ping-pong", {
            { "TIME", "TIME", {} }, { "SYNC", "SYNC", { "Free", "1/4", "1/4 D", "1/8", "1/8 D", "1/8 T", "1/16", "1/2" } },
            { "FEEDBACK", "FEEDBK", {} }, { "TONE", "TONE", {} }, { "PING-PONG", "PING-P", { "OFF", "ON" } }, { "MODULATION", "MOD", {} } } },
        { "CHORUS", "Multi-voice chorus and widening", {
            { "RATE", "RATE", {} }, { "DEPTH", "DEPTH", {} }, { "VOICES", "VOICES", { "1", "2", "3" } },
            { "WIDTH", "WIDTH", {} }, { "DELAY", "DELAY", {} }, { "FEEDBACK", "FEEDBK", {} } } },
        { "PHASER / FLANGER", "Sweeping notches or comb flange", {
            { "TYPE", "TYPE", { "Phaser", "Flanger" } }, { "RATE", "RATE", {} }, { "DEPTH", "DEPTH", {} },
            { "FEEDBACK", "FEEDBK", {} }, { "CENTER", "CENTER", {} }, { "STEREO", "STEREO", {} } } },
        { "FILTER", "Resonant filter with LFO and envelope", {
            { "TYPE", "TYPE", { "Low-pass", "High-pass", "Band-pass" } }, { "CUTOFF", "CUTOFF", {} },
            { "RESONANCE", "RESO", {} }, { "LFO RATE", "LFO", {} }, { "LFO DEPTH", "LFO DPT", {} }, { "ENVELOPE", "ENV", {} } } },
        { "DE-ESSER", "Tames sibilance and harsh highs", {
            { "FREQUENCY", "FREQ", {} }, { "THRESHOLD", "THRESH", {} }, { "MAX REDUCTION", "MAX RED", {} },
            { "MODE", "MODE", { "Split", "Wideband" } } } },
        { "GATE", "Closes the sound in the silences", {
            { "THRESHOLD", "THRESH", {} }, { "ATTACK", "ATTACK", {} }, { "HOLD", "HOLD", {} },
            { "RELEASE", "RELEASE", {} }, { "RANGE", "RANGE", {} } } },
        { "STEREO", "Width, mono bass, pan and doubler", {
            { "WIDTH", "WIDTH", {} }, { "MONO BASS", "MONO BS", {} }, { "PAN", "PAN", {} }, { "DOUBLER", "DOUBLER", {} } } },
        { "TREMOLO", "Pulsing volume or auto-pan", {
            { "RATE", "RATE", {} }, { "DEPTH", "DEPTH", {} }, { "SHAPE", "SHAPE", { "Sine", "Triangle", "Square" } },
            { "AUTO-PAN", "AUTOPAN", {} }, { "SYNC", "SYNC", { "Free", "1/2", "1/4", "1/8", "1/16", "1/8 T" } } } },
        { "LO-FI", "Bits, sample rate, noise, band and wow", {
            { "BITS", "BITS", {} }, { "SAMPLE RATE", "RATE", {} }, { "NOISE", "NOISE", {} },
            { "LOW CUT", "LO CUT", {} }, { "HIGH CUT", "HI CUT", {} }, { "WOW", "WOW", {} } } },
        { "LIMITER", "Catches peaks and raises loudness", {
            { "CEILING", "CEILING", {} }, { "GAIN", "GAIN", {} }, { "RELEASE", "RELEASE", {} } } },
    };
    return t;
}

const B& blk (int t) { const auto& tb = table(); return tb[(size_t) (t < 0 || t >= (int) tb.size() ? 0 : t)]; }
const P* prm (int t, int k) { const auto& b = blk (t); return k >= 0 && k < (int) b.params.size() ? &b.params[(size_t) k] : nullptr; }

std::string fmt (const char* f, double v) { char b[48]; std::snprintf (b, sizeof b, f, v); return b; }
std::string hz (float v) { return v >= 1000 ? fmt ("%.1f kHz", v / 1000.0) : fmt ("%.0f Hz", v); }
} // namespace

std::string blockName (int t) { return blk (t).name; }
std::string blockTagline (int t) { return blk (t).tagline; }
std::string paramName (int t, int k) { auto* p = prm (t, k); return p ? p->name : ""; }
std::string paramShort (int t, int k) { auto* p = prm (t, k); return p ? p->shortName : ""; }

std::string choiceName (int t, int k, int i)
{
    auto* p = prm (t, k);
    if (! p || p->choices.empty()) return std::to_string (i);
    return p->choices[(size_t) std::max (0, std::min (i, (int) p->choices.size() - 1))];
}

std::string formatValue (int t, int k, float v)
{
    const auto& ps = info (t).params[(size_t) std::max (0, std::min (k, kNumParams - 1))];
    if (! ps.used()) return "";
    if (ps.scale == Scale::Choice) return choiceName (t, k, (int) ps.clamp (v));
    if (ps.scale == Scale::Toggle) return v >= 0.5f ? "ON" : "OFF";
    const auto bt = (BlockType) t;
    // valores que desligam a função
    if ((bt == BlockType::EQ && k == 0 && v <= 20.5f) || (bt == BlockType::EQ && k == 5 && v >= 19900.0f)
        || (bt == BlockType::Estereo && k == 1 && v <= 20.5f) || (bt == BlockType::Estereo && k == 3 && v < 0.05f)
        || (bt == BlockType::LoFi && k == 3 && v <= 20.5f) || (bt == BlockType::LoFi && k == 4 && v >= 19900.0f))
        return "OFF";
    if (bt == BlockType::Estereo && k == 2)
    {
        const int p = (int) std::lround (v);
        return p == 0 ? "C" : (p < 0 ? "L" + std::to_string (-p) : "R" + std::to_string (p));
    }
    if (bt == BlockType::LoFi && k == 0) return fmt ("%.0f bit", v);
    const std::string u = ps.unit;
    if (u == "Hz") return hz (v);
    if (u == "dB") return fmt (std::abs (v) < 0.05f ? "0.0 dB" : "%+.1f dB", v);
    if (u == "ms") return v >= 1000 ? fmt ("%.2f s", v / 1000.0) : (v < 10 ? fmt ("%.1f ms", v) : fmt ("%.0f ms", v));
    if (u == "s") return fmt ("%.1f s", v);
    if (u == ":1") return fmt ("%.1f:1", v);
    if (u == "%") return fmt ("%.0f %%", v);
    return fmt ("%.0f", v);
}
} // namespace wr::ui
