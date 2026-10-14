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
        { 80, 215, 29, params::lowMidCut, false, "dB", 1 }, { 211, 215, 29, params::presence, true, "dB", 1 },
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

void VonoxEditor::showPresetMenu() {
    juce::PopupMenu menu; const auto& ps = factoryPresets();
    std::map<juce::String, juce::PopupMenu> subs; std::vector<juce::String> order;
    for (int i = 0; i < int(ps.size()); ++i) {
        juce::String cat(ps[size_t(i)].category);
        if (!subs.count(cat)) order.push_back(cat);
        subs[cat].addItem(i + 1, ps[size_t(i)].name, true, i == proc.getCurrentProgram());
    }
    for (auto& c : order) menu.addSubMenu(c, subs[c]);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(styleBox)), [this](int r) { if (r > 0) proc.applyPreset(r - 1); });
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
    repaint(40, 500, 360, 160);
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
        g.drawFittedText(juce::String(factoryPresets()[size_t(proc.getCurrentProgram())].name).toUpperCase(), 446, 232, 208, 18, juce::Justification::centred, 1);
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
    drawMeter(44, 522, 346, 22, mIn); drawMeter(44, 574, 346, 22, mOut); drawMeter(44, 626, 346, 22, mGr);
}

// ---------------------------------------------------------------- ADVANCED panel
void AdvancedPanel::Content::paint(juce::Graphics& g) {
    for (auto& h : heads) {
        g.setColour(juce::Colour(150, 70, 255)); g.setFont(juce::FontOptions(13.f, juce::Font::bold));
        g.drawText(h.first, h.second, juce::Justification::centredLeft);
        g.setColour(juce::Colour(150, 70, 255).withAlpha(0.5f)); g.drawHorizontalLine(h.second.getBottom() - 1, float(h.second.getX()), float(h.second.getRight()));
    }
}

AdvancedPanel::AdvancedPanel(VonoxProcessor& p, const juce::Image& b) : proc(p), bg(b) {
    using namespace params;
    struct G { const char* title; std::vector<int> ids; const char* combo1; const char* combo2; };
    const std::vector<G> groups = {
        { "SUBTRACTIVE EQ + COMPRESSORS", { hpf, lowMidHz, boxCut, harshCut, harshHz, fetThresh, fetRatio, fetAttack, fetRelease, optoThresh, optoRatio, optoAttack, optoRelease, presenceHz }, nullptr, nullptr },
        { "DE-ESSER, RESONANCE, SATURATION, OUTPUT", { deEssHz, deEssThresh, deEssAmount, resAmount, resThresh, satDrive, exciter, parComp, ceiling }, kSatModeId, nullptr },
        { "REVERB", { rvAmount, rvDecay, rvPre, rvDamp, rvHp, rvLp, duck }, kRvModeId, nullptr },
        { "DELAY, DOUBLER, CHORUS", { dlAmount, dlMs, dlFb, dlHp, dlLp, doubler, chorus }, kDlModeId, kDlDivId },
    };
    const int cellW = 100, cellH = 80, perRow = 10; int y = 4;
    auto addCombo = [&](const char* id, int x, int yy) {
        auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id)); if (!par) return;
        auto c = std::make_unique<juce::ComboBox>(); c->addItemList(par->choices, 1); c->setBounds(x + 4, yy + 4, cellW * 2 - 12, 26);
        c->setColour(juce::ComboBox::backgroundColourId, juce::Colour(14, 6, 24)); c->setColour(juce::ComboBox::outlineColourId, juce::Colour(150, 70, 255));
        c->setColour(juce::ComboBox::textColourId, juce::Colours::white); c->setColour(juce::ComboBox::arrowColourId, juce::Colour(255, 205, 225));
        cAtt.push_back(std::make_unique<juce::ComboBoxParameterAttachment>(*par, *c, nullptr));
        content.addAndMakeVisible(*c); combos.push_back(std::move(c));
        auto lab = par->getName(30); (void) lab;
    };
    for (auto& g : groups) {
        content.heads.push_back({ g.title, { 0, y, 1040, 20 } }); y += 24;
        int col = 0;
        if (g.combo1) { addCombo(g.combo1, 0, y); col += 2; }
        if (g.combo2) { addCombo(g.combo2, col * cellW, y); col += 2; }
        for (int id : g.ids) {
            if (col >= perRow) { col = 0; y += cellH; if (g.combo1 && col == 0) {} }
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
