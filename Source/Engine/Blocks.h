// WYATT RACK — blocos de efeito
#pragma once
#include "Catalog.h"
#include <memory>

namespace wr
{
struct Ctx
{
    double bpm = 120.0;
};

class Block
{
public:
    virtual ~Block() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    // p = parâmetros em unidades reais (6), constantes no trecho (≤ 32 amostras).
    virtual void process (float* L, float* R, int n, const float* p, const Ctx& ctx) = 0;
};

std::unique_ptr<Block> createBlock (BlockType t);
} // namespace wr
