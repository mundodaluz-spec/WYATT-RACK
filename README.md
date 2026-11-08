# WYATT RACK

Plugin da **WYATT Sounds**: o usuário descreve o som em português e o rack monta uma cadeia de até 8 blocos de efeito, todos com DSP próprio. VST3 + Standalone (Windows/Mac/Linux) e AU (Mac). C++17, JUCE 8.0.4, CMake.

## Como funciona
- **Motor** (`Source/Engine`): 14 blocos — EQ, compressor, saturação, reverb, delay, chorus, phaser/flanger, filtro, de-esser, gate, estéreo, tremolo, lo-fi e limitador — em 8 slots com troca sem clique, mix por bloco e latência zero.
- **Cérebro offline** (`Source/Brain/LocalBrain.cpp`): regras em português com tolerância a erros de digitação; estilos, instrumentos, ~150 artistas de referência, efeitos de caráter, números ("ratio 4", "1/8 pontuado") e refinamentos ("mais", "menos", "sem", "zera").
- **Análise da faixa** (`Analysis.cpp`): nível, pico e 5 faixas de frequência; ajusta limiares do compressor, de-esser, gate e limitador ao nível real.
- **IA avançada** (`CloudBrain.cpp`): API do Claude com saída JSON estruturada; só escolhe blocos e valores. Modo híbrido: offline primeiro, online quando o offline não entende (confiança < 75%), e volta para o offline se a internet ou a chave falharem.

## Compilar
```
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git ../JUCE
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/CoreTests && ./build/PluginTests_artefacts/Release/PluginTests capturas
```
O GitHub Actions (`.github/workflows/build.yml`) compila, testa e roda o pluginval nos três sistemas.
