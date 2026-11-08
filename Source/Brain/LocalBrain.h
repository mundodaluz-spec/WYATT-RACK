// WYATT RACK — cérebro offline: entende pedidos em português e monta o tratamento
#pragma once
#include "Treatment.h"
#include <string>
#include <vector>

namespace wr
{
struct BrainResult
{
    Treatment treatment;
    std::string explanation;               // texto para a tela
    std::vector<std::string> understood;   // conceitos reconhecidos (nomes para a tela)
    std::vector<std::string> unknown;      // palavras que não entendeu
    float confidence = 0;                  // 0..1
    bool refinement = false;               // ajustou o que já existia
    bool reset = false;                    // pedido de "zerar"
    bool recognized() const { return ! understood.empty() || reset; }
};

class LocalBrain
{
public:
    BrainResult interpret (const std::string& text, const Treatment& current) const;
};

// Utilidades expostas para os testes
int fuzzyDistance (const std::string& a, const std::string& b);
} // namespace wr
