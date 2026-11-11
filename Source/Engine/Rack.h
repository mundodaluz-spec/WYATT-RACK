// WYATT RACK — a cadeia de 8 slots
#pragma once
#include "Blocks.h"
#include <array>
#include <atomic>
#include <vector>

namespace wr
{
// Ponteiros para os valores (normalizados 0..1) dos parâmetros de um slot.
struct SlotParams
{
    std::atomic<float>* type = nullptr;
    std::atomic<float>* on = nullptr;
    std::atomic<float>* mix = nullptr;     // 0..100 (valor real)
    std::atomic<float>* p[kNumParams] {};
};

// Macros no caminho do áudio: valor de cada knob (0..100, ponteiro do parâmetro) e quanto ele move
// cada parâmetro de cada slot (índice kNumParams = mix). Tudo atômico: a tela escreve, o áudio lê.
constexpr int kMaxMacros = 4;
struct MacroBus
{
    std::atomic<float>* value[kMaxMacros] {};
    std::atomic<float> depth[kMaxMacros][kNumSlots][kNumParams + 1] {};
    // Deslocamento normalizado do parâmetro k (kNumParams = mix) no slot s.
    float offset (int s, int k) const
    {
        float o = 0;
        for (int m = 0; m < kMaxMacros; ++m)
        {
            const float d = depth[m][s][k].load (std::memory_order_relaxed);
            if (d == 0.0f || value[m] == nullptr) continue;
            o += (value[m]->load (std::memory_order_relaxed) / 100.0f - 0.5f) * 2.0f * d;
        }
        return o;
    }
    void clear() { for (auto& a : depth) for (auto& b : a) for (auto& c : b) c.store (0.0f); }
};

class Slot
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* L, float* R, int n, const Ctx& ctx);

    SlotParams params;
    const MacroBus* macros = nullptr;      // opcional
    int index = 0;                         // posição no rack (para as macros)
    std::atomic<float> activity { 0.0f };  // nível de saída, para a tela

    // Leitura direta, para os testes.
    int currentType() const { return curType; }
private:
    void readTargets (float* out, int type) const;
    void runBlock (int type, float* L, float* R, int n, const float* p, const Ctx& ctx);

    double sr = 48000;
    int maxN = 512;
    std::array<std::unique_ptr<Block>, kNumTypes> blocks;
    int curType = 0, prevType = 0;
    int xfadeLeft = 0, xfadeLen = 1;
    float sm[kNumParams] {}, prevSm[kNumParams] {};
    float onG = 1.0f, mixG = 1.0f;
    bool first = true;
    std::vector<float> dry[2], wa[2], wb[2];
};

class Rack
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    // Processa no lugar (2 canais). Neutro = áudio idêntico.
    void process (float* L, float* R, int n, const Ctx& ctx);

    Rack() { for (int i = 0; i < kNumSlots; ++i) { slots[(size_t) i].macros = &macros; slots[(size_t) i].index = i; } }
    std::array<Slot, kNumSlots> slots;
    MacroBus macros;
    std::atomic<float>* inGainDb = nullptr;
    std::atomic<float>* outGainDb = nullptr;
    std::atomic<float>* bypass = nullptr;

    std::atomic<float> inPeak { 0 }, outPeak { 0 };
private:
    double sr = 48000;
    float inG = 1, outG = 1, byG = 0;
    std::vector<float> dry[2];
};
} // namespace wr
