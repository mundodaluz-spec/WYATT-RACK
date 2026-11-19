// WMusic Rack — visores dos painéis expandidos (um por tipo de bloco), desenhados ao vivo
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/Catalog.h"
#include <functional>
#include <vector>

namespace wr::ui
{
struct VizState
{
    int type = 0;
    float p[kNumParams] {};      // valores reais
    float mix = 100;
    bool on = true;
    float live = 0;              // ver Block::live
    float inDb = -100, outDb = -100;
    double bpm = 120;
    std::vector<float> scopeL, scopeR;    // últimas amostras de saída do slot
    std::vector<float> specDb;            // espectro da saída em kSpecBins faixas log (20 Hz–20 kHz), dB
    std::vector<float> hIn, hOut, hLive;  // histórico (~8 s), mais antigo primeiro
    static constexpr int kSpecBins = 200;
    static constexpr int kHist = 240;
};

class Visualizer : public juce::Component
{
public:
    VizState state;
    // muda um parâmetro (valor real), com gesto na DAW
    std::function<void (int param, float value)> setParam;
    std::function<void (int param)> beginDrag, endDrag;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    // alça arrastável: mexe em até 2 parâmetros (x e y)
    struct Handle { int px = -1, py = -1; juce::Point<float> pos; float radius = 12; };
    std::vector<Handle> handles;      // refeitas a cada pintura
    std::vector<std::pair<juce::Rectangle<float>, std::pair<int, int>>> tabs;   // área clicável → (parâmetro, escolha)
    int dragging = -1;
    void addHandle (int px, int py, juce::Point<float> pos, float r = 12) { handles.push_back ({ px, py, pos, r }); }
    void addTab (juce::Rectangle<float> r, int param, int choice) { tabs.push_back ({ r, { param, choice } }); }
    void applyDrag (const Handle&, juce::Point<float>);

    // mapeamentos usados pelas alças
    float freqToX (float f, float x0, float x1) const;
    float xToFreq (float x, float x0, float x1) const;
    juce::Rectangle<float> plot;      // área do gráfico atual (para as alças)
    float plotDbRange = 18;

    void paintEq (juce::Graphics&);
    void paintComp (juce::Graphics&);
    void paintSat (juce::Graphics&);
    void paintReverb (juce::Graphics&);
    void paintDelay (juce::Graphics&);
    void paintChorus (juce::Graphics&);
    void paintPhaser (juce::Graphics&);
    void paintFilter (juce::Graphics&);
    void paintDeEsser (juce::Graphics&);
    void paintGate (juce::Graphics&);
    void paintStereo (juce::Graphics&);
    void paintTremolo (juce::Graphics&);
    void paintLoFi (juce::Graphics&);
    void paintLimiter (juce::Graphics&);
};
} // namespace wr::ui
