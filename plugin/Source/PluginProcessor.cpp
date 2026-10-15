#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace vonox;

VonoxProcessor::VonoxProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VONOX", params::createLayout()) {
    cache.bind(apvts);
    applyPreset(0);          // a fresh instance starts from a finished, musical sound (not a dry default)
    presetActive = false;
}
VonoxProcessor::~VonoxProcessor() { pool.removeAllJobs(true, 5000); }

bool VonoxProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto o = l.getMainOutputChannelSet();
    if (!(o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) || o != l.getMainInputChannelSet()) return false;
    const auto sc = l.getChannelSet(true, 1);       // the sidechain (unmasking) input is optional: disabled, mono or stereo
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void VonoxProcessor::prepareToPlay(double sr, int block) {
    sampleRate = sr;
    chain.prepare(sr, block);
    setLatencySamples(chain.latency());     // constant: every stage is always in the path, bypass is delay-matched
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
    t.space = cache.get(params::space); t.character = cache.get(params::character);
    t.voice = params::toVoice(cache);
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) t.bpm = *bpm;
    chain.setTarget(t);
    float* ch[2] = { buf.getWritePointer(0), nc > 1 ? buf.getWritePointer(1) : nullptr };
    const float* aux[2] = { nullptr, nullptr }; int auxCh = 0;
    if (auto* scBus = getBus(true, 1)) if (scBus->isEnabled()) {
        auto sc = getBusBuffer(buf, true, 1); auxCh = juce::jmin(2, sc.getNumChannels());
        for (int c = 0; c < auxCh; ++c) aux[c] = sc.getReadPointer(c);
    }
    chain.process(ch, nc, n, auxCh > 0 ? aux : nullptr, auxCh);
}

void VonoxProcessor::setParam(const char* id, float plain) {
    if (auto* p = apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(plain)); p->endChangeGesture(); }
}

void VonoxProcessor::writeValues(const std::array<float, params::kNumFloats>& v) {
    for (int i = 0; i < params::kNumFloats; ++i) if (!std::isnan(v[size_t(i)])) setParam(params::kFloats[i].id, v[size_t(i)]);
}

void VonoxProcessor::applyPreset(int index) {
    if (index < 0 || index >= int(factoryPresets().size())) return;
    resetToDefault();
    currentProgram = index; presetActive = true;
}

// Every parameter (floats, choices, toggles) back to its declared default = the "Default" preset.
void VonoxProcessor::resetToDefault() {
    for (auto* par : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(par)) {
            if (rp->paramID == juce::String(params::kBypassId)) continue;      // bypass is a monitoring switch, not part of a sound
            rp->beginChangeGesture(); rp->setValueNotifyingHost(rp->getDefaultValue()); rp->endChangeGesture();
        }
    currentProgram = 0; presetActive = true;
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
                    juce::String extra;
                    {
                        const juce::ScopedLock rl(p->refLock);
                        if (p->refProfile.valid) { engine::VocalMixEngine::applyReference(m, prof, p->refProfile); extra << "\nReference tone match applied (" << p->refLabel << "): presence / air / low-mid moved toward it (max 2.4 dB each)."; }
                    }
                    p->writeValues(params::fromSettings(m));
                    p->setParam(params::kSatModeId, float(int(m.satMode)));
                    p->setParam(params::kRvModeId, float(m.reverbMode)); p->setParam(params::kDlModeId, float(m.delayMode)); p->setParam(params::kDlDivId, float(m.delayDiv));
                    // measured clean-up suggestions (set from the analysis; you can change them in ADVANCED)
                    p->setParam("nr_amount", prof.noisy ? 0.45f : 0.f);
                    p->setParam("plosive", prof.rumble > 0.35 ? 0.4f : 0.f);
                    if (prof.noisy) extra << "\nNoisy recording detected: noise reduction set to 45 %.";
                    // key from the sung pitches seen so far
                    std::array<double, 12> hist{}; for (int i = 0; i < 12; ++i) hist[size_t(i)] = p->chain.meters.keyHist[size_t(i)].load();
                    const auto ke = engine::estimateKey(hist);
                    if (ke.valid && ke.confidence >= 0.5) {
                        p->setParam(params::kKeyId, float(ke.key)); p->setParam(params::kScaleId, ke.minor ? 2.f : 1.f);
                        extra << "\nKey detected: " << engine::keyName(ke.key) << (ke.minor ? " minor" : " major") << " (confidence " << juce::String(ke.confidence, 2) << ") - used by TUNE and the harmonies.";
                    } else extra << "\nKey: not enough sung pitch data to detect it (key left as set).";
                    const juce::ScopedLock sl(p->textLock); p->lastText = juce::String(prof.summary()) + extra;
                } else { const juce::ScopedLock sl(p->textLock); p->lastText = juce::String(prof.note); }
                p->presetActive = false; p->autoMixDone++; p->busy = false;
            }
        });
    });
}

void VonoxProcessor::loadReference(const juce::File& f) {
    if (!f.existsAsFile()) return;
    juce::WeakReference<VonoxProcessor> self(this);
    pool.addJob([self, f]() {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(f));
        engine::VocalProfile prof; juce::String label = f.getFileName();
        if (rd && rd->lengthInSamples > 0) {
            const int n = int(std::min<double>(double(rd->lengthInSamples), rd->sampleRate * 90.0));
            juce::AudioBuffer<float> b(int(rd->numChannels), n); rd->read(&b, 0, n, 0, true, true);
            std::vector<float> m(static_cast<size_t>(n)); for (int i = 0; i < n; ++i) { float a = 0; for (int c = 0; c < b.getNumChannels(); ++c) a += b.getSample(c, i); m[size_t(i)] = a / float(b.getNumChannels()); }
            prof = engine::VocalAnalyzer::analyze(m, rd->sampleRate);
            if (!prof.valid) label << " (unusable: " << juce::String(prof.note) << ")";
        } else label << " (could not be read)";
        juce::MessageManager::callAsync([self, prof, label, f]() {
            if (auto* p = self.get()) { { const juce::ScopedLock sl(p->refLock); p->refProfile = prof; p->refLabel = label; p->refPath = prof.valid ? f.getFullPathName() : juce::String(); }
                const juce::ScopedLock tl(p->textLock); p->lastText = prof.valid ? "Reference loaded: " + label + ". Press AUTO MIX to match its tone." : "Reference rejected: " + label; p->autoMixDone++; }
        });
    });
}
void VonoxProcessor::clearReference() { { const juce::ScopedLock sl(refLock); refProfile = {}; refPath = {}; refLabel = {}; } const juce::ScopedLock tl(textLock); lastText = "Reference cleared."; autoMixDone++; }
juce::String VonoxProcessor::referenceName() const { const juce::ScopedLock sl(refLock); return refProfile.valid ? refLabel : juce::String(); }

juce::File VonoxProcessor::userPresetDir() const { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("VONOX").getChildFile("User Presets"); }
juce::StringArray VonoxProcessor::userPresetNames() const {
    juce::StringArray r; for (auto& f : userPresetDir().findChildFiles(juce::File::findFiles, false, "*.vonoxpreset")) r.add(f.getFileNameWithoutExtension());
    r.sort(true); return r;
}
bool VonoxProcessor::saveUserPreset(const juce::String& name) {
    const auto clean = juce::File::createLegalFileName(name.trim()); if (clean.isEmpty()) return false;
    auto dir = userPresetDir(); if (!dir.createDirectory()) return false;
    auto xml = apvts.copyState().createXml(); return xml != nullptr && xml->writeTo(dir.getChildFile(clean + ".vonoxpreset"));
}
bool VonoxProcessor::loadUserPreset(const juce::String& name) {
    auto xml = juce::XmlDocument::parse(userPresetDir().getChildFile(name + ".vonoxpreset"));
    if (!xml || !xml->hasTagName(apvts.state.getType())) return false;
    apvts.replaceState(juce::ValueTree::fromXml(*xml)); presetActive = false; return true;
}
juce::String VonoxProcessor::keyText() const {
    std::array<double, 12> hist{}; for (int i = 0; i < 12; ++i) hist[size_t(i)] = chain.meters.keyHist[size_t(i)].load();
    const auto ke = engine::estimateKey(hist); if (!ke.valid || ke.confidence < 0.4) return {};
    return juce::String(engine::keyName(ke.key)) + (ke.minor ? " minor" : " major") + " (" + juce::String(ke.confidence, 2) + ")";
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
    { const juce::ScopedLock sl(refLock); if (refPath.isNotEmpty()) xml->setAttribute("referencePath", refPath); }
    const int other = 1 - slot;
    if (slotState[other]) { auto* o = new juce::XmlElement(*slotState[other]); o->setTagName("OTHER_SLOT"); xml->addChildElement(o); }
    copyXmlToBinary(*xml, dest);
}

void VonoxProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        currentProgram = xml->getIntAttribute("program", 0); slot = xml->getIntAttribute("slot", 0) ? 1 : 0;
        slotState[0].reset(); slotState[1].reset();
        if (xml->hasAttribute("referencePath")) loadReference(juce::File(xml->getStringAttribute("referencePath")));
        if (auto* o = xml->getChildByName("OTHER_SLOT")) { auto c = std::make_unique<juce::XmlElement>(*o); c->setTagName(apvts.state.getType().toString()); slotState[1 - slot] = std::move(c); xml->removeChildElement(o, true); }
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorEditor* VonoxProcessor::createEditor() { return new VonoxEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VonoxProcessor(); }
