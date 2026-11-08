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

class Slot
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* L, float* R, int n, const Ctx& ctx);

    SlotParams params;
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

    std::array<Slot, kNumSlots> slots;
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
