// WYATT RACK — "Tratamento": a cadeia de blocos em unidades reais
#pragma once
#include "../Engine/Catalog.h"
#include <array>
#include <string>
#include <vector>

namespace wr
{
struct BlockState
{
    int type = 0;
    bool on = true;
    float mix = 100;                         // 0..100
    std::array<float, kNumParams> p {};      // unidades reais

    static BlockState defaults (BlockType t)
    {
        BlockState b; b.type = (int) t; b.on = true;
        const auto& bi = info (t);
        b.mix = bi.defaultMix;
        for (int i = 0; i < kNumParams; ++i) b.p[(size_t) i] = bi.params[(size_t) i].used() ? bi.params[(size_t) i].def : 0.0f;
        return b;
    }
    BlockType bt() const { return (BlockType) type; }
    void clampAll()
    {
        const auto& bi = info (type);
        for (int i = 0; i < kNumParams; ++i)
            p[(size_t) i] = bi.params[(size_t) i].used() ? bi.params[(size_t) i].clamp (p[(size_t) i]) : 0.0f;
        mix = mix < 0 ? 0 : (mix > 100 ? 100 : mix);
    }
    bool operator== (const BlockState& o) const
    {
        if (type != o.type || on != o.on || std::abs (mix - o.mix) > 1e-3f) return false;
        for (int i = 0; i < kNumParams; ++i) if (std::abs (p[(size_t) i] - o.p[(size_t) i]) > 1e-3f * std::max (1.0f, std::abs (p[(size_t) i]))) return false;
        return true;
    }
};

struct Treatment
{
    std::vector<BlockState> blocks;           // até 8, na ordem da cadeia
    bool empty() const { return blocks.empty(); }
    const BlockState* find (BlockType t) const
    {
        for (auto& b : blocks) if (b.type == (int) t) return &b;
        return nullptr;
    }
    bool operator== (const Treatment& o) const
    {
        if (blocks.size() != o.blocks.size()) return false;
        for (size_t i = 0; i < blocks.size(); ++i) if (! (blocks[i] == o.blocks[i])) return false;
        return true;
    }
    std::string chainText() const;           // "EQ → COMPRESSOR → REVERB (Igreja, 3,2 s)"
};

// Texto curto com as diferenças entre dois tratamentos (para "Ajustei: ...").
std::string describeChanges (const Treatment& before, const Treatment& after);
} // namespace wr
