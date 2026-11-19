// Estresse: áudio numa thread, parâmetros aleatórios (e leitura dos textos) em outra, ao mesmo tempo.
#include "../Source/PluginProcessor.h"
#include <thread>
#include <cstdio>
using namespace wr;
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    WyattRackProcessor p;
    p.prepareToPlay (48000, 512);
    p.ask (juce::String::fromUTF8 ("voz gospel com reverb de igreja e delay"));
    std::atomic<bool> stop { false };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; juce::Random r (1);
        while (! stop.load())
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) b.setSample (c, i, r.nextFloat() * 0.5f - 0.25f);
            p.processBlock (b, m);
        }
    });
    juce::Random r (7);
    auto& params = p.getParameters();
    const auto end = juce::Time::getMillisecondCounter() + 15000;
    int n = 0;
    while (juce::Time::getMillisecondCounter() < end)
    {
        auto* prm = params[r.nextInt (params.size())];
        prm->setValueNotifyingHost (r.nextFloat());
        (void) prm->getCurrentValueAsText();
        if (++n % 5000 == 0) { juce::MemoryBlock mb; p.getStateInformation (mb); }
    }
    stop = true; audio.join();
    std::printf ("fuzz ok: %d mudanças\n", n);
    return 0;
}
