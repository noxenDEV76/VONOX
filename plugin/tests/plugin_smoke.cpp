// Headless smoke test of the real plugin classes: processing, AUTO MIX, presets, A/B, state round-trip,
// editor construction and a snapshot of the editor to PNG (arg 1 = output dir).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } else std::printf("ok   %s\n", #c); } while (0)

static void voice(juce::AudioBuffer<float>& b, double sr, long& t) {
    for (int i = 0; i < b.getNumSamples(); ++i, ++t) {
        const double s = t / sr, env = 0.5 + 0.5 * std::sin(6.28318 * 3.0 * s);
        float v = float(0.25 * env * (std::sin(6.28318 * 180 * s) + 0.5 * std::sin(6.28318 * 360 * s) + 0.3 * std::sin(6.28318 * 3200 * s)));
        for (int c = 0; c < b.getNumChannels(); ++c) b.setSample(c, i, v);
    }
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    VonoxProcessor p; const double sr = 48000; const int blk = 512;
    p.setPlayConfigDetails(2, 2, sr, blk); p.prepareToPlay(sr, blk);
    juce::MidiBuffer midi; juce::AudioBuffer<float> buf(2, blk); long t = 0;

    // 3 s of "vocal" so AUTO MIX has material
    for (int i = 0; i < int(3 * sr / blk); ++i) { voice(buf, sr, t); p.processBlock(buf, midi); }
    CHECK(p.capturedSeconds() > 2.5f);
    bool finite = true; for (int c = 0; c < 2; ++c) for (int i = 0; i < blk; ++i) finite &= std::isfinite(buf.getSample(c, i));
    CHECK(finite);

    const float hpf0 = p.apvts.getRawParameterValue("hpf")->load();
    p.runAutoMix();
    for (int k = 0; k < 400 && p.autoMixCounter() == 0; ++k) { juce::Thread::sleep(10); juce::MessageManager::getInstance()->runDispatchLoopUntil(1); }
    CHECK(p.autoMixCounter() == 1);
    std::printf("analysis: %s\n", p.analysisText().toRawUTF8());
    (void) hpf0;

    p.applyPreset(3); CHECK(p.getCurrentProgram() == 3);
    const float warm = p.apvts.getRawParameterValue("warmth")->load();
    p.switchAB(1); p.setParam("warmth", warm + 1.5f); p.switchAB(0);
    CHECK(std::abs(p.apvts.getRawParameterValue("warmth")->load() - warm) < 0.02f);
    p.switchAB(1); CHECK(std::abs(p.apvts.getRawParameterValue("warmth")->load() - (warm + 1.5f)) < 0.02f);

    juce::MemoryBlock mb; p.getStateInformation(mb);
    VonoxProcessor q; q.setPlayConfigDetails(2, 2, sr, blk); q.prepareToPlay(sr, blk);
    q.setStateInformation(mb.getData(), int(mb.getSize()));
    CHECK(std::abs(q.apvts.getRawParameterValue("warmth")->load() - (warm + 1.5f)) < 0.02f);
    CHECK(q.activeSlot() == 1);
    q.switchAB(0); CHECK(std::abs(q.apvts.getRawParameterValue("warmth")->load() - warm) < 0.02f);

    p.setParam("bypass", 1.f);
    { std::vector<float> hin, hout;   // bypass = the input delayed by exactly the reported latency (bit-exact after the 10 ms fade)
      for (int i = 0; i < 60; ++i) { voice(buf, sr, t); for (int j = 0; j < blk; ++j) hin.push_back(buf.getSample(0, j)); p.processBlock(buf, midi); for (int j = 0; j < blk; ++j) hout.push_back(buf.getSample(0, j)); }
      const size_t L = size_t(p.getLatencySamples()); bool same = true; for (size_t i = size_t(40 * blk); i < hout.size(); ++i) same &= hout[i] == hin[i - L];
      CHECK(same); }
    p.setParam("bypass", 0.f);

    CHECK(p.getNumPrograms() >= 60);
    { // user presets: save, list, change, load, delete
      p.setParam("warmth", 2.25f); CHECK(p.saveUserPreset("VONOX smoke test")); CHECK(p.userPresetNames().contains("VONOX smoke test"));
      p.setParam("warmth", -1.f); CHECK(p.loadUserPreset("VONOX smoke test")); CHECK(std::abs(p.apvts.getRawParameterValue("warmth")->load() - 2.25f) < 0.02f);
      p.userPresetDir().getChildFile("VONOX smoke test.vonoxpreset").deleteFile(); }
    { // every preset (artist names, styles, vocal FX) applies and renders finite audio under the ceiling, stereo
      int artists = 0; bool allOk = true; const auto& ps = vonox::factoryPresets();
      for (int i = 0; i < int(ps.size()); ++i) {
          if (ps[size_t(i)].artist) ++artists;
          VonoxProcessor q2; q2.setPlayConfigDetails(2, 2, sr, blk); q2.prepareToPlay(sr, blk); q2.applyPreset(i); long t2 = 0; juce::AudioBuffer<float> b2(2, blk);
          for (int k = 0; k < 24; ++k) { voice(b2, sr, t2); q2.processBlock(b2, midi); }
          for (int c = 0; c < 2; ++c) for (int j = 0; j < blk; ++j) { const float v = b2.getSample(c, j); allOk &= std::isfinite(v) && std::abs(v) <= 1.0f; }
      }
      CHECK(allOk); CHECK(artists >= 38); }
    CHECK(p.getLatencySamples() == p.chain.latency() && p.getLatencySamples() > 500);
    CHECK(p.getNumPrograms() >= 80);
    { p.applyPreset(int(vonox::factoryPresets().size()) - 1); CHECK(p.getProgramName(p.getCurrentProgram()).isNotEmpty()); }

    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        CHECK(ed != nullptr && ed->getWidth() == 1100 && ed->getHeight() == 700);
        if (argc > 1) {
            auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            juce::File f = juce::File(argv[1]).getChildFile("editor_snapshot.png"); f.deleteFile();
            juce::FileOutputStream fo(f); juce::PNGImageFormat().writeImageToStream(img, fo);
            std::printf("snapshot %s\n", f.getFullPathName().toRawUTF8());
            p.applyPreset(6);
            if (auto* ve = dynamic_cast<VonoxEditor*>(ed.get())) {
                juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                auto i1 = ve->createComponentSnapshot(ve->getLocalBounds(), true, 1.0f);
                juce::File f1 = juce::File(argv[1]).getChildFile("editor_main.png"); f1.deleteFile(); { juce::FileOutputStream o1(f1); juce::PNGImageFormat().writeImageToStream(i1, o1); }
                ve->showBrowser(true); for (int k = 0; k < 12; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
                auto i3 = ve->createComponentSnapshot(ve->getLocalBounds(), true, 1.0f);
                juce::File f3 = juce::File(argv[1]).getChildFile("editor_presets.png"); f3.deleteFile(); { juce::FileOutputStream o3(f3); juce::PNGImageFormat().writeImageToStream(i3, o3); }
                ve->showBrowser(false);
                ve->showAdvanced(true); juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
                auto i2 = ve->createComponentSnapshot(ve->getLocalBounds(), true, 1.0f);
                juce::File f2 = juce::File(argv[1]).getChildFile("editor_adv.png"); f2.deleteFile(); { juce::FileOutputStream o2(f2); juce::PNGImageFormat().writeImageToStream(i2, o2); }
            }
        }
    }
    // optional: frame dump for the README animation (arg 2 = output dir): preset swipe, preset browser, advanced panel
    if (argc > 2) {
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor()); auto* ve = dynamic_cast<VonoxEditor*>(ed.get()); int n = 0;
        auto shot = [&](int blocks) {
            for (int k = 0; k < blocks; ++k) { voice(buf, sr, t); p.processBlock(buf, midi); }
            juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
            auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            juce::File fl = juce::File(argv[2]).getChildFile(juce::String::formatted("f%04d.png", n++)); fl.deleteFile();
            juce::FileOutputStream fo(fl); juce::PNGImageFormat().writeImageToStream(img, fo);
        };
        auto find = [](const char* nm) { const auto& ps = vonox::factoryPresets(); for (int i = 0; i < int(ps.size()); ++i) if (ps[size_t(i)].artist && juce::String(ps[size_t(i)].artist) == nm) return i; for (int i = 0; i < int(ps.size()); ++i) if (juce::String(ps[size_t(i)].name) == nm) return i; return 0; };
        for (const char* nm : { "Travis Scott", "Drake", "Doja Cat", "Booba", "Heavy Autotune" }) { p.applyPreset(find(nm)); for (int f = 0; f < 22; ++f) shot(4); }
        ve->showBrowser(true); for (int f = 0; f < 40; ++f) shot(4); ve->showBrowser(false);
        p.applyPreset(find("Natural Harmony")); ve->showAdvanced(true); for (int f = 0; f < 26; ++f) shot(4);
    }
    p.releaseResources();
    std::printf(fails ? "SMOKE FAILED (%d)\n" : "SMOKE PASSED\n", fails);
    return fails ? 1 : 0;
}
