#include "Rack.h"
#include "Dsp.h"
#include <cstring>

namespace wr
{
static constexpr int kChunk = 32;

void Slot::prepare (double s, int maxBlock)
{
    sr = s; maxN = std::max (maxBlock, kChunk);
    for (int t = 1; t < kNumTypes; ++t)
    {
        if (! blocks[(size_t) t]) blocks[(size_t) t] = createBlock ((BlockType) t);
        blocks[(size_t) t]->prepare (sr, kChunk);
    }
    for (int c = 0; c < 2; ++c) { dry[c].assign ((size_t) kChunk, 0); wa[c].assign ((size_t) kChunk, 0); wb[c].assign ((size_t) kChunk, 0); }
    xfadeLen = std::max (1, (int) (0.03 * sr));
    reset();
}

void Slot::reset()
{
    for (int t = 1; t < kNumTypes; ++t) if (blocks[(size_t) t]) blocks[(size_t) t]->reset();
    first = true; xfadeLeft = 0;
}

void Slot::readTargets (float* out, int type) const
{
    const auto& bi = info (type);
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& ps = bi.params[(size_t) i];
        if (! ps.used() || params.p[i] == nullptr) { out[i] = 0.0f; continue; }
        float n = params.p[i]->load (std::memory_order_relaxed);
        if (macros != nullptr && (ps.scale == Scale::Linear || ps.scale == Scale::Log))
            n = std::clamp (n + macros->offset (index, i), 0.0f, 1.0f);
        out[i] = ps.toReal (n);
    }
}

void Slot::runBlock (int type, float* L, float* R, int n, const float* p, const Ctx& ctx)
{
    if (type <= 0 || type >= kNumTypes) return;
    blocks[(size_t) type]->process (L, R, n, p, ctx);
}

void Slot::process (float* L, float* R, int n, const Ctx& ctx)
{
    if (params.type == nullptr) return;
    const int targetType = std::clamp ((int) std::lround (params.type->load()), 0, kNumTypes - 1);  // índice do tipo
    const bool onT = params.on == nullptr || params.on->load() >= 0.5f;
    const float mixT = params.mix ? std::clamp (params.mix->load() / 100.0f + (macros ? macros->offset (index, kNumParams) : 0.0f), 0.0f, 1.0f) : 1.0f;

    float tgt[kNumParams];
    if (first)
    {
        curType = targetType; prevType = 0; xfadeLeft = 0;
        onG = onT ? 1.0f : 0.0f; mixG = mixT;
        readTargets (sm, curType);
        if (curType > 0) blocks[(size_t) curType]->reset();
        first = false;
    }
    if (targetType != curType)
    {
        prevType = curType;
        std::memcpy (prevSm, sm, sizeof sm);
        curType = targetType;
        readTargets (sm, curType);
        if (curType > 0) blocks[(size_t) curType]->reset();
        xfadeLeft = xfadeLen;
    }

    // Caminho direto: slot vazio ou desligado e parado → não toca no áudio.
    const bool idle = (curType == 0 || (! onT && onG <= 0.0f)) && xfadeLeft == 0;
    if (idle) { activity.store (0.0f); return; }

    readTargets (tgt, curType);
    const auto& bi = info (curType);
    const float onStep = (float) (1.0 / (0.02 * sr));
    const float mixStep = (float) (1.0 / (0.03 * sr));
    float peak = 0;

    for (int off = 0; off < n; off += kChunk)
    {
        const int m = std::min (kChunk, n - off);
        float* l = L + off; float* r = R + off;
        // suavização dos parâmetros contínuos (≈ 30 ms)
        const float a = (float) std::exp (-(double) m / (0.03 * sr));
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto& ps = bi.params[(size_t) i];
            if (! ps.used()) continue;
            if (ps.scale == Scale::Choice || ps.scale == Scale::Toggle) sm[i] = tgt[i];
            else
            {
                sm[i] = tgt[i] + a * (sm[i] - tgt[i]);
                if (std::abs (sm[i] - tgt[i]) <= 1e-5f * std::max (1.0f, std::abs (tgt[i]))) sm[i] = tgt[i];
            }
        }

        const bool steady = onT && onG >= 1.0f && mixG == mixT && mixT >= 1.0f && xfadeLeft == 0;
        if (steady)
        {   // 100% molhado e ligado: o bloco processa direto no buffer
            runBlock (curType, l, r, m, sm, ctx);
        }
        else
        {
            std::memcpy (dry[0].data(), l, sizeof (float) * (size_t) m);
            std::memcpy (dry[1].data(), r, sizeof (float) * (size_t) m);
            std::memcpy (wa[0].data(), l, sizeof (float) * (size_t) m);
            std::memcpy (wa[1].data(), r, sizeof (float) * (size_t) m);
            runBlock (curType, wa[0].data(), wa[1].data(), m, sm, ctx);
            if (xfadeLeft > 0)
            {
                std::memcpy (wb[0].data(), l, sizeof (float) * (size_t) m);
                std::memcpy (wb[1].data(), r, sizeof (float) * (size_t) m);
                runBlock (prevType, wb[0].data(), wb[1].data(), m, prevSm, ctx);
            }
            for (int i = 0; i < m; ++i)
            {
                float wl = wa[0][(size_t) i], wr = wa[1][(size_t) i];
                if (xfadeLeft > 0)
                {
                    const float t = 1.0f - (float) xfadeLeft / (float) xfadeLen;
                    wl = wb[0][(size_t) i] + t * (wl - wb[0][(size_t) i]);
                    wr = wb[1][(size_t) i] + t * (wr - wb[1][(size_t) i]);
                    --xfadeLeft;
                }
                if (mixG < mixT) mixG = std::min (mixT, mixG + mixStep);
                else if (mixG > mixT) mixG = std::max (mixT, mixG - mixStep);
                if (onT) onG = std::min (1.0f, onG + onStep); else onG = std::max (0.0f, onG - onStep);
                const float g = mixG * onG;
                const float dl = dry[0][(size_t) i], dr = dry[1][(size_t) i];
                l[i] = dl + g * (wl - dl);
                r[i] = dr + g * (wr - dr);
            }
        }
        for (int i = 0; i < m; ++i) peak = std::max (peak, std::max (std::abs (l[i]), std::abs (r[i])));
    }
    activity.store (peak);
}

// ------------------------------------------------------------------ Rack
void Rack::prepare (double s, int maxBlock)
{
    sr = s;
    for (auto& sl : slots) sl.prepare (sr, maxBlock);
    dry[0].assign ((size_t) std::max (maxBlock, 64), 0); dry[1].assign ((size_t) std::max (maxBlock, 64), 0);
    reset();
}

void Rack::reset()
{
    for (auto& sl : slots) sl.reset();
    inG = inGainDb ? dsp::dbToGain (inGainDb->load()) : 1.0f;
    outG = outGainDb ? dsp::dbToGain (outGainDb->load()) : 1.0f;
    byG = (bypass && bypass->load() >= 0.5f) ? 1.0f : 0.0f;
}

void Rack::process (float* L, float* R, int n, const Ctx& ctx)
{
    float pk = 0;
    for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
    inPeak.store (pk);

    const bool byT = bypass && bypass->load() >= 0.5f;
    if (byT && byG >= 1.0f) { outPeak.store (pk); return; }

    const bool needDry = byT || byG > 0.0f;
    if (needDry)
    {
        if ((int) dry[0].size() < n) { dry[0].resize ((size_t) n); dry[1].resize ((size_t) n); }
        std::memcpy (dry[0].data(), L, sizeof (float) * (size_t) n);
        std::memcpy (dry[1].data(), R, sizeof (float) * (size_t) n);
    }

    const float inT = inGainDb ? dsp::dbToGain (inGainDb->load()) : 1.0f;
    const float outT = outGainDb ? dsp::dbToGain (outGainDb->load()) : 1.0f;
    const float c = dsp::onePoleCoef (20.0, sr);

    auto applyGain = [&] (float& g, float target)
    {
        if (g == target && g == 1.0f) return;
        for (int i = 0; i < n; ++i)
        {
            g = target + c * (g - target);
            if (std::abs (g - target) < 1e-6f) g = target;
            L[i] *= g; R[i] *= g;
        }
    };

    applyGain (inG, inT);
    for (auto& sl : slots) sl.process (L, R, n, ctx);
    applyGain (outG, outT);

    if (needDry)
    {
        const float step = (float) (1.0 / (0.02 * sr));
        for (int i = 0; i < n; ++i)
        {
            if (byT) byG = std::min (1.0f, byG + step); else byG = std::max (0.0f, byG - step);
            L[i] += byG * (dry[0][(size_t) i] - L[i]);
            R[i] += byG * (dry[1][(size_t) i] - R[i]);
        }
    }
    float po = 0;
    for (int i = 0; i < n; ++i) po = std::max (po, std::max (std::abs (L[i]), std::abs (R[i])));
    outPeak.store (po);
}
} // namespace wr
