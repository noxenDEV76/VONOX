#include "PluginEditor.h"
#include "BinaryData.h"
using namespace vonox;

static const juce::Colour RED(255, 40, 60), VIOLET(150, 70, 255);
static constexpr float kPi = juce::MathConstants<float>::pi;

juce::Image VonoxEditor::res(const char* name) {
    int sz = 0; const char* d = BinaryData::getNamedResource(name, sz);
    return d ? juce::ImageFileFormat::loadFrom(d, size_t(sz)) : juce::Image();
}

VonoxKnob::VonoxKnob(const juce::Image& b, juce::Colour a, int rad, const juce::String& u, int d)
    : body(b), accent(a), r(rad), unit(u), dec(d) {
    setSliderStyle(juce::Slider::RotaryVerticalDrag); setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters(0.f, 1.f, true);   // only used for the drag mapping; the drawing is custom
    setMouseDragSensitivity(180); setScrollWheelEnabled(true); setPopupMenuEnabled(false);
}

void VonoxKnob::paint(juce::Graphics& g) {
    const auto c = juce::Point<float>(getWidth() * 0.5f, kTopPad + (getHeight() - kTopPad) * 0.5f);
    const float norm = float(valueToProportionOfLength(getValue()));
    const float a0 = 225.f * kPi / 180.f, a1 = a0 + 270.f * kPi / 180.f * norm, rr = float(r) + 7.f;
    juce::Path track; track.addCentredArc(c.x, c.y, rr, rr, 0.f, a0, a0 + 270.f * kPi / 180.f, true);
    g.setColour(juce::Colours::black.withAlpha(0.67f)); g.strokePath(track, juce::PathStrokeType(3.f));
    if (norm > 0.001f) {
        juce::Path val; val.addCentredArc(c.x, c.y, rr, rr, 0.f, a0, a1, true);
        g.setColour(accent.withAlpha(0.22f)); g.strokePath(val, juce::PathStrokeType(9.f));
        g.setColour(accent.withAlpha(0.35f)); g.strokePath(val, juce::PathStrokeType(6.f));
        g.setColour(accent); g.strokePath(val, juce::PathStrokeType(3.f));
    }
    g.drawImageAt(body, juce::roundToInt(c.x - body.getWidth() * 0.5f), juce::roundToInt(c.y - body.getHeight() * 0.5f));
    const float R = float(r);
    g.setColour(accent);
    g.drawLine(c.x + std::sin(a1) * R * .22f, c.y - std::cos(a1) * R * .22f, c.x + std::sin(a1) * R * .84f, c.y - std::cos(a1) * R * .84f, 3.f);
    if (isMouseOverOrDragging() || isMouseButtonDown()) {
        auto t = juce::String(getValue(), dec) + " " + unit;
        auto bub = juce::Rectangle<float>(0, 0, 74.f, 20.f).withCentre({ c.x, 10.f });
        g.setColour(juce::Colour(8, 0, 12).withAlpha(0.92f)); g.fillRoundedRectangle(bub, 4.f);
        g.setColour(accent); g.drawRoundedRectangle(bub, 4.f, 1.f);
        g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
        g.drawText(t.trim(), bub.toNearestInt(), juce::Justification::centred);
    }
}

VonoxEditor::VonoxEditor(VonoxProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      bypassBtn("bypass", res("btn_bypass_on_png"), res("btn_bypass_off_png")), aBtn("a", res("btn_a_on_png"), res("btn_a_off_png")),
      bBtn("b", res("btn_b_on_png"), res("btn_b_off_png")), levelBtn("level", res("btn_level_on_png"), res("btn_level_off_png")),
      advBtn("adv", res("btn_adv_on_png"), res("btn_adv_off_png")) {
    background = res("background_png"); fireTitle = res("fire_title_jpg"); fireAuto = res("fire_auto_jpg");
    setSize(1100, 700); setResizable(false, false);

    const auto b29 = res("knob_body_29_png"), b26 = res("knob_body_26_png");
    struct K { int cx, cy, r; int param; bool violet; const char* unit; int dec; };
    const K ks[] = {
        { 80, 215, 29, params::tune, false, "%", 0 }, { 211, 215, 29, params::presence, true, "dB", 1 },
        { 80, 360, 29, params::air, true, "dB", 1 },        { 211, 360, 29, params::warmth, false, "dB", 1 },
        { 889, 215, 29, params::space, false, "", 2 },      { 1020, 215, 29, params::character, true, "", 2 },
        { 889, 360, 29, params::width, true, "%", 0 },      { 1020, 360, 29, params::compress, false, "", 2 },
        { 746, 566, 26, params::mix, true, "%", 0 },        { 841, 566, 26, params::output, false, "dB", 1 },
    };
    for (const auto& k : ks) {
        auto kn = std::make_unique<VonoxKnob>(k.r == 29 ? b29 : b26, k.violet ? VIOLET : RED, k.r, k.unit, k.dec);
        const int half = k.r + 12;
        kn->setBounds(k.cx - half, k.cy - half - VonoxKnob::kTopPad, half * 2, half * 2 + VonoxKnob::kTopPad);
        auto* par = proc.apvts.getParameter(params::kFloats[k.param].id);
        kn->setNormalisableRange(juce::NormalisableRange<double>(par->getNormalisableRange().start, par->getNormalisableRange().end, 0.01, par->getNormalisableRange().skew));
        kn->setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
        knobAtt.push_back(std::make_unique<juce::SliderParameterAttachment>(*par, *kn, nullptr));
        if (k.param == params::mix) mixKnob = kn.get();
        addAndMakeVisible(*kn); knobs.push_back(std::move(kn));
    }
    // The 72 % readout in the octagon is a second view of MIX: hide the duplicate knob drag area by using it as a zone below.
    auto place = [this](juce::Component& c, int x, int y, int w, int h) { c.setBounds(x - 4, y - 4, w + 8, h + 8); addAndMakeVisible(c); };
    place(bypassBtn, 905, 512, 150, 34); place(aBtn, 905, 556, 70, 34); place(bBtn, 985, 556, 70, 34); place(levelBtn, 905, 600, 150, 34); place(advBtn, 487, 610, 126, 32);
    browser = std::make_unique<PresetBrowser>(proc); browser->setBounds(getLocalBounds()); addChildComponent(*browser);
    adv = std::make_unique<AdvancedPanel>(proc, background); adv->setBounds(18, 118, 1064, 304); addAndMakeVisible(*adv); adv->setVisible(false);
    advBtn.onClick = [this] { adv->setVisible(!adv->isVisible()); advBtn.setToggleState(adv->isVisible(), juce::dontSendNotification); advBtn.toFront(false); };
    advBtn.setTooltip("Advanced: every module parameter (EQ, compressors, de-esser, saturation, reverb, delay, doubler, chorus)");
    autoBtn.setBounds(428, 428, 244, 74); addAndMakeVisible(autoBtn);
    autoBtn.onClick = [this] { proc.runAutoMix(); };
    bypassAtt = std::make_unique<juce::ButtonParameterAttachment>(*proc.apvts.getParameter(params::kBypassId), bypassBtn);
    levelAtt = std::make_unique<juce::ButtonParameterAttachment>(*proc.apvts.getParameter(params::kLevelMatchId), levelBtn);
    aBtn.onClick = [this] { proc.switchAB(0); };
    bBtn.onClick = [this] { proc.switchAB(1); };
    aBtn.setTooltip("Slot A"); bBtn.setTooltip("Slot B: the first time you open it, it starts as a copy of A");
    bypassBtn.setTooltip("Bypass (10 ms crossfade, no latency)"); levelBtn.setTooltip("Level Match: compensates the loudness of the processing so A/B is fair");
    autoBtn.setTooltip("Analyses the audio captured while playing and sets the whole chain");
    aBtn.setToggleState(proc.activeSlot() == 0, juce::dontSendNotification); bBtn.setToggleState(proc.activeSlot() == 1, juce::dontSendNotification);
    statusLine = proc.analysisText();
    for (int i = 0; i < 70; ++i) parts.push_back({ 0, 0, 0, 0, -1.f, 1.f });
    startTimerHz(24);
}
VonoxEditor::~VonoxEditor() { stopTimer(); }

void VonoxEditor::cycleStyle(int dir) {
    proc.presetActive = false;
    if (auto* par = proc.apvts.getParameter(params::kStyleId)) {
        const int n = 9, cur = juce::roundToInt(par->convertFrom0to1(par->getValue())), nx = (cur + dir + n) % n;
        par->beginChangeGesture(); par->setValueNotifyingHost(par->convertTo0to1(float(nx))); par->endChangeGesture();
    }
}

void VonoxEditor::mouseUp(const juce::MouseEvent& e) {
    if (e.mouseWasDraggedSinceMouseDown()) return;
    const auto p = e.getEventRelativeTo(this).getPosition();
    if (leftArrow.contains(p)) cycleStyle(-1); else if (rightArrow.contains(p)) cycleStyle(1); else if (styleBox.contains(p)) showPresetMenu();
}

void VonoxEditor::timerCallback() {
    frame = (frame + 1) % 72;
    auto& m = proc.chain.meters;
    auto lvl = [](float db) { return juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f); };
    auto smooth = [](float& cur, float t) { cur = t > cur ? t : cur + (t - cur) * 0.18f; };
    smooth(mIn, lvl(m.inPeakDb.load())); smooth(mOut, lvl(m.outPeakDb.load()));
    smooth(mGr, juce::jlimit(0.f, 1.f, -m.gainReductionDb.load() / 18.f));
    if (proc.autoMixCounter() != lastAutoCount) { lastAutoCount = proc.autoMixCounter(); statusLine = proc.analysisText(); autoBtn.setTooltip(statusLine); }
    autoBtn.busyFlag = proc.analysisBusy();
    aBtn.setToggleState(proc.activeSlot() == 0, juce::dontSendNotification); bBtn.setToggleState(proc.activeSlot() == 1, juce::dontSendNotification);
    // ember particles inside the glass
    for (auto& q : parts) {
        if (q.life <= 0.f) { q = { 550.f + (rng.nextFloat() - .5f) * 200.f, 370.f + rng.nextFloat() * 10.f, -(0.6f + rng.nextFloat() * 1.6f), (rng.nextFloat() - .5f) * 0.6f, 1.f, 1.f + rng.nextFloat() * 2.2f }; }
        q.x += q.vx + std::sin((q.y + q.size * 40.f) * 0.05f) * 0.3f; q.y += q.vy; q.life -= 0.012f + q.size * 0.002f;
    }
    repaint(235, 14, 630, 88); repaint(424, 424, 252, 82); repaint(420, 140, 260, 250);
    repaint(40, 500, 360, 160); repaint(30, 424, 232, 24);
    if (autoBtn.busyFlag) autoBtn.repaint();
}

void VonoxEditor::paint(juce::Graphics& g) {
    g.drawImageAt(background, 0, 0);
    // animated fire strips (frames pre-rendered by tools/export_assets.py)
    const int th = 88, ah = 82;
    g.drawImage(fireTitle, 235, 14, 630, th, 0, frame * th, 630, th);
    g.drawImage(fireAuto, 424, 424, 252, ah, 0, frame * ah, 252, ah);

    // octagon glass contents
    juce::Path glass; for (int i = 0; i < 8; ++i) { const float a = (22.5f + 45.f * i) * kPi / 180.f; juce::Point<float> pt(550.f + 118.f * std::cos(a), 262.f + 118.f * std::sin(a)); if (i == 0) glass.startNewSubPath(pt); else glass.lineTo(pt); }
    glass.closeSubPath();
    {
        juce::Graphics::ScopedSaveState ss(g); g.reduceClipRegion(glass);
        for (auto& q : parts) if (q.life > 0.f) {
            const float a = juce::jlimit(0.f, 1.f, q.life);
            g.setColour(juce::Colour(255, 120, 120).withAlpha(0.18f * a)); g.fillEllipse(q.x - q.size * 2.2f, q.y - q.size * 2.2f, q.size * 4.4f, q.size * 4.4f);
            g.setColour(juce::Colour(255, 200, 190).withAlpha(0.85f * a)); g.fillEllipse(q.x - q.size * .5f, q.y - q.size * .5f, q.size, q.size);
        }
    }
    const juce::Colour pink(255, 205, 225);
    g.setColour(pink); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
    g.drawText("VOCAL PROFILE", 450, 172, 200, 18, juce::Justification::centred);
    g.setColour(juce::Colour(8, 0, 12).withAlpha(0.92f)); g.fillRoundedRectangle(styleBox.toFloat(), 4.f);
    g.setColour(juce::Colour(150, 50, 110)); g.drawRoundedRectangle(styleBox.toFloat(), 4.f, 1.f);
    const int si = juce::roundToInt(proc.apvts.getRawParameterValue(params::kStyleId)->load());
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(16.f, juce::Font::bold));
    g.drawText(juce::String(engine::styleName(static_cast<engine::Style>(juce::jlimit(0, 8, si)))).toUpperCase(), styleBox.reduced(26, 0), juce::Justification::centred);
    g.drawText(juce::CharPointer_UTF8("\xe2\x97\x80"), leftArrow, juce::Justification::centred); g.drawText(juce::CharPointer_UTF8("\xe2\x96\xb6"), rightArrow, juce::Justification::centred);
    if (proc.presetActive.load()) {
        g.setColour(juce::Colour(255, 120, 170)); g.setFont(juce::FontOptions(11.f, juce::Font::bold));
        g.drawFittedText(juce::String(displayName(factoryPresets()[size_t(proc.getCurrentProgram())])).toUpperCase(), 446, 232, 208, 18, juce::Justification::centred, 1);
    }
    g.setColour(pink); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
    g.drawText("MIX AMOUNT", 450, 262, 200, 18, juce::Justification::centred);
    const juce::String pct = juce::String(juce::roundToInt(mixKnob->getValue())) + " %";
    g.setFont(juce::FontOptions(40.f, juce::Font::bold));
    g.setColour(RED.withAlpha(0.35f)); g.drawText(pct, 446, 290, 208, 56, juce::Justification::centred);
    g.setColour(juce::Colour(255, 225, 230)); g.drawText(pct, 450, 290, 200, 56, juce::Justification::centred);
    g.setFont(juce::FontOptions(10.5f)); g.setColour(pink.withAlpha(0.85f));
    auto line = statusLine.upToFirstOccurrenceOf("\n", false, false);
    g.drawFittedText(line, 452, 346, 196, 30, juce::Justification::centredTop, 2);

    // meters (same geometry as the artwork's empty frames)
    auto drawMeter = [&](int x, int y, int w, int h, float level) {
        const int segs = 26; const float sw = (w - 8) / float(segs);
        for (int i = 0; i < segs; ++i) {
            const bool on = (i / float(segs)) < level;
            const juce::Colour col = i < segs * 0.6 ? VIOLET : (i < segs * 0.85 ? juce::Colour(255, 80, 150) : RED);
            juce::Rectangle<float> r(x + 4 + i * sw, float(y + 4), sw - 2, float(h - 8));
            if (on) { g.setColour(col.withAlpha(0.25f)); g.fillRect(r.expanded(2)); }
            g.setColour(on ? col : juce::Colour(34, 28, 44)); g.fillRect(r);
        }
    };
    {   // measured readouts: detected key (pitch histogram) in the VOICE panel, true-peak under the OUTPUT meter
        g.setFont(juce::FontOptions(11.f, juce::Font::bold)); g.setColour(juce::Colour(255, 205, 225));
        const auto kt = proc.keyText(); g.drawText("KEY  " + (kt.isEmpty() ? juce::String("--") : kt), 30, 428, 230, 16, juce::Justification::centred);
        const float tp = proc.chain.meters.truePeakDb.load();
        g.setColour(tp > -0.5f ? RED : juce::Colour(255, 205, 225).withAlpha(0.9f)); g.setFont(juce::FontOptions(10.5f));
        g.drawText(juce::String("TRUE PEAK ") + (tp < -90.f ? juce::String("--") : juce::String(tp, 1)) + " dBTP", 220, 560, 170, 14, juce::Justification::centredRight);
        g.setColour(juce::Colour(255, 205, 225).withAlpha(0.8f));
        g.drawText("LATENCY " + juce::String(proc.getLatencySamples() * 1000.0 / juce::jmax(1.0, proc.getSampleRate()), 1) + " ms", 220, 508, 170, 14, juce::Justification::centredRight);
    }
    drawMeter(44, 522, 346, 22, mIn); drawMeter(44, 574, 346, 22, mOut); drawMeter(44, 626, 346, 22, mGr);
}

// ---------------------------------------------------------------- ADVANCED panel
void AdvancedPanel::Content::paint(juce::Graphics& g) {
    for (auto& l : labels) { g.setColour(juce::Colour(255, 205, 225).withAlpha(0.9f)); g.setFont(juce::FontOptions(10.5f, juce::Font::bold)); g.drawText(l.first, l.second, juce::Justification::centredLeft); }
    for (auto& h : heads) {
        g.setColour(juce::Colour(150, 70, 255)); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
        g.drawText(h.first, h.second, juce::Justification::centredLeft);
        g.setColour(juce::Colour(150, 70, 255).withAlpha(0.5f)); g.drawHorizontalLine(h.second.getBottom() - 1, float(h.second.getX()), float(h.second.getRight()));
    }
}

AdvancedPanel::AdvancedPanel(VonoxProcessor& p, const juce::Image& b) : proc(p), bg(b) {
    using namespace params;
    struct G { const char* title; std::vector<int> ids; std::vector<const char*> combos; std::vector<const char*> toggles; bool refButtons = false; };
    const std::vector<G> groups = {
        { "PITCH CORRECTION (TUNE knob = amount)", { retune, humanize }, { kKeyId, kScaleId }, {} },
        { "HARMONIZER + OCTAVES (in-key, wet voices)", { harm1Gain, harm2Gain, octDown, octUp }, { kHarm1Id, kHarm2Id }, {} },
        { "INPUT + RESTORATION", { inputGain, autoLevelTarget, nrAmount, breathAmount, plosiveAmount }, {}, { kAutoLevelId } },
        { "DYNAMIC EQ, UNMASKING, BAND-LIMIT, TRACK ROLE", { dynEqHz, dynEqAmount, dynEqThresh, unmask, bandLimit }, {}, { kAdlibId }, true },
        { "SUBTRACTIVE EQ + COMPRESSORS", { hpf, lowMidCut, lowMidHz, boxCut, harshCut, harshHz, fetThresh, fetRatio, fetAttack, fetRelease, optoThresh, optoRatio, optoAttack, optoRelease, presenceHz }, {}, {} },
        { "DE-ESSER, RESONANCE, SATURATION, OUTPUT", { deEssHz, deEssThresh, deEssAmount, resAmount, resThresh, satDrive, exciter, parComp, ceiling }, { kSatModeId }, {} },
        { "REVERB", { rvAmount, rvDecay, rvPre, rvDamp, rvHp, rvLp, duck }, { kRvModeId }, {} },
        { "DELAY, DOUBLER, CHORUS", { dlAmount, dlMs, dlFb, dlHp, dlLp, doubler, chorus }, { kDlModeId, kDlDivId }, {} },
    };
    const int cellW = 100, cellH = 80, perRow = 10; int y = 4;
    auto styleCombo = [](juce::ComboBox& c) {
        c.setColour(juce::ComboBox::backgroundColourId, juce::Colour(14, 6, 24)); c.setColour(juce::ComboBox::outlineColourId, juce::Colour(150, 70, 255));
        c.setColour(juce::ComboBox::textColourId, juce::Colours::white); c.setColour(juce::ComboBox::arrowColourId, juce::Colour(255, 205, 225));
    };
    auto addCombo = [&](const char* id, int x, int yy) {
        auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id)); if (!par) return;
        content.labels.push_back({ par->getName(30), { x + 4, yy, cellW * 2 - 12, 16 } });
        auto c = std::make_unique<juce::ComboBox>(); c->addItemList(par->choices, 1); c->setBounds(x + 4, yy + 18, cellW * 2 - 12, 26); styleCombo(*c);
        cAtt.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*par, *c, nullptr));
        content.addAndMakeVisible(*c); combos.push_back(std::move(c));
    };
    for (auto& g : groups) {
        content.heads.push_back({ g.title, { 0, y, 1040, 20 } }); y += 24;
        int col = 0;
        for (auto* cid : g.combos) { addCombo(cid, col * cellW, y); col += 2; }
        for (auto* tid : g.toggles) {
            auto* par = proc.apvts.getParameter(tid); if (!par) continue;
            auto t = std::make_unique<juce::ToggleButton>(par->getName(30)); t->setBounds(col * cellW + 4, y + 18, cellW * 2 - 12, 26);
            t->setColour(juce::ToggleButton::textColourId, juce::Colours::white); t->setColour(juce::ToggleButton::tickColourId, juce::Colour(150, 70, 255)); t->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(150, 70, 255));
            tAtt.push_back(std::make_unique<juce::ButtonParameterAttachment>(*par, *t)); content.addAndMakeVisible(*t); toggles.push_back(std::move(t)); col += 2;
        }
        if (g.refButtons) {
            auto mk = [&](const char* txt, int x, std::function<void()> fn, const char* tip) {
                auto bt = std::make_unique<juce::TextButton>(txt); bt->setBounds(x + 4, y + 18, cellW * 2 - 12, 26); bt->onClick = std::move(fn); bt->setTooltip(tip);
                bt->setColour(juce::TextButton::buttonColourId, juce::Colour(14, 6, 24)); bt->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
                content.addAndMakeVisible(*bt); buttons.push_back(std::move(bt));
            };
            mk("Load reference...", col * cellW, [this] {
                chooser = std::make_unique<juce::FileChooser>("Choose a reference vocal (WAV / AIFF / FLAC / OGG)", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadReference(fc.getResult()); });
            }, "Analyses a reference recording; AUTO MIX then moves presence / air / low-mid toward its tonal balance (max 2.4 dB each)");
            mk("Clear reference", (col + 2) * cellW, [this] { proc.clearReference(); }, "Removes the reference");
            col += 4;
        }
        for (int id : g.ids) {
            if (col >= perRow) { col = 0; y += cellH; }
            const auto& d = kFloats[id];
            auto k = std::make_unique<MiniKnob>(d.name, d.unit, d.hi - d.lo > 100 ? 0 : (d.hi - d.lo > 10 ? 1 : 2), juce::Colour(255, 40, 60));
            auto* par = proc.apvts.getParameter(d.id);
            k->setNormalisableRange(juce::NormalisableRange<double>(d.lo, d.hi, 0.01, d.skew));
            k->setDoubleClickReturnValue(true, d.def);
            k->setBounds(col * cellW, y, cellW, cellH);
            kAtt.push_back(std::make_unique<juce::SliderParameterAttachment>(*par, *k, nullptr));
            content.addAndMakeVisible(*k); knobs.push_back(std::move(k)); ++col;
        }
        y += cellH + 10;
    }
    content.setSize(1040, y); vp.setViewedComponent(&content, false); vp.setScrollBarsShown(true, false); addAndMakeVisible(vp);
    vp.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, juce::Colour(150, 70, 255));
}

void AdvancedPanel::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(8, 3, 14).withAlpha(0.97f)); g.fillRoundedRectangle(r, 8.f);
    g.setColour(juce::Colour(150, 70, 255)); g.drawRoundedRectangle(r.reduced(1.f), 8.f, 2.f);
    g.setColour(juce::Colour(255, 205, 225)); g.setFont(juce::FontOptions(15.f, juce::Font::bold));
    g.drawText("ADVANCED  -  every module parameter (all automatable)", 18, 8, getWidth() - 36, 20, juce::Justification::centredLeft);
}

// ---------------------------------------------------------------- PRESET BROWSER (black, white names, white particles)
PresetBrowser::PresetBrowser(VonoxProcessor& p) : proc(p) {
    setWantsKeyboardFocus(true); setInterceptsMouseClicks(true, true);
    for (int i = 0; i < 130; ++i) parts.push_back({ rng.nextFloat() * 1100.f, rng.nextFloat() * 700.f, -(0.08f + rng.nextFloat() * 0.35f), (rng.nextFloat() - .5f) * 0.12f, 0.25f + rng.nextFloat() * 0.65f, 0.9f + rng.nextFloat() * 1.5f, rng.nextFloat() * 6.28f });
}

void PresetBrowser::refresh() {
    cats.clear();   // artists first, then generic styles and effects, then the user's own presets
    for (const char* c : { "US Rap", "US Female Rap", "French Rap", "French Female", "Vocal Styles", "Vocal FX" })
        for (const auto& pr : factoryPresets()) if (juce::String(pr.category) == c) { cats.add(c); break; }
    for (const auto& pr : factoryPresets()) if (!cats.contains(pr.category)) cats.add(pr.category);
    cats.add("My Presets");
    // start on the category of the active factory preset
    const int cp = proc.getCurrentProgram(); const auto& ps = factoryPresets();
    if (proc.presetActive.load() && cp >= 0 && cp < int(ps.size())) selCat = juce::jmax(0, cats.indexOf(ps[size_t(cp)].category));
    selCat = juce::jlimit(0, cats.size() - 1, selCat); scroll = 0.f; rebuildRows();
}

void PresetBrowser::rebuildRows() {
    rows.clear(); const auto& ps = factoryPresets();
    if (cats[selCat] == "My Presets") {
        rows.push_back({ "+  Save current sound as preset...", "stored in your user folder, available in every project", -1, {}, true });
        for (auto& n : proc.userPresetNames()) rows.push_back({ n, "your preset", -1, n, false });
    } else for (int i = 0; i < int(ps.size()); ++i) if (cats[selCat] == ps[size_t(i)].category) {
        const auto& pr = ps[size_t(i)];
        rows.push_back({ displayName(pr), pr.artist ? juce::String(pr.name) : juce::String(), i, {}, false });
    }
    hoverRow = -1;
}

void PresetBrowser::timerCallback() {
    for (auto& q : parts) {
        q.ph += 0.03f; q.y += q.vy; q.x += q.vx + std::sin(q.ph) * 0.06f;
        if (q.y < -4.f) { q.y = 704.f; q.x = rng.nextFloat() * 1100.f; }
        if (q.x < -4.f) q.x = 1104.f; else if (q.x > 1104.f) q.x = -4.f;
    }
    repaint();
}

void PresetBrowser::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.62f));                              // dims the plugin behind
    auto pn = panel().toFloat();
    g.setColour(juce::Colours::black); g.fillRoundedRectangle(pn, 10.f);           // pure black panel
    {
        juce::Graphics::ScopedSaveState ss(g); juce::Path clip; clip.addRoundedRectangle(pn, 10.f); g.reduceClipRegion(clip);
        for (auto& q : parts) {                                                    // small white particles, only inside the panel
            const float tw = 0.5f + 0.5f * std::sin(q.ph * 1.7f);
            g.setColour(juce::Colours::white.withAlpha(q.a * (0.4f + 0.6f * tw))); g.fillEllipse(q.x - q.size * .5f, q.y - q.size * .5f, q.size, q.size);
        }
    }
    g.setColour(juce::Colours::white.withAlpha(0.55f)); g.drawRoundedRectangle(pn.reduced(0.5f), 10.f, 1.2f);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(22.f, juce::Font::bold));
    g.drawText("PRESETS", panel().withTrimmedLeft(24).withHeight(60), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(11.f)); g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawText("creative references - not the artists' own chains, not endorsed", panel().withTrimmedLeft(150).withTrimmedRight(60).withHeight(60), juce::Justification::centredLeft);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(20.f, juce::Font::bold));
    g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x95"), panel().withTrimmedTop(10).removeFromTop(40).removeFromRight(50), juce::Justification::centred);
    g.setColour(juce::Colours::white.withAlpha(0.25f)); g.drawHorizontalLine(panel().getY() + 64, pn.getX() + 16, pn.getRight() - 16);
    // categories
    auto ca = catArea(); g.setFont(juce::FontOptions(15.f, juce::Font::bold));
    for (int i = 0; i < cats.size(); ++i) {
        auto r = juce::Rectangle<int>(ca.getX() + 14, ca.getY() + i * 42, ca.getWidth() - 14, 38);
        if (i == selCat) { g.setColour(juce::Colours::white.withAlpha(0.14f)); g.fillRoundedRectangle(r.toFloat(), 6.f); g.setColour(juce::Colours::white); g.fillRect(r.getX() - 8, r.getY() + 6, 3, r.getHeight() - 12); }
        else if (i == hoverCat) { g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRoundedRectangle(r.toFloat(), 6.f); }
        g.setColour(juce::Colours::white.withAlpha(i == selCat ? 1.f : 0.75f)); g.drawText(cats[i], r.withTrimmedLeft(10), juce::Justification::centredLeft);
    }
    // list
    auto la = listArea(); juce::Graphics::ScopedSaveState ss(g); g.reduceClipRegion(la);
    const int cp = proc.getCurrentProgram(); const bool active = proc.presetActive.load();
    for (int i = 0; i < int(rows.size()); ++i) {
        auto r = juce::Rectangle<int>(la.getX(), la.getY() + int(i * rowH - scroll), la.getWidth() - 6, rowH - 4);
        if (r.getBottom() < la.getY() || r.getY() > la.getBottom()) continue;
        const bool cur = active && rows[size_t(i)].program == cp && rows[size_t(i)].program >= 0;
        if (i == hoverRow) { g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRoundedRectangle(r.toFloat(), 6.f); }
        if (cur) { g.setColour(juce::Colours::white.withAlpha(0.9f)); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 6.f, 1.4f); }
        g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(17.f, juce::Font::bold));
        g.drawText(rows[size_t(i)].title, r.withTrimmedLeft(14).withTrimmedRight(rows[size_t(i)].user.isNotEmpty() ? 40 : 0), rows[size_t(i)].sub.isEmpty() ? juce::Justification::centredLeft : juce::Justification::topLeft);
        if (rows[size_t(i)].sub.isNotEmpty()) { g.setColour(juce::Colours::white.withAlpha(0.5f)); g.setFont(juce::FontOptions(11.5f)); g.drawText(rows[size_t(i)].sub, r.withTrimmedLeft(14).withTrimmedTop(24), juce::Justification::topLeft); }
        if (rows[size_t(i)].user.isNotEmpty()) { g.setColour(juce::Colours::white.withAlpha(0.6f)); g.setFont(juce::FontOptions(15.f)); g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x95"), r.removeFromRight(36), juce::Justification::centred); }
    }
    if (rows.empty()) { g.setColour(juce::Colours::white.withAlpha(0.5f)); g.drawText("Nothing here yet", la, juce::Justification::centred); }
}

void PresetBrowser::mouseMove(const juce::MouseEvent& e) {
    const auto p = e.getPosition(); int hc = -1, hr = -1; auto ca = catArea();
    for (int i = 0; i < cats.size(); ++i) if (juce::Rectangle<int>(ca.getX() + 14, ca.getY() + i * 42, ca.getWidth() - 14, 38).contains(p)) hc = i;
    auto la = listArea(); if (la.contains(p)) { const int i = int((p.y - la.getY() + scroll) / rowH); if (i >= 0 && i < int(rows.size())) hr = i; }
    if (hc != hoverCat || hr != hoverRow) { hoverCat = hc; hoverRow = hr; }
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e) {
    const auto p = e.getPosition();
    if (!panel().contains(p) || panel().withTrimmedTop(10).removeFromTop(40).removeFromRight(50).contains(p)) { close(); return; }
    auto ca = catArea();
    for (int i = 0; i < cats.size(); ++i) if (juce::Rectangle<int>(ca.getX() + 14, ca.getY() + i * 42, ca.getWidth() - 14, 38).contains(p)) { selCat = i; scroll = 0.f; rebuildRows(); return; }
    auto la = listArea(); if (!la.contains(p)) return;
    const int i = int((p.y - la.getY() + scroll) / rowH); if (i < 0 || i >= int(rows.size())) return;
    const auto row = rows[size_t(i)];
    if (row.isSave) {
        auto* w = new juce::AlertWindow("Save preset", "Name of the new preset:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("name", "My preset"); w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int r) { if (r == 1) { proc.saveUserPreset(w->getTextEditorContents("name")); rebuildRows(); repaint(); } delete w; }), false);
    } else if (row.user.isNotEmpty()) {
        if (p.x > la.getRight() - 46) { proc.userPresetDir().getChildFile(row.user + ".vonoxpreset").deleteFile(); rebuildRows(); }
        else { proc.loadUserPreset(row.user); if (onPicked) onPicked(); }
    } else if (row.program >= 0) { proc.applyPreset(row.program); if (onPicked) onPicked(); }
}

void PresetBrowser::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    const float maxS = juce::jmax(0.f, rows.size() * float(rowH) - listArea().getHeight());
    scroll = juce::jlimit(0.f, maxS, scroll - w.deltaY * 220.f);
}

bool PresetBrowser::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey) {   // swipe through presets of this category
        const auto& ps = factoryPresets(); int idx = -1;
        for (int i = 0; i < int(rows.size()); ++i) if (rows[size_t(i)].program == proc.getCurrentProgram()) idx = i;
        const int nx = juce::jlimit(0, int(rows.size()) - 1, idx + (k == juce::KeyPress::downKey ? 1 : -1));
        if (!rows.empty() && rows[size_t(nx)].program >= 0) { proc.applyPreset(rows[size_t(nx)].program); (void) ps; }
        return true;
    }
    return false;
}
