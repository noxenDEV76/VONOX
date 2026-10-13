#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace vonox;

VonoxProcessor::VonoxProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VONOX", params::createLayout()) {
    cache.bind(apvts);
    setLatencySamples(0);
}
VonoxProcessor::~VonoxProcessor() { pool.removeAllJobs(true, 5000); }

bool VonoxProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void VonoxProcessor::prepareToPlay(double sr, int block) {
    sampleRate = sr;
    chain.prepare(sr, block);
    ringSize = int(sr * 20.0);
    ring.reset(new std::atomic<float>[size_t(ringSize)]);
    for (int i = 0; i < ringSize; ++i) ring[size_t(i)].store(0.f, std::memory_order_relaxed);
    ringPos = 0; ringFilled = 0;
}

float VonoxProcessor::capturedSeconds() const { return sampleRate > 0 ? float(ringFilled.load() / sampleRate) : 0.f; }

void VonoxProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&) {
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nc = juce::jmin(buf.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buf.clear(c, 0, n);
    if (nc == 0 || n == 0 || ring == nullptr) return;

    // capture mono input (pre-processing) for AUTO MIX
    {
        int p = ringPos.load(std::memory_order_relaxed);
        const float* l = buf.getReadPointer(0); const float* r = buf.getReadPointer(nc > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i) { ring[size_t(p)].store(0.5f * (l[i] + r[i]), std::memory_order_relaxed); if (++p >= ringSize) p = 0; }
        ringPos.store(p, std::memory_order_release);
        ringFilled.store(juce::jmin(ringSize, ringFilled.load(std::memory_order_relaxed) + n), std::memory_order_relaxed);
    }

    engine::ChainTarget t;
    t.base = params::toSettings(cache);
    t.mix = cache.get(params::mix) / 100.0;
    t.compress = cache.get(params::compress);
    t.outputDb = cache.get(params::output);
    t.bypass = cache.bypass->load(std::memory_order_relaxed) > 0.5f;
    t.levelMatch = cache.levelMatch->load(std::memory_order_relaxed) > 0.5f;
    chain.setTarget(t);
    float* ch[2] = { buf.getWritePointer(0), nc > 1 ? buf.getWritePointer(1) : nullptr };
    chain.process(ch, nc, n);
}

void VonoxProcessor::setParam(const char* id, float plain) {
    if (auto* p = apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(plain)); p->endChangeGesture(); }
}

void VonoxProcessor::writeValues(const std::array<float, params::kNumFloats>& v) {
    for (int i = 0; i < params::kNumFloats; ++i) if (!std::isnan(v[size_t(i)])) setParam(params::kFloats[i].id, v[size_t(i)]);
}

void VonoxProcessor::applyPreset(int index) {
    const auto& ps = factoryPresets();
    if (index < 0 || index >= int(ps.size())) return;
    const auto& pr = ps[size_t(index)];
    auto m = applyCharacter(engine::VocalMixEngine::compute(typicalProfile(), pr.style), pr);
    setParam(params::kStyleId, float(int(pr.style)));
    writeValues(params::fromSettings(m));
    setParam("compress", float(pr.compress)); setParam("mix", float(pr.mix));
    setParam(params::kSatModeId, float(int(m.satMode)));
    currentProgram = index;
}

void VonoxProcessor::runAutoMix() {
    if (busy.exchange(true)) return;
    const int filled = ringFilled.load(), pos = ringPos.load(std::memory_order_acquire);
    const int want = juce::jmin(filled, int(sampleRate * 15.0));
    std::vector<float> x(size_t(juce::jmax(want, 0)));
    for (int i = 0; i < want; ++i) { int idx = pos - want + i; if (idx < 0) idx += ringSize; x[size_t(i)] = ring[size_t(idx)].load(std::memory_order_relaxed); }
    const int styleIdx = juce::jlimit(0, 8, int(apvts.getRawParameterValue(params::kStyleId)->load() + 0.5f));
    const double sr = sampleRate;
    juce::WeakReference<VonoxProcessor> self(this);
    pool.addJob([self, x = std::move(x), sr, styleIdx]() mutable {
        auto prof = engine::VocalAnalyzer::analyze(x, sr);
        juce::MessageManager::callAsync([self, prof, styleIdx]() {
            if (auto* p = self.get()) {
                if (prof.valid) {
                    auto m = engine::VocalMixEngine::compute(prof, static_cast<engine::Style>(styleIdx));
                    p->writeValues(params::fromSettings(m));
                    p->setParam(params::kSatModeId, float(int(m.satMode)));
                    const juce::ScopedLock sl(p->textLock); p->lastText = juce::String(prof.summary());
                } else { const juce::ScopedLock sl(p->textLock); p->lastText = juce::String(prof.note); }
                p->autoMixDone++; p->busy = false;
            }
        });
    });
}

juce::String VonoxProcessor::analysisText() const { const juce::ScopedLock sl(textLock); return lastText; }

void VonoxProcessor::switchAB(int s) {
    s = s ? 1 : 0; if (s == slot) return;
    slotState[slot] = std::make_unique<juce::XmlElement>(*apvts.copyState().createXml());
    slot = s;
    if (slotState[slot]) apvts.replaceState(juce::ValueTree::fromXml(*slotState[slot]));
    // first visit of an empty slot: it starts as a copy of the other one
}

void VonoxProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto xml = apvts.copyState().createXml();
    xml->setAttribute("program", currentProgram); xml->setAttribute("slot", slot);
    const int other = 1 - slot;
    if (slotState[other]) { auto* o = new juce::XmlElement(*slotState[other]); o->setTagName("OTHER_SLOT"); xml->addChildElement(o); }
    copyXmlToBinary(*xml, dest);
}

void VonoxProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        currentProgram = xml->getIntAttribute("program", 0); slot = xml->getIntAttribute("slot", 0) ? 1 : 0;
        slotState[0].reset(); slotState[1].reset();
        if (auto* o = xml->getChildByName("OTHER_SLOT")) { auto c = std::make_unique<juce::XmlElement>(*o); c->setTagName(apvts.state.getType().toString()); slotState[1 - slot] = std::move(c); xml->removeChildElement(o, true); }
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorEditor* VonoxProcessor::createEditor() { return new VonoxEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VonoxProcessor(); }
