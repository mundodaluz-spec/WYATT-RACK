// WMusic Rack — nomes em inglês para a tela e para a DAW (o cérebro continua em português)
#pragma once
#include "../Engine/Catalog.h"
#include <string>

namespace wr::ui
{
std::string blockName (int type);                 // "COMPRESSOR"
std::string blockTagline (int type);              // uma linha curta
std::string paramName (int type, int param);      // "THRESHOLD"
std::string paramShort (int type, int param);     // versão curta para knobs estreitos
std::string choiceName (int type, int param, int index);
std::string formatValue (int type, int param, float realValue);   // "−18.0 dB", "1/8 D", "OFF"
} // namespace wr::ui
