// WYATT RACK — "Tratamento": a cadeia de blocos em unidades reais
#pragma once
#include "../Engine/Catalog.h"
#include <array>
#include <cmath>
#include <algorithm>
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

// ---- macros: knobs que movem vários parâmetros de uma vez
constexpr int kNumMacros = 4;
constexpr int kMacroMix  = -1;               // alvo "mix" do bloco

struct MacroTarget
{
    int block = 0;                            // posição do bloco na cadeia
    int param = kMacroMix;                    // 0..5 ou kMacroMix
    float depth = 0;                          // quanto anda, em unidades normalizadas (-1..1)
    bool operator== (const MacroTarget& o) const { return block == o.block && param == o.param && std::abs (depth - o.depth) < 1e-3f; }
};

struct Macro
{
    std::string name;                         // "PESO", "BRILHO"...
    float value = 0.5f;                       // 0..1; 0,5 = o tratamento como foi montado
    std::vector<MacroTarget> targets;
    bool used() const { return ! targets.empty(); }
    bool operator== (const Macro& o) const
    {
        return name == o.name && std::abs (value - o.value) < 1e-3f && targets == o.targets;
    }
};

// Pode a macro mexer neste parâmetro? (só contínuos: escolhas e liga/desliga ficam de fora)
inline bool macroCanTarget (int blockType, int param)
{
    if (param == kMacroMix) return blockType > 0;
    if (param < 0 || param >= kNumParams) return false;
    const auto& ps = info (blockType).params[(size_t) param];
    return ps.used() && (ps.scale == Scale::Linear || ps.scale == Scale::Log);
}

// Deslocamento normalizado de uma macro na posição v (0..1).
inline float macroOffset (float value, float depth) { return (value - 0.5f) * 2.0f * depth; }

struct Treatment
{
    std::vector<BlockState> blocks;           // até 8, na ordem da cadeia
    std::array<Macro, kNumMacros> macros;
    bool empty() const { return blocks.empty(); }
    bool hasMacros() const { for (auto& m : macros) if (m.used()) return true; return false; }
    // Remove alvos inválidos (bloco inexistente, parâmetro que não pode).
    void sanitizeMacros();
    // Aplica nos valores dos blocos o que as macros estão fazendo agora e zera as macros:
    // o som continua igual, mas sem depender dos knobs de macro.
    Treatment baked() const;
    // Reorganiza os alvos quando a cadeia muda: newIndexOf[antigo] = novo (-1 = bloco saiu).
    void remapMacros (const std::vector<int>& newIndexOf);
    const BlockState* find (BlockType t) const
    {
        for (auto& b : blocks) if (b.type == (int) t) return &b;
        return nullptr;
    }
    bool operator== (const Treatment& o) const
    {
        if (blocks.size() != o.blocks.size()) return false;
        for (size_t i = 0; i < blocks.size(); ++i) if (! (blocks[i] == o.blocks[i])) return false;
        for (int m = 0; m < kNumMacros; ++m) if (! (macros[(size_t) m] == o.macros[(size_t) m])) return false;
        return true;
    }
    std::string chainText() const;           // "EQ → COMPRESSOR → REVERB (Igreja, 3,2 s)"
};

// Texto curto com as diferenças entre dois tratamentos (para "Ajustei: ...").
std::string describeChanges (const Treatment& before, const Treatment& after);
} // namespace wr
