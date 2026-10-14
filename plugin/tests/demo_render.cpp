// Renders real editor frames while cycling presets (used for README media). Arg1 = output dir, arg2 = list of preset indices (comma)
#include "PluginProcessor.h"
#include "PluginEditor.h"
static void voice(juce::AudioBuffer<float>& b, double sr, long& t) {
    for (int i = 0; i < b.getNumSamples(); ++i, ++t) {
        const double s = t / sr, env = 0.55 + 0.45 * std::sin(6.28318 * 2.2 * s);
        float v = float(0.3 * env * (std::sin(6.28318 * 190 * s) + 0.5 * std::sin(6.28318 * 380 * s) + 0.3 * std::sin(6.28318 * 3200 * s) + 0.1 * std::sin(6.28318 * 7000 * s)));
        for (int c = 0; c < b.getNumChannels(); ++c) b.setSample(c, i, v);
    }
}
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    VonoxProcessor p; const double sr = 48000; const int blk = 2000;
    p.setPlayConfigDetails(2, 2, sr, blk); p.prepareToPlay(sr, blk);
    juce::MidiBuffer midi; juce::AudioBuffer<float> buf(2, blk); long t = 0;
    for (int i = 0; i < 80; ++i) { voice(buf, sr, t); p.processBlock(buf, midi); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
    juce::StringArray ids; ids.addTokens(argv[2], ",", ""); int n = 0;
    auto snap = [&]() {
        voice(buf, sr, t); p.processBlock(buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(38);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::File f = juce::File(argv[1]).getChildFile(juce::String::formatted("f%04d.png", n++)); juce::FileOutputStream fo(f); fo.setPosition(0); fo.truncate();
        juce::PNGImageFormat().writeImageToStream(img, fo);
    };
    for (int k = 0; k < 14; ++k) snap();
    for (auto& id : ids) { p.applyPreset(id.getIntValue()); for (int k = 0; k < 40; ++k) snap(); }
}
