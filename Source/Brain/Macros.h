// WYATT RACK — macros automáticas: 4 knobs com nome (PESO, BRILHO, ESPAÇO...) que movem
// vários parâmetros do tratamento de uma vez. Na posição do meio (50%) o som é o tratamento como foi montado.
#pragma once
#include "Treatment.h"
#include <string>

namespace wr
{
// Monta as macros do tratamento olhando os blocos que existem e as palavras do pedido.
// Substitui as macros que já existiam; todas começam em 50%.
void autoMacros (Treatment& t, const std::string& prompt);

// Texto curto do que a macro move ("EQ grave, SATURAÇÃO drive, ...") para a dica da tela.
std::string macroDescription (const Treatment& t, int macroIndex);
} // namespace wr
