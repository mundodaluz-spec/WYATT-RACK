// WYATT RACK — IA avançada (online): manda o pedido para a API do Claude e recebe o tratamento em JSON.
// O som continua 100% no DSP do WYATT RACK; a IA só escolhe blocos e valores.
#pragma once
#include "Treatment.h"
#include "Analysis.h"
#include <juce_core/juce_core.h>

namespace wr
{
// ---- tratamento ⇄ JSON (biblioteca, IA online, copiar/colar)
juce::var treatmentToVar (const Treatment& t);
// Aceita "params" como objeto {"limiar": -20} ou lista [{"nome": "limiar", "valor": -20}].
// Tipos e parâmetros por nome (com ou sem acento), escolhas por índice ou nome. Valores fora da faixa são limitados.
bool treatmentFromVar (const juce::var& v, Treatment& out, juce::String& error);
// Lê a lista "macros" ([{"nome": "PESO", "valor": 0.5, "alvos": [{"bloco": 0, "param": "grave", "quanto": 0.25}]}])
// para dentro de out.macros. Alvos inválidos são descartados. Devolve quantas macros ficaram.
int macrosFromVar (const juce::var& list, Treatment& out);

struct CloudConfig
{
    juce::String apiKey;
    juce::String model { "claude-haiku-5-5" };
    juce::String endpoint { "https://api.anthropic.com/v1/messages" };
    int timeoutMs = 25000;
};

struct CloudResult
{
    bool ok = false;
    Treatment treatment;
    bool reset = false;
    juce::String explanation;
    juce::String error;      // mensagem amigável em português
};

class CloudBrain
{
public:
    static juce::String systemPrompt();
    static juce::String requestBody (const CloudConfig& cfg, const juce::String& prompt, const Treatment& current, const TrackProfile& profile);
    static CloudResult parseResponse (int httpStatus, const juce::String& body);
    // Bloqueante: chamar fora da thread de áudio e da tela.
    static CloudResult ask (const CloudConfig& cfg, const juce::String& prompt, const Treatment& current, const TrackProfile& profile);
};
} // namespace wr
