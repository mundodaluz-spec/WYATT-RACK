// WYATT RACK — catálogo de blocos de efeito (tipos, parâmetros, faixas e unidades)
#pragma once
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace wr
{
constexpr int kNumSlots  = 8;
constexpr int kNumParams = 6;

// A ordem é fixa: o índice vai salvo nos projetos.
enum class BlockType : int
{
    Vazio = 0, EQ, Compressor, Saturacao, Reverb, Delay, Chorus, Modulacao,
    Filtro, DeEsser, Gate, Estereo, Tremolo, LoFi, Limitador,
    Count
};
constexpr int kNumTypes = (int) BlockType::Count;

enum class Scale { Linear, Log, Choice, Toggle };

struct ParamSpec
{
    const char* name = nullptr;     // nullptr = parâmetro não usado neste bloco
    const char* key  = nullptr;     // nome curto sem acento (IA, JSON)
    const char* unit = "";
    float min = 0, max = 1, def = 0;
    Scale scale = Scale::Linear;
    std::vector<const char*> choices {};

    bool used() const { return name != nullptr; }
    int numChoices() const { return (int) choices.size(); }

    float toReal (float n) const
    {
        n = n < 0 ? 0 : (n > 1 ? 1 : n);
        switch (scale)
        {
            case Scale::Log:    return min * std::pow (max / min, n);
            case Scale::Choice: return (float) std::lround (n * (float) (numChoices() - 1));
            case Scale::Toggle: return n >= 0.5f ? 1.0f : 0.0f;
            default:            return min + (max - min) * n;
        }
    }
    float toNorm (float v) const
    {
        switch (scale)
        {
            case Scale::Log:
            {
                v = v < min ? min : (v > max ? max : v);
                return std::log (v / min) / std::log (max / min);
            }
            case Scale::Choice:
            {
                const int k = numChoices() - 1;
                if (k <= 0) return 0;
                int i = (int) std::lround (v); i = i < 0 ? 0 : (i > k ? k : i);
                return (float) i / (float) k;
            }
            case Scale::Toggle: return v >= 0.5f ? 1.0f : 0.0f;
            default:
            {
                v = v < min ? min : (v > max ? max : v);
                return (v - min) / (max - min);
            }
        }
    }
    float clamp (float v) const
    {
        if (scale == Scale::Choice) { const int k = numChoices() - 1; int i = (int) std::lround (v); return (float) (i < 0 ? 0 : (i > k ? k : i)); }
        if (scale == Scale::Toggle) return v >= 0.5f ? 1.0f : 0.0f;
        return v < min ? min : (v > max ? max : v);
    }
    std::string format (float v) const;
};

struct BlockInfo
{
    BlockType type;
    const char* name;        // nome na tela
    const char* key;         // nome curto sem acento
    const char* description; // uma linha
    float defaultMix;        // 0..100
    std::array<ParamSpec, kNumParams> params;
};

const BlockInfo& info (BlockType t);
inline const BlockInfo& info (int t) { return info ((BlockType) (t < 0 || t >= kNumTypes ? 0 : t)); }

// Normaliza texto: minúsculas, sem acento (UTF-8 → ASCII).
std::string semAcento (const std::string& s);

// Procura um tipo pelo nome (com tolerância: "reverb", "reverberação", "eco"...). -1 se não achar.
int typeFromName (const std::string& name);
// Procura um parâmetro pelo nome dentro do tipo. -1 se não achar.
int paramFromName (BlockType t, const std::string& name);
} // namespace wr
