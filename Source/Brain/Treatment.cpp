#include "Treatment.h"
#include <cmath>
#include <algorithm>

namespace wr
{
static std::string lower (std::string s)
{
    // Os nomes do catálogo estão em MAIÚSCULAS com acento; para a explicação usamos como estão.
    return s;
}

static std::string blockSummary (const BlockState& b)
{
    const auto& bi = info (b.type);
    std::string s = bi.name;
    auto fmt = [&] (int i) { return bi.params[(size_t) i].format (b.p[(size_t) i]); };
    switch (b.bt())
    {
        case BlockType::Reverb:     s += " (" + fmt (0) + ", " + fmt (2) + ")"; break;
        case BlockType::Delay:      s += " (" + ((int) b.p[1] > 0 ? fmt (1) : fmt (0)) + (b.p[4] >= 0.5f ? ", ping-pong" : "") + ")"; break;
        case BlockType::Saturacao:  s += " (" + fmt (1) + ")"; break;
        case BlockType::Modulacao:  s += " (" + fmt (0) + ")"; break;
        case BlockType::Filtro:     s += " (" + fmt (0) + ")"; break;
        default: break;
    }
    if (! b.on) s += " [desligado]";
    return lower (s);
}

void Treatment::sanitizeMacros()
{
    for (auto& m : macros)
    {
        std::vector<MacroTarget> ok;
        for (auto t : m.targets)
        {
            if (t.block < 0 || t.block >= (int) blocks.size()) continue;
            if (! macroCanTarget (blocks[(size_t) t.block].type, t.param)) continue;
            t.depth = std::clamp (t.depth, -1.0f, 1.0f);
            if (std::abs (t.depth) < 1e-4f) continue;
            bool dup = false;
            for (auto& o : ok) if (o.block == t.block && o.param == t.param) dup = true;
            if (! dup && ok.size() < 12) ok.push_back (t);
        }
        m.targets = ok;
        m.value = std::clamp (m.value, 0.0f, 1.0f);
        if (! m.used()) { m.name.clear(); m.value = 0.5f; }
    }
}

Treatment Treatment::baked() const
{
    Treatment t = *this;
    for (auto& m : macros)
        for (auto& tg : m.targets)
        {
            if (tg.block < 0 || tg.block >= (int) t.blocks.size()) continue;
            auto& b = t.blocks[(size_t) tg.block];
            const float off = macroOffset (m.value, tg.depth);
            if (tg.param == kMacroMix) { b.mix = std::clamp (b.mix + off * 100.0f, 0.0f, 100.0f); continue; }
            if (! macroCanTarget (b.type, tg.param)) continue;
            const auto& ps = info (b.type).params[(size_t) tg.param];
            b.p[(size_t) tg.param] = ps.toReal (std::clamp (ps.toNorm (b.p[(size_t) tg.param]) + off, 0.0f, 1.0f));
        }
    t.macros = {};
    return t;
}

void Treatment::remapMacros (const std::vector<int>& newIndexOf)
{
    for (auto& m : macros)
    {
        std::vector<MacroTarget> ok;
        for (auto t : m.targets)
        {
            if (t.block < 0 || t.block >= (int) newIndexOf.size()) continue;
            const int ni = newIndexOf[(size_t) t.block];
            if (ni < 0) continue;
            t.block = ni; ok.push_back (t);
        }
        m.targets = ok;
    }
    sanitizeMacros();
}

std::string Treatment::chainText() const
{
    if (blocks.empty()) return "rack vazio";
    std::string s;
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        if (i) s += " → ";
        s += blockSummary (blocks[i]);
    }
    return s;
}

std::string describeChanges (const Treatment& a, const Treatment& b)
{
    std::vector<std::string> added, removed, changed;
    std::vector<bool> usedB (b.blocks.size(), false);
    for (auto& x : a.blocks)
    {
        int match = -1;
        for (size_t j = 0; j < b.blocks.size(); ++j) if (! usedB[j] && b.blocks[j].type == x.type) { match = (int) j; break; }
        if (match < 0) { removed.push_back (info (x.type).name); continue; }
        usedB[(size_t) match] = true;
        const auto& y = b.blocks[(size_t) match];
        if (x == y) continue;
        const auto& bi = info (x.type);
        std::string d;
        int count = 0;
        if (std::abs (x.mix - y.mix) > 0.5f)
        {
            char t[64]; std::snprintf (t, sizeof t, "mix %.0f → %.0f%%", x.mix, y.mix); d += t; ++count;
        }
        for (int i = 0; i < kNumParams && count < 2; ++i)
        {
            const auto& ps = bi.params[(size_t) i];
            if (! ps.used()) continue;
            const float va = x.p[(size_t) i], vb = y.p[(size_t) i];
            if (std::abs (va - vb) <= 1e-3f * std::max (1.0f, std::abs (va))) continue;
            std::string nm = ps.name;
            for (auto& c : nm) c = (char) std::tolower ((unsigned char) c);
            if (count) d += ", ";
            d += nm + " " + ps.format (va) + " → " + ps.format (vb);
            ++count;
        }
        if (x.on != y.on) { if (count) d += ", "; d += y.on ? "ligado" : "desligado"; }
        changed.push_back (std::string (bi.name) + (d.empty() ? "" : " (" + d + ")"));
    }
    for (size_t j = 0; j < b.blocks.size(); ++j) if (! usedB[j]) added.push_back (blockSummary (b.blocks[j]));

    std::string s;
    auto join = [] (const std::vector<std::string>& v)
    {
        std::string r;
        for (size_t i = 0; i < v.size(); ++i) { if (i) r += "; "; r += v[i]; }
        return r;
    };
    if (! changed.empty()) s += "Ajustei: " + join (changed) + ". ";
    if (! added.empty())   s += "Adicionei: " + join (added) + ". ";
    if (! removed.empty()) s += "Tirei: " + join (removed) + ". ";
    if (s.empty()) s = "Nada mudou. ";
    if (! s.empty() && s.back() == ' ') s.pop_back();
    return s;
}
} // namespace wr
