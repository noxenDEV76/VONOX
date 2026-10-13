#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Presets.h"
#include "engine/VocalChain.h"
#include "engine/VocalAnalyzer.h"

class VonoxProcessor : public juce::AudioProcessor {
public:
    VonoxProcessor();
    ~VonoxProcessor() override;

    void prepareToPlay(double sr, int block) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VONOX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return int(vonox::factoryPresets().size()); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int i) override { applyPreset(i); }
    const juce::String getProgramName(int i) override {
        const auto& p = vonox::factoryPresets();
        return (i >= 0 && i < int(p.size())) ? juce::String(p[size_t(i)].category) + " / " + p[size_t(i)].name : juce::String();
    }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- features used by the editor ----
    void applyPreset(int index);
    void runAutoMix();                          // analyses the captured input, then writes the parameters
    juce::String analysisText() const;          // last AUTO MIX result (thread-safe copy)
    bool analysisBusy() const { return busy.load(); }
    int  autoMixCounter() const { return autoMixDone.load(); }   // increments on every completed AUTO MIX
    void switchAB(int slot);                    // 0 = A, 1 = B
    int  activeSlot() const { return slot; }
    float capturedSeconds() const;

    void setParam(const char* id, float plain);
    juce::AudioProcessorValueTreeState apvts;
    vonox::engine::VocalChain chain;
    juce::Atomic<bool> dummy;

private:
    JUCE_DECLARE_WEAK_REFERENCEABLE(VonoxProcessor)
    vonox::params::Cache cache;

    // lock-free mono capture of the last ~20 s of input for AUTO MIX
    std::unique_ptr<std::atomic<float>[]> ring; int ringSize = 0; std::atomic<int> ringPos{0}; std::atomic<int> ringFilled{0};
    double sampleRate = 44100.0;

    std::atomic<bool> busy{false};
    std::atomic<int> autoMixDone{0};
    mutable juce::CriticalSection textLock; juce::String lastText = "Play your vocal, then press AUTO MIX.";
    juce::ThreadPool pool{1};

    int currentProgram = 0, slot = 0;
    std::unique_ptr<juce::XmlElement> slotState[2];

    void writeValues(const std::array<float, vonox::params::kNumFloats>& v);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VonoxProcessor)
};
