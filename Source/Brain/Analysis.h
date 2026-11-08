// WYATT RACK — "ouvido" da faixa: nível, picos e equilíbrio de frequências, para a IA adaptar o tratamento
#pragma once
#include "Treatment.h"
#include <array>
#include <atomic>
#include <string>

namespace wr
{
constexpr int kNumBands = 5;   // graves (<150), médio-graves (150–500), médios (500–2k), presença (2–6k), agudos (>6k)

struct TrackProfile
{
    bool valid = false;           // ouviu o suficiente (≥ 1,5 s de som)
    float seconds = 0;            // tempo de som ouvido
    float rmsDb = -100;           // nível médio dos trechos com som
    float peakDb = -100;          // pico
    float crestDb = 0;            // pico − média (alto = muitos transientes)
    std::array<float, kNumBands> bandDb {};   // energia de cada faixa, relativa ao total (dB)
    std::string describe() const; // texto curto para a tela e para a IA online
};

// Roda na thread de áudio (sem alocação); a tela/IA lê o resumo quando quiser.
class TrackAnalyzer
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const float* L, const float* R, int n);
    TrackProfile snapshot() const;

private:
    struct Bq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0; float run (float x); };
    double sr = 48000;
    std::array<Bq, kNumBands> hp {}, lp {};   // cada faixa = passa-altas na borda de baixo + passa-baixas na de cima
    double bandAcc[kNumBands] {}, totAcc = 0, decay = 0.9999;
    double peakHold = 0;
    double voicedSamples = 0;
    float gateEnv = 0;
    std::atomic<float> outBand[kNumBands] {}, outRms { -100 }, outPeak { -100 }, outSecs { 0 };
};

// Ajusta limiares e ganhos do tratamento ao nível real da faixa. Devolve o que mudou ("" se nada).
std::string adaptToProfile (Treatment& t, const TrackProfile& p);
} // namespace wr
