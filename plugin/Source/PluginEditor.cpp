#include "PluginEditor.h"
#include "BinaryData.h"
using namespace vonox;
using theme::blue; using theme::violet; using theme::magenta; using theme::cyan; using theme::lavender;
static constexpr float kPi = juce::MathConstants<float>::pi;

namespace vfont {
static juce::Typeface::Ptr load(const char* name) {
    int sz = 0; const char* d = BinaryData::getNamedResource(name, sz);
    return d ? juce::Typeface::createSystemTypefaceFor(d, size_t(sz)) : nullptr;
}
juce::Font orbitron(float h) { static auto t = load("Orbitron_Bold_sub_ttf"); return t ? juce::Font(juce::FontOptions(t).withHeight(h)) : juce::Font(juce::FontOptions(h, juce::Font::bold)); }
juce::Font rajdhani(float h, bool semi) {
    static auto b = load("Rajdhani_Bold_sub_ttf"); static auto s = load("Rajdhani_SemiBold_sub_ttf"); auto t = semi ? s : b;
    return t ? juce::Font(juce::FontOptions(t).withHeight(h)) : juce::Font(juce::FontOptions(h, semi ? juce::Font::plain : juce::Font::bold));
}
}

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
    if (hover > 0.01f) {   // soft halo around the knob (radial fade, not an outline)
        juce::ColourGradient gr(accent.withAlpha(0.30f * hover), c.x, c.y, accent.withAlpha(0.f), c.x + r + 22.f, c.y, true);
        gr.addColour(float(r) / (r + 22.f), accent.withAlpha(0.f));
        g.setGradientFill(gr); g.fillEllipse(c.x - r - 22.f, c.y - r - 22.f, 2.f * (r + 22.f), 2.f * (r + 22.f));
    }
    juce::Path track; track.addCentredArc(c.x, c.y, rr, rr, 0.f, a0, a0 + 270.f * kPi / 180.f, true);
    g.setColour(juce::Colours::black.withAlpha(0.67f)); g.strokePath(track, juce::PathStrokeType(3.f));
    if (norm > 0.001f) {
        juce::Path val; val.addCentredArc(c.x, c.y, rr, rr, 0.f, a0, a1, true);
        const float k = 1.f + 0.6f * pulse + 0.3f * hover;
        g.setColour(accent.withAlpha(juce::jmin(1.f, 0.22f * k))); g.strokePath(val, juce::PathStrokeType(9.f + 3.f * pulse));
        g.setColour(accent.withAlpha(juce::jmin(1.f, 0.35f * k))); g.strokePath(val, juce::PathStrokeType(6.f));
        g.setColour(accent.brighter(0.5f * pulse)); g.strokePath(val, juce::PathStrokeType(3.f));
        if (pulse > 0.05f) {   // spark at the tip of the arc while the value changes
            const float tx = c.x + std::sin(a1) * rr, ty = c.y - std::cos(a1) * rr;
            g.setColour(juce::Colours::white.withAlpha(pulse * 0.8f)); g.fillEllipse(tx - 2.5f, ty - 2.5f, 5.f, 5.f);
            g.setColour(accent.withAlpha(pulse * 0.4f)); g.fillEllipse(tx - 7.f, ty - 7.f, 14.f, 14.f);
        }
    }
    g.drawImageAt(body, juce::roundToInt(c.x - body.getWidth() * 0.5f), juce::roundToInt(c.y - body.getHeight() * 0.5f));
    const float R = float(r);
    g.setColour(accent.brighter(0.3f * pulse));
    g.drawLine(c.x + std::sin(a1) * R * .22f, c.y - std::cos(a1) * R * .22f, c.x + std::sin(a1) * R * .84f, c.y - std::cos(a1) * R * .84f, 3.f);
    if (isMouseOverOrDragging() || isMouseButtonDown()) {
        auto t = juce::String(getValue(), dec) + " " + unit;
        auto bub = juce::Rectangle<float>(0, 0, 78.f, 20.f).withCentre({ c.x, 10.f });
        g.setColour(juce::Colour(4, 4, 12).withAlpha(0.94f)); g.fillRoundedRectangle(bub, 5.f);
        g.setColour(accent.withAlpha(0.9f)); g.drawRoundedRectangle(bub, 5.f, 1.f);
        g.setColour(juce::Colours::white); g.setFont(vfont::orbitron(11.f));
        g.drawText(t.trim(), bub.toNearestInt(), juce::Justification::centred);
    }
}

void MiniKnob::paint(juce::Graphics& g) {
    const float cx = getWidth() * 0.5f, cy = 21.f, r = 15.f;
    const float norm = float(valueToProportionOfLength(getValue())), a0 = 225.f * 0.0174533f, a1 = a0 + 270.f * 0.0174533f * norm;
    juce::Path tr; tr.addCentredArc(cx, cy, r + 4, r + 4, 0.f, a0, a0 + 270.f * 0.0174533f, true);
    g.setColour(juce::Colour(40, 44, 66)); g.strokePath(tr, juce::PathStrokeType(2.5f));
    if (norm > 0.001f) { juce::Path v; v.addCentredArc(cx, cy, r + 4, r + 4, 0.f, a0, a1, true); g.setColour(accent.withAlpha(0.35f)); g.strokePath(v, juce::PathStrokeType(6.f)); g.setColour(accent); g.strokePath(v, juce::PathStrokeType(2.5f)); }
    g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillEllipse(cx - r, cy - r + 2.f, 2 * r, 2 * r);                       // drop shadow
    { juce::ColourGradient m(juce::Colour(238, 240, 250), cx - 5.f, cy - 6.f, juce::Colour(70, 74, 96), cx + r, cy + r, true);   // metal body
      m.addColour(0.45, juce::Colour(150, 156, 178)); g.setGradientFill(m); g.fillEllipse(cx - r, cy - r, 2 * r, 2 * r); }
    for (int i = 0; i < 30; ++i) {                                                                                               // knurled edge: turns with the knob
        const float a = a1 + i * (2.f * kPi / 30.f), s = std::sin(a), c = std::cos(a);
        g.setColour(juce::Colour(20, 22, 38).withAlpha(0.55f)); g.drawLine(cx + s * (r - 3.2f), cy - c * (r - 3.2f), cx + s * (r - 0.6f), cy - c * (r - 0.6f), 1.2f);
        const float a2 = a + kPi / 30.f; g.setColour(juce::Colours::white.withAlpha(0.35f)); g.drawLine(cx + std::sin(a2) * (r - 3.f), cy - std::cos(a2) * (r - 3.f), cx + std::sin(a2) * (r - 0.8f), cy - std::cos(a2) * (r - 0.8f), 0.8f);
    }
    { const float ri = r - 5.2f; juce::ColourGradient cap(juce::Colour(46, 50, 74), cx + 3.f, cy + 4.f, juce::Colour(150, 156, 182), cx - ri, cy - ri, true);
      g.setGradientFill(cap); g.fillEllipse(cx - ri, cy - ri, 2 * ri, 2 * ri);
      for (int k = 1; k <= 3; ++k) { juce::Path br; br.addCentredArc(cx, cy, ri * k / 3.4f, ri * k / 3.4f, 0.f, 0.3f + k, 2.3f + k, true); g.setColour(juce::Colours::white.withAlpha(0.10f)); g.strokePath(br, juce::PathStrokeType(0.8f)); } }
    g.setColour(accent.brighter(0.3f)); g.drawLine(cx + std::sin(a1) * 2.5f, cy - std::cos(a1) * 2.5f, cx + std::sin(a1) * (r - 3.f), cy - std::cos(a1) * (r - 3.f), 2.4f);
    g.setColour(lavender); g.setFont(vfont::rajdhani(13.f));
    g.drawText(juce::String(getValue(), dec) + (unit.isEmpty() ? "" : " " + unit), 0, 40, getWidth(), 14, juce::Justification::centred);
    g.setColour(juce::Colours::white.withAlpha(0.85f)); g.setFont(vfont::rajdhani(11.5f, true));
    g.drawFittedText(label, 2, 54, getWidth() - 4, 24, juce::Justification::centredTop, 2);
}

VonoxEditor::VonoxEditor(VonoxProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
      bypassBtn("bypass", res("btn_bypass_on_png"), res("btn_bypass_off_png"), magenta), aBtn("a", res("btn_a_on_png"), res("btn_a_off_png")),
      bBtn("b", res("btn_b_on_png"), res("btn_b_off_png")), resetBtn("reset", res("btn_reset_on_png"), res("btn_reset_off_png"), cyan),
      presetsBtn("presets", res("btn_presets_on_png"), res("btn_presets_off_png"), violet), advBtn("adv", res("btn_adv_on_png"), res("btn_adv_off_png"), violet) {
    background = res("background_png"); fireTitle = res("fire_title_jpg"); fireAuto = res("fire_auto_jpg");
    setSize(1100, 700); setResizable(false, false); setOpaque(true);

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
        auto kn = std::make_unique<VonoxKnob>(k.r == 29 ? b29 : b26, k.violet ? violet : blue, k.r, k.unit, k.dec);
        const int half = k.r + 12;
        kn->setBounds(k.cx - half, k.cy - half - VonoxKnob::kTopPad, half * 2, half * 2 + VonoxKnob::kTopPad);
        auto* par = proc.apvts.getParameter(params::kFloats[k.param].id);
        kn->setNormalisableRange(juce::NormalisableRange<double>(par->getNormalisableRange().start, par->getNormalisableRange().end, 0.01, par->getNormalisableRange().skew));
        kn->setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
        knobAtt.push_back(std::make_unique<juce::SliderParameterAttachment>(*par, *kn, nullptr));
        if (k.param == params::mix) mixKnob = kn.get();
        addAndMakeVisible(*kn); knobs.push_back(std::move(kn));
    }
    // sprites carry an 8 px transparent margin around the button
    auto place = [this](SpriteButton& c, int x, int y) { c.setBounds(x - 8, y - 8, c.imgOff.getWidth(), c.imgOff.getHeight()); addAndMakeVisible(c); };
    place(bypassBtn, 905, 512); place(aBtn, 905, 556); place(bBtn, 985, 556); place(resetBtn, 905, 600); place(presetsBtn, 985, 600); place(advBtn, 480, 578);
    browser = std::make_unique<PresetBrowser>(proc); browser->setBounds(getLocalBounds()); addChildComponent(*browser);
    adv = std::make_unique<AdvancedPanel>(proc, background); adv->setBounds(18, 118, 1064, 304); addAndMakeVisible(*adv); adv->setVisible(false);
    advBtn.onClick = [this] { adv->setVisible(!adv->isVisible()); advBtn.setToggleState(adv->isVisible(), juce::dontSendNotification); advBtn.toFront(false); };
    advBtn.setTooltip("Advanced: every module parameter (EQ, compressors, de-esser, saturation, reverb, delay, doubler, chorus)");
    autoBtn.setBounds(428, 428, 244, 74); addAndMakeVisible(autoBtn);
    autoBtn.onClick = [this] { proc.runAutoMix(); burst(550.f, 466.f); };
    bypassBtn.setClickingTogglesState(true);        // ButtonParameterAttachment does not do this by itself (this was the "Bypass does nothing" bug)
    bypassAtt = std::make_unique<juce::ButtonParameterAttachment>(*proc.apvts.getParameter(params::kBypassId), bypassBtn);
    aBtn.onClick = [this] { proc.switchAB(0); };
    bBtn.onClick = [this] { proc.switchAB(1); };
    resetBtn.onClick = [this] { proc.resetToDefault(); statusLine = "All parameters reset to Default."; };
    presetsBtn.onClick = [this] { browser->open(); };
    aBtn.setTooltip("Slot A"); bBtn.setTooltip("Slot B: the first time you open it, it starts as a copy of A");
    bypassBtn.setTooltip("Bypass: passes the audio through untouched (delay-matched, 10 ms crossfade)");
    resetBtn.setTooltip("Reset every parameter to the Default preset"); presetsBtn.setTooltip("Preset browser: Default and your own saved presets");
    autoBtn.setTooltip("Analyses the audio captured while playing and sets the whole chain");
    aBtn.setToggleState(proc.activeSlot() == 0, juce::dontSendNotification); bBtn.setToggleState(proc.activeSlot() == 1, juce::dontSendNotification);
    statusLine = proc.analysisText();
    stars.init(150, 1100.f, 700.f, 0.22f);
    for (int i = 0; i < 8; ++i) { const float a = (22.5f + 45.f * i) * kPi / 180.f; juce::Point<float> pt(550.f + 108.f * std::cos(a), 262.f + 108.f * std::sin(a)); if (i == 0) glassPath.startNewSubPath(pt); else glassPath.lineTo(pt); }
    glassPath.closeSubPath();
    for (int i = 0; i < 80; ++i) { const float a = rng.nextFloat() * 6.28f, rd = std::sqrt(rng.nextFloat()) * 100.f;
        voiceStars.push_back({ 550.f + std::cos(a) * rd, 262.f + std::sin(a) * rd, 0.f, 0.f, 0.9f + rng.nextFloat() * 1.6f, rng.nextFloat() * 6.28f, 0.4f + rng.nextFloat() * 0.6f }); }
    startTimerHz(30);
}
VonoxEditor::~VonoxEditor() { stopTimer(); }

void VonoxEditor::burst(float x, float y) {
    for (int i = 0; i < 44; ++i) { const float a = rng.nextFloat() * 6.28f, s = 1.5f + rng.nextFloat() * 4.5f; bursts.push_back({ x, y, std::cos(a) * s, std::sin(a) * s * 0.8f - 0.8f, 1.f, 1.f + rng.nextFloat() * 2.2f }); }
}

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
    frameF += 0.8f; if (frameF >= 72.f) frameF -= 72.f; frame = int(frameF);       // fire loops are 72 frames at 24 fps
    auto& m = proc.chain.meters;
    auto lvl = [](float db) { return juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f); };
    auto smooth = [](float& cur, float t) { cur = t > cur ? t : cur + (t - cur) * 0.18f; };
    smooth(mIn, lvl(m.inPeakDb.load())); smooth(mOut, lvl(m.outPeakDb.load()));
    {   // voice intensity (RMS of what you hear, 0..1): fast attack, slower release
        const float rms = juce::jmax(m.inRmsDb.load(), m.outRmsDb.load());
        const float t = juce::jlimit(0.f, 1.f, (rms + 52.f) / 38.f);
        level = t > level ? level + (t - level) * 0.6f : level + (t - level) * 0.12f;
    }
    if (proc.autoMixCounter() != lastAutoCount) { lastAutoCount = proc.autoMixCounter(); statusLine = proc.analysisText(); autoBtn.setTooltip(statusLine); }
    autoBtn.busyFlag = proc.analysisBusy();
    aBtn.setToggleState(proc.activeSlot() == 0, juce::dontSendNotification); bBtn.setToggleState(proc.activeSlot() == 1, juce::dontSendNotification);
    bypassPulse += ((bypassBtn.getToggleState() ? 1.f : 0.f) - bypassPulse) * 0.2f;
    stars.tick();
    // stars inside the octagon: calm drift when quiet, agitated and brighter with the voice
    for (auto& q : voiceStars) {
        q.ph += 0.06f + level * 0.25f;
        const float k = 0.04f + level * 1.2f;
        q.vx += (rng.nextFloat() - .5f) * k; q.vy += (rng.nextFloat() - .5f) * k;
        q.vx += (550.f - q.x) * 0.0012f; q.vy += (262.f - q.y) * 0.0012f;
        q.vx *= 0.9f; q.vy *= 0.9f; q.x += q.vx; q.y += q.vy;
        if (!glassPath.contains(q.x, q.y)) { const float a = rng.nextFloat() * 6.28f, rd = std::sqrt(rng.nextFloat()) * 95.f; q.x = 550.f + std::cos(a) * rd; q.y = 262.f + std::sin(a) * rd; q.vx = q.vy = 0.f; }
    }
    for (auto& q : bursts) { q.x += q.vx; q.y += q.vy; q.vx *= 0.95f; q.vy = q.vy * 0.95f + 0.03f; q.life -= 0.028f; }
    bursts.erase(std::remove_if(bursts.begin(), bursts.end(), [](const B& b) { return b.life <= 0.f; }), bursts.end());
    for (auto* b : { &bypassBtn, &aBtn, &bBtn, &resetBtn, &presetsBtn, &advBtn }) b->tick();
    autoBtn.tick();
    for (auto& k : knobs) k->tick();
    if (adv->isVisible()) adv->tick();
    repaint();
}

void VonoxEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
    stars.paint(g);
    g.setOpacity(1.f);                    // JUCE applies the last colour's alpha to drawImage: reset it or the artwork turns translucent
    g.drawImageAt(background, 0, 0);
    // animated fire strips (frames pre-rendered by tools/export_assets.py)
    const int th = 88, ah = 82;
    g.drawImage(fireTitle, 235, 14, 630, th, 0, frame * th, 630, th);
    g.drawImage(fireAuto, 424, 424, 252, ah, 0, frame * ah, 252, ah);

    // octagon glass: voice-reactive stars
    {
        juce::Path glass; for (int i = 0; i < 8; ++i) { const float a = (22.5f + 45.f * i) * kPi / 180.f; juce::Point<float> pt(550.f + 118.f * std::cos(a), 262.f + 118.f * std::sin(a)); if (i == 0) glass.startNewSubPath(pt); else glass.lineTo(pt); }
        glass.closeSubPath();
        juce::Graphics::ScopedSaveState ss(g); g.reduceClipRegion(glass);
        juce::ColourGradient gl(violet.withAlpha(0.04f + 0.30f * level), 550.f, 262.f, violet.withAlpha(0.f), 550.f + 118.f, 262.f, true);
        g.setGradientFill(gl); g.fillRect(430, 140, 240, 240);
        for (const auto& q : voiceStars) {
            const float tw = 0.5f + 0.5f * std::sin(q.ph), sz = q.size * (1.f + level * 0.9f), a = q.a * (0.3f + 0.4f * tw + 0.5f * level);
            const juce::Colour c = juce::Colours::white.interpolatedWith(cyan, juce::jlimit(0.f, 1.f, level * 1.2f));
            g.setColour(c.withAlpha(juce::jmin(1.f, a) * 0.2f)); g.fillEllipse(q.x - sz * 2.2f, q.y - sz * 2.2f, sz * 4.4f, sz * 4.4f);
            g.setColour(c.withAlpha(juce::jmin(1.f, a))); g.fillEllipse(q.x - sz * .5f, q.y - sz * .5f, sz, sz);
        }
    }
    g.setColour(lavender); g.setFont(vfont::orbitron(10.5f));
    g.drawText("VOCAL PROFILE", 450, 173, 200, 16, juce::Justification::centred);
    g.setColour(juce::Colour(4, 4, 14).withAlpha(0.9f)); g.fillRoundedRectangle(styleBox.toFloat(), 5.f);
    g.setColour(violet.withAlpha(0.8f)); g.drawRoundedRectangle(styleBox.toFloat(), 5.f, 1.2f);
    const int si = juce::roundToInt(proc.apvts.getRawParameterValue(params::kStyleId)->load());
    g.setColour(juce::Colours::white); g.setFont(vfont::rajdhani(17.f));
    g.drawFittedText(juce::String(engine::styleName(static_cast<engine::Style>(juce::jlimit(0, 8, si)))).toUpperCase(), styleBox.reduced(26, 0), juce::Justification::centred, 1);
    g.setColour(cyan); g.drawText(juce::CharPointer_UTF8("\xe2\x97\x80"), leftArrow, juce::Justification::centred); g.drawText(juce::CharPointer_UTF8("\xe2\x96\xb6"), rightArrow, juce::Justification::centred);
    if (proc.presetActive.load() && proc.getCurrentProgram() >= 0 && proc.getCurrentProgram() < int(factoryPresets().size())) {
        g.setColour(cyan.withAlpha(0.85f)); g.setFont(vfont::rajdhani(12.f));
        g.drawFittedText(juce::String(displayName(factoryPresets()[size_t(proc.getCurrentProgram())])).toUpperCase(), 466, 230, 168, 14, juce::Justification::centred, 1);
    }
    g.setColour(lavender); g.setFont(vfont::orbitron(10.5f));
    g.drawText("MIX AMOUNT", 450, 254, 200, 16, juce::Justification::centred);
    const juce::String pct = juce::String(juce::roundToInt(mixKnob->getValue())) + "%";
    g.setFont(vfont::orbitron(34.f));
    g.setColour(violet.withAlpha(0.45f + 0.3f * level)); g.drawText(pct, 446, 274, 208, 46, juce::Justification::centred);
    g.setColour(juce::Colour(240, 236, 255)); g.drawText(pct, 450, 274, 200, 46, juce::Justification::centred);
    g.setFont(vfont::rajdhani(12.f, true)); g.setColour(lavender.withAlpha(0.9f));
    g.drawFittedText(statusLine.upToFirstOccurrenceOf("\n", false, false), 490, 322, 120, 38, juce::Justification::centredTop, 3, 0.8f);

    // meters (same geometry as the artwork's empty frames)
    auto drawMeter = [&](int x, int y, int w, int h, float lv) {
        const int segs = 26; const float sw = (w - 8) / float(segs);
        for (int i = 0; i < segs; ++i) {
            const bool on = (i / float(segs)) < lv;
            const juce::Colour col = i < segs * 0.6 ? violet : (i < segs * 0.85 ? cyan : magenta);
            juce::Rectangle<float> r(x + 4 + i * sw, float(y + 4), sw - 2, float(h - 8));
            if (on) { g.setColour(col.withAlpha(0.25f)); g.fillRect(r.expanded(2)); }
            g.setColour(on ? col : juce::Colour(26, 24, 44)); g.fillRect(r);
        }
    };
    drawMeter(44, 522, 346, 22, mIn); drawMeter(44, 574, 346, 22, mOut);
    {   // detected key in the VOICE panel
        g.setFont(vfont::orbitron(10.5f)); g.setColour(lavender);
        const auto kt = proc.keyText(); g.drawText("KEY  " + (kt.isEmpty() ? juce::String("--") : kt), 30, 427, 230, 16, juce::Justification::centred);
    }
    {   // LATENCY / TRUE PEAK plates (recessed plates are in the artwork at y 614..656)
        auto plate = [&](int x, const juce::String& val, juce::Colour col) {   // the plate captions (LATENCY / TRUE PEAK) are part of the artwork
            g.setColour(col.withAlpha(0.3f)); g.setFont(vfont::orbitron(15.f)); g.drawText(val, x + 1, 633, 163, 20, juce::Justification::centred);
            g.setColour(col); g.drawText(val, x, 632, 163, 20, juce::Justification::centred);
        };
        plate(44, juce::String(proc.getLatencySamples() * 1000.0 / juce::jmax(1.0, proc.getSampleRate()), 1) + " ms", cyan);
        const float tp = proc.chain.meters.truePeakDb.load();
        plate(227, (tp < -90.f ? juce::String("--") : juce::String(tp, 1)) + " dBTP", tp > -0.5f ? magenta : juce::Colour(235, 230, 255));
    }
}

void VonoxEditor::paintOverChildren(juce::Graphics& g) {
    if (bypassPulse > 0.01f && !browser->isVisible()) {
        juce::Graphics::ScopedSaveState ss(g); g.excludeClipRegion(bypassBtn.getBounds());
        g.setColour(juce::Colours::black.withAlpha(0.42f * bypassPulse)); g.fillRect(getLocalBounds());
    }
    if (bypassPulse > 0.01f && !browser->isVisible()) {
        const float pulse = 0.65f + 0.35f * std::sin(juce::Time::getMillisecondCounter() * 0.006f);
        juce::Path glass; for (int i = 0; i < 8; ++i) { const float a = (22.5f + 45.f * i) * kPi / 180.f; juce::Point<float> pt(550.f + 116.f * std::cos(a), 262.f + 116.f * std::sin(a)); if (i == 0) glass.startNewSubPath(pt); else glass.lineTo(pt); }
        glass.closeSubPath();
        g.setColour(juce::Colour(8, 2, 16).withAlpha(0.82f * bypassPulse)); g.fillPath(glass);
        g.setColour(magenta.withAlpha(pulse * bypassPulse)); g.setFont(vfont::orbitron(21.f)); g.drawText("BYPASSED", 440, 236, 220, 30, juce::Justification::centred);
        g.setColour(lavender.withAlpha(0.85f * bypassPulse)); g.setFont(vfont::rajdhani(13.f, true));
        g.drawFittedText("VONOX is off.\nThe audio passes through untouched.", 470, 272, 160, 40, juce::Justification::centredTop, 2);
    }
    for (const auto& q : bursts) {
        const float a = juce::jlimit(0.f, 1.f, q.life);
        g.setColour(cyan.withAlpha(0.22f * a)); g.fillEllipse(q.x - q.size * 2.f, q.y - q.size * 2.f, q.size * 4.f, q.size * 4.f);
        g.setColour(juce::Colours::white.withAlpha(0.95f * a)); g.fillEllipse(q.x - q.size * .5f, q.y - q.size * .5f, q.size, q.size);
    }
}

// ---------------------------------------------------------------- ADVANCED panel
void AdvancedPanel::Content::paint(juce::Graphics& g) {
    for (auto& l : labels) { g.setColour(lavender.withAlpha(0.9f)); g.setFont(vfont::rajdhani(12.5f, true)); g.drawText(l.first, l.second, juce::Justification::centredLeft); }
    for (auto& h : heads) {
        g.setColour(cyan); g.setFont(vfont::orbitron(11.5f));
        g.drawText(h.first, h.second, juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.28f)); g.drawHorizontalLine(h.second.getBottom() - 1, float(h.second.getX()), float(h.second.getRight()));
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
    stars.init(110, 1064, 304, 0.3f);
    const int cellW = 100, cellH = 80, perRow = 10; int y = 4;
    auto styleCombo = [](juce::ComboBox& c) {
        c.setColour(juce::ComboBox::backgroundColourId, juce::Colour(6, 6, 14)); c.setColour(juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha(0.45f));
        c.setColour(juce::ComboBox::textColourId, juce::Colours::white); c.setColour(juce::ComboBox::arrowColourId, theme::lavender);
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
            t->setColour(juce::ToggleButton::textColourId, juce::Colours::white); t->setColour(juce::ToggleButton::tickColourId, theme::cyan); t->setColour(juce::ToggleButton::tickDisabledColourId, theme::cyan);
            tAtt.push_back(std::make_unique<juce::ButtonParameterAttachment>(*par, *t)); content.addAndMakeVisible(*t); toggles.push_back(std::move(t)); col += 2;
        }
        if (g.refButtons) {
            auto mk = [&](const char* txt, int x, std::function<void()> fn, const char* tip) {
                auto bt = std::make_unique<juce::TextButton>(txt); bt->setBounds(x + 4, y + 18, cellW * 2 - 12, 26); bt->onClick = std::move(fn); bt->setTooltip(tip);
                bt->setColour(juce::TextButton::buttonColourId, juce::Colour(6, 6, 14)); bt->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
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
            auto k = std::make_unique<MiniKnob>(d.name, d.unit, d.hi - d.lo > 100 ? 0 : (d.hi - d.lo > 10 ? 1 : 2), theme::blue);
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
    vp.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, theme::violet);
}

void AdvancedPanel::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black); g.fillRoundedRectangle(r, 10.f);
    { juce::Graphics::ScopedSaveState ss(g); juce::Path clip; clip.addRoundedRectangle(r, 10.f); g.reduceClipRegion(clip); stars.paint(g, 0.9f); }
    g.setColour(juce::Colours::white.withAlpha(0.55f)); g.drawRoundedRectangle(r.reduced(0.6f), 10.f, 1.2f);
    g.setColour(juce::Colours::white); g.setFont(vfont::orbitron(14.f));
    g.drawText("ADVANCED", 18, 8, 200, 22, juce::Justification::centredLeft);
    g.setColour(lavender.withAlpha(0.7f)); g.setFont(vfont::rajdhani(13.f, true));
    g.drawText("every module parameter, all automatable", 150, 8, getWidth() - 170, 22, juce::Justification::centredLeft);
}

// ---------------------------------------------------------------- PRESET BROWSER (black, white text, white particles)
PresetBrowser::PresetBrowser(VonoxProcessor& p) : proc(p) {
    setWantsKeyboardFocus(true); setInterceptsMouseClicks(true, true);
    stars.init(130, 1100.f, 700.f, 0.35f);
}

void PresetBrowser::refresh() {
    cats.clear();
    for (const auto& pr : factoryPresets()) if (!cats.contains(pr.category)) cats.add(pr.category);
    cats.add("My Presets");
    selCat = proc.presetActive.load() ? 0 : juce::jmin(1, cats.size() - 1);
    scroll = 0.f; rebuildRows();
}

void PresetBrowser::rebuildRows() {
    rows.clear(); const auto& ps = factoryPresets();
    if (cats[selCat] == "My Presets") {
        rows.push_back({ "+  Save current sound as preset...", "stored in your user folder, available in every project", -1, {}, true });
        for (auto& n : proc.userPresetNames()) rows.push_back({ n, "your preset", -1, n, false });
    } else for (int i = 0; i < int(ps.size()); ++i) if (cats[selCat] == ps[size_t(i)].category)
        rows.push_back({ displayName(ps[size_t(i)]), "the factory sound - the RESET button restores it", i, {}, false });
    hoverRow = -1;
}

void PresetBrowser::timerCallback() { stars.tick(); repaint(); }

void PresetBrowser::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.62f));                              // dims the plugin behind
    auto pn = panel().toFloat();
    g.setColour(juce::Colours::black); g.fillRoundedRectangle(pn, 10.f);           // pure black panel
    { juce::Graphics::ScopedSaveState ss(g); juce::Path clip; clip.addRoundedRectangle(pn, 10.f); g.reduceClipRegion(clip); stars.paint(g, 0.9f); }
    g.setColour(juce::Colours::white.withAlpha(0.55f)); g.drawRoundedRectangle(pn.reduced(0.5f), 10.f, 1.2f);
    g.setColour(juce::Colours::white); g.setFont(vfont::orbitron(20.f));
    g.drawText("PRESETS", panel().withTrimmedLeft(24).withHeight(60), juce::Justification::centredLeft);
    g.setColour(juce::Colours::white); g.setFont(vfont::rajdhani(24.f));
    g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x95"), panel().withTrimmedTop(10).removeFromTop(40).removeFromRight(50), juce::Justification::centred);
    g.setColour(juce::Colours::white.withAlpha(0.25f)); g.drawHorizontalLine(panel().getY() + 64, pn.getX() + 16, pn.getRight() - 16);
    auto ca = catArea(); g.setFont(vfont::rajdhani(19.f));
    for (int i = 0; i < cats.size(); ++i) {
        auto r = juce::Rectangle<int>(ca.getX() + 14, ca.getY() + i * 42, ca.getWidth() - 14, 38);
        if (i == selCat) { g.setColour(juce::Colours::white.withAlpha(0.14f)); g.fillRoundedRectangle(r.toFloat(), 6.f); g.setColour(juce::Colours::white); g.fillRect(r.getX() - 8, r.getY() + 6, 3, r.getHeight() - 12); }
        else if (i == hoverCat) { g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRoundedRectangle(r.toFloat(), 6.f); }
        g.setColour(juce::Colours::white.withAlpha(i == selCat ? 1.f : 0.75f)); g.drawText(cats[i], r.withTrimmedLeft(10), juce::Justification::centredLeft);
    }
    auto la = listArea(); juce::Graphics::ScopedSaveState ss(g); g.reduceClipRegion(la);
    const int cp = proc.getCurrentProgram(); const bool active = proc.presetActive.load();
    for (int i = 0; i < int(rows.size()); ++i) {
        auto r = juce::Rectangle<int>(la.getX(), la.getY() + int(i * rowH - scroll), la.getWidth() - 6, rowH - 4);
        if (r.getBottom() < la.getY() || r.getY() > la.getBottom()) continue;
        const bool cur = active && rows[size_t(i)].program == cp && rows[size_t(i)].program >= 0;
        if (i == hoverRow) { g.setColour(juce::Colours::white.withAlpha(0.10f)); g.fillRoundedRectangle(r.toFloat(), 6.f); }
        if (cur) { g.setColour(juce::Colours::white.withAlpha(0.9f)); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 6.f, 1.4f); }
        g.setColour(juce::Colours::white); g.setFont(vfont::rajdhani(21.f));
        g.drawText(rows[size_t(i)].title, r.withTrimmedLeft(14).withTrimmedRight(rows[size_t(i)].user.isNotEmpty() ? 40 : 0), rows[size_t(i)].sub.isEmpty() ? juce::Justification::centredLeft : juce::Justification::topLeft);
        if (rows[size_t(i)].sub.isNotEmpty()) { g.setColour(juce::Colours::white.withAlpha(0.5f)); g.setFont(vfont::rajdhani(13.f, true)); g.drawText(rows[size_t(i)].sub, r.withTrimmedLeft(14).withTrimmedTop(25), juce::Justification::topLeft); }
        if (rows[size_t(i)].user.isNotEmpty()) { g.setColour(juce::Colours::white.withAlpha(0.6f)); g.setFont(vfont::rajdhani(17.f)); g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x95"), r.removeFromRight(36), juce::Justification::centred); }
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
