// WYATT RACK — processador do plugin
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/Rack.h"
#include "Brain/LocalBrain.h"
#include "Brain/CloudBrain.h"
#include "Brain/Analysis.h"

namespace wr
{
enum class AiMode { Offline = 0, Hibrido = 1, Online = 2 };

struct LogEntry
{
    juce::String prompt, reply;
    bool online = false;     // respondido pela IA avançada
    bool ok = true;
};

struct FactoryTreatment { const char* name; const char* prompt; };
const std::vector<FactoryTreatment>& factoryTreatments();

class WyattRackProcessor final : public juce::AudioProcessor,
                                 public juce::ChangeBroadcaster
{
public:
    WyattRackProcessor();
    ~WyattRackProcessor() override;

    // ---- áudio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WYATT RACK"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- parâmetros
    juce::AudioProcessorValueTreeState apvts;
    static juce::String slotId (int slot, const char* what);       // "s1_tipo", "s1_p3"...
    juce::RangedAudioParameter* slotParam (int slot, const char* what) const;

    // ---- tratamento (thread da tela)
    Treatment currentTreatment() const;
    void applyTreatment (const Treatment& t, bool pushUndo = true);
    void setSlotType (int slot, int type);                 // troca o bloco do slot (valores padrão)
    void moveSlot (int from, int to);
    void clearSlot (int slot);
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void undo();
    void redo();
    void pushUndo();

    // ---- assistente
    void ask (const juce::String& prompt, bool forceOnline = false);
    bool isThinking() const { return thinking.load(); }
    const std::vector<LogEntry>& log() const { return chatLog; }
    juce::String lastPrompt() const { return lastPromptText; }
    AiMode aiMode() const { return mode; }
    void setAiMode (AiMode m);
    bool hasApiKey() const;
    juce::String apiKey() const;
    juce::String aiModel() const;
    void setApiKey (const juce::String& key, const juce::String& model);
    TrackProfile trackProfile() const { return analyzer.snapshot(); }
    bool adaptToTrack = true;

    // ---- biblioteca do usuário
    static juce::File libraryFolder();
    bool saveToLibrary (const juce::String& name);
    bool loadFromFile (const juce::File& f);
    juce::Array<juce::File> libraryFiles() const;

    // ---- medidores
    Rack rack;
    float slotActivity (int s) const { return rack.slots[(size_t) s].activity.load(); }

    // Testes: espera a IA online terminar (bloqueia).
    void waitForAssistant (int ms);
    // Testes: troca o endereço e a chave da IA online sem mexer no arquivo de configurações.
    CloudConfig cloudOverride; bool useCloudOverride = false;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (WyattRackProcessor*);
    void setParamReal (juce::RangedAudioParameter* p, float normalized);
    void addLog (LogEntry e);
    void finishAnswer (const juce::String& prompt, const BrainResult& local, const CloudResult* cloud, const Treatment& before);
    CloudConfig cloudConfig() const;
    std::unique_ptr<juce::PropertiesFile> openSettings() const;

    std::atomic<float>* inDb = nullptr;
    std::atomic<float>* outDb = nullptr;
    std::atomic<float>* bypassP = nullptr;
    TrackAnalyzer analyzer;
    juce::AudioBuffer<float> dbuf;
    int currentProgram = 0;

    LocalBrain brain;
    std::vector<Treatment> undoStack, redoStack;
    std::vector<LogEntry> chatLog;
    juce::String lastPromptText;
    AiMode mode = AiMode::Hibrido;
    std::atomic<bool> thinking { false };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::unique_ptr<juce::Thread> worker;
    juce::CriticalSection workerLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WyattRackProcessor)
};
} // namespace wr
