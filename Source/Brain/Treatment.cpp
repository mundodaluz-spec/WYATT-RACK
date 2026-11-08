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
